#include <Helix/UI/Engine/Implementation/GuiState.h>
#include <Helix/UI/Engine/Implementation/Overlay.h>

#include <Helix/Engine/Time/Time.h>

#include <cmath>

// GuiStyle's drawing and measuring: the state's background 9-sliced over the overflow rect, the content (image and
// text) placed inside the padding by the alignment and the image position.
namespace helix::ui::gui
{
    using namespace detail;

    namespace
    {
        constexpr f32 kDefaultFontSize = 13;

        i32 Column(TextAnchor a) { return i32(a) % 3; }
        i32 Row(TextAnchor a) { return i32(a) / 3; }

        // Where content of a size sits in a rect by the alignment.
        Vector2 Place(const Rect& r, Vector2 size, TextAnchor a)
        {
            f32 x = Column(a) == 0 ? r.x : Column(a) == 1 ? r.x + (r.width - size.x) * 0.5f : r.x + r.width - size.x;
            f32 y = Row(a) == 0 ? r.y : Row(a) == 1 ? r.y + (r.height - size.y) * 0.5f : r.y + r.height - size.y;
            return Vector2(std::round(x), std::round(y));
        }

        // The image's size in the content rect: its own, made smaller to fit (keeping its aspect).
        Vector2 ImageSize(engine::Texture2D* image, Vector2 room)
        {
            if (!image)
                return Vector2(0, 0);
            f32 w = f32(image->width()), h = f32(image->height());
            f32 k = 1;
            if (room.x > 0 && w > room.x)
                k = room.x / w;
            if (room.y > 0 && h * k > room.y)
                k = room.y / h;
            return Vector2(w * k, h * k);
        }

        // The parts of a content in a style's rect: the image's rect and the text's.
        struct ContentLayout
        {
            Rect image, text;
            bool hasImage = false, hasText = false;
        };

        ContentLayout LayOut(const GuiStyle& style, const Rect& position, const GuiContent& content)
        {
            ContentLayout out;
            Rect inner = style.padding()->Remove(position);
            inner.x += style.contentOffset().x;
            inner.y += style.contentOffset().y;
            engine::Texture2D* image = style.imagePosition() == ImagePosition::TextOnly ? nullptr : content.image();
            bool text = style.imagePosition() != ImagePosition::ImageOnly && !content.text().Empty();
            out.hasImage = image != nullptr;
            out.hasText = text;
            if (!image)
            {
                out.text = inner;
                return out;
            }
            Vector2 isize = ImageSize(image, Vector2(inner.width, inner.height));
            if (!text)
            {
                Vector2 at = Place(inner, isize, style.alignment());
                out.image = Rect(at.x, at.y, isize.x, isize.y);
                return out;
            }
            if (style.imagePosition() == ImagePosition::ImageAbove)
            {
                Vector2 at = Place(Rect(inner.x, inner.y, inner.width, isize.y), isize, TextAnchor(Column(style.alignment())));
                out.image = Rect(at.x, inner.y, isize.x, isize.y);
                out.text = Rect(inner.x, inner.y + isize.y, inner.width, inner.height - isize.y);
                return out;
            }
            // Image on the left of the text: the two placed as one by the alignment.
            Vector2 tsize = S().generator.PreferredSize(content.text().View(), TextSettings(style, -1, -1, Color(1, 1, 1, 1)));
            f32 total = isize.x + tsize.x;
            Vector2 at = Place(inner, Vector2(total, isize.y > tsize.y ? isize.y : tsize.y), style.alignment());
            f32 iy = Place(inner, isize, style.alignment()).y;
            out.image = Rect(at.x, iy, isize.x, isize.y);
            f32 tx = at.x + isize.x;
            out.text = Rect(tx, inner.y, inner.x + inner.width - tx, inner.height);
            return out;
        }

        // The state drawn: on states when on; the focused, hover and active ones when they have a background.
        const GuiStyleState& StateOf(const GuiStyle& s, bool isHover, bool isActive, bool on, bool focus)
        {
            GuiStyleState* states[4] = {on ? s.onNormal() : s.normal(), on ? s.onHover() : s.hover(), on ? s.onActive() : s.active(),
                                        on ? s.onFocused() : s.focused()};
            const GuiStyleState* state = states[0];
            if (focus && states[3]->background())
                state = states[3];
            if (isHover && states[1]->background())
                state = states[1];
            if (isActive && isHover && states[2]->background())
                state = states[2];
            return *state;
        }

        void DrawBackground(const GuiStyle& s, const Rect& position, const GuiStyleState& state)
        {
            if (engine::Texture2D* bg = state.background())
                DrawImage(s.overflow()->Add(position), bg, s.border(), BackgroundTint());
        }

        // The text's layout in its rect (y down from the rect's top left).
        void TextIn(const GuiStyle& style, const Rect& r, StringView text, Color color, TextInfo& info)
        {
            // No text lays out nothing (an empty field's cursor sits where its first line starts).
            if (text.Empty())
            {
                info.Clear();
                return;
            }
            TextGenerationSettings settings = TextSettings(style, r.width, r.height, color);
            S().generator.Generate(text, settings, info);
        }

        void DrawContent(const GuiStyle& s, const Rect& position, const GuiContent& content, const GuiStyleState& state, StringView text,
                         i32 cursor = -1, i32 selectFrom = -1, i32 selectTo = -1, Vector2 scroll = Vector2(0, 0))
        {
            ContentLayout l = LayOut(s, position, content);
            if (l.hasImage)
                DrawTextureRect(l.image, content.image(), Rect(0, 0, 1, 1), ContentTint(Color(1, 1, 1, 1)));
            bool clip = s.clipping() != TextClipping::Overflow;
            UIClip saved = S().mesh.Clip();
            if (clip)
            {
                // The text cut to the content rect (inside the padding) in GUI space.
                Rect inner = s.padding()->Remove(position);
                Vector2 o = TopClip().origin;
                Rect gui(inner.x + o.x, inner.y + o.y, inner.width, inner.height);
                Rect v = TopClip().visible;
                f32 x0 = gui.x > v.x ? gui.x : v.x, y0 = gui.y > v.y ? gui.y : v.y;
                f32 x1 = gui.x + gui.width < v.x + v.width ? gui.x + gui.width : v.x + v.width;
                f32 y1 = gui.y + gui.height < v.y + v.height ? gui.y + gui.height : v.y + v.height;
                UIClip c = saved;
                c.rect = Rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
                S().mesh.SetClip(c);
            }
            if (l.hasText || cursor >= 0)
            {
                TextInfo info;
                Rect tr = l.text;
                TextIn(s, Rect(tr.x - scroll.x, tr.y - scroll.y, tr.width, tr.height), text, ContentTint(state.textColor()), info);
                Vector2 at(tr.x - scroll.x, tr.y - scroll.y);
                // The selection behind the glyphs, a rect per line.
                if (selectFrom >= 0 && selectFrom != selectTo)
                {
                    u32 a = ByteOfUnit(text, selectFrom < selectTo ? selectFrom : selectTo), b = ByteOfUnit(text, selectFrom < selectTo ? selectTo : selectFrom);
                    Color sel = Skin().settings()->selectionColor();
                    for (const CharacterInfo& c : info.characterInfo)
                        if (c.index >= a && c.index < b)
                            DrawRectangle(Rect(at.x + c.origin, at.y + c.ascender, c.xAdvance - c.origin, c.descender - c.ascender), sel);
                }
                DrawText(info, at);
                if (cursor >= 0)
                {
                    Vector2 p = s.GetCursorPixelPosition(position, const_cast<GuiContent*>(&content), cursor);
                    p.x -= scroll.x, p.y -= scroll.y;
                    f32 h = s.lineHeight();
                    DrawRectangle(Rect(std::round(p.x), p.y, 1, h), ContentTint(Skin().settings()->cursorColor()));
                }
            }
            if (clip)
                S().mesh.SetClip(saved);
        }
    }

    // ------------------------------------------------------------------------------------------------- fonts and text

    f32 GuiStyle::EffectiveFontSize() const { return fontSize_ > 0 ? f32(fontSize_) : kDefaultFontSize; }

    f32 GuiStyle::lineHeight() const
    {
        TextGenerationSettings settings = TextSettings(*this, -1, -1, Color(1, 1, 1, 1));
        if (!settings.font)
            return EffectiveFontSize();
        TextInfo info;
        S().generator.Generate("X", settings, info);
        return info.lineInfo.Empty() ? EffectiveFontSize() : info.lineInfo[0].lineHeight;
    }

    // ------------------------------------------------------------------------------------------------- drawing

    void GuiStyle::DrawState(Rect position, const GuiContent& content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
    {
        if (!Repainting())
            return;
        const GuiStyleState& state = StateOf(*this, isHover, isActive, on, hasKeyboardFocus);
        DrawBackground(*this, position, state);
        DrawContent(*this, position, content, state, content.text().View());
    }

    void GuiStyle::Draw(Rect position, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
    {
        DrawState(position, *GuiContent::none(), isHover, isActive, on, hasKeyboardFocus);
    }

    void GuiStyle::Draw(Rect position, StringView text, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
    {
        DrawState(position, GuiContent::Temp(text), isHover, isActive, on, hasKeyboardFocus);
    }

    void GuiStyle::Draw(Rect position, engine::Texture2D* image, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
    {
        DrawState(position, GuiContent::Temp(StringView(), image), isHover, isActive, on, hasKeyboardFocus);
    }

    void GuiStyle::Draw(Rect position, GuiContent* content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const
    {
        DrawState(position, content ? *content : *GuiContent::none(), isHover, isActive, on, hasKeyboardFocus);
    }

    void GuiStyle::Draw(Rect position, GuiContent* content, i32 controlID) const { Draw(position, content, controlID, false); }

    void GuiStyle::Draw(Rect position, GuiContent* content, i32 controlID, bool on) const
    {
        Draw(position, content, controlID, on, Contains(position, Mouse()));
    }

    void GuiStyle::Draw(Rect position, GuiContent* content, i32 controlID, bool on, bool hover) const
    {
        State& s = S();
        bool hot = s.hotControl == controlID;
        DrawState(position, content ? *content : *GuiContent::none(), hover && (hot || s.hotControl == 0) && Hover(position, controlID), hot, on,
                  controlID != 0 && s.keyboardControl == controlID);
    }

    void GuiStyle::DrawCursor(Rect position, GuiContent* content, i32 controlID, i32 character) const
    {
        DrawWithTextSelection(position, content, controlID, character, character);
    }

    void GuiStyle::DrawWithTextSelection(Rect position, GuiContent* content, i32 controlID, i32 firstSelectedCharacter, i32 lastSelectedCharacter) const
    {
        if (!Repainting())
            return;
        State& s = S();
        const GuiContent& c = content ? *content : *GuiContent::none();
        bool focus = controlID != 0 && s.keyboardControl == controlID;
        const GuiStyleState& state = StateOf(*this, Hover(position, controlID), s.hotControl == controlID, false, focus);
        DrawBackground(*this, position, state);
        Vector2 scroll = s.editor && s.editor->controlID() == controlID ? s.editor->scrollOffset() : Vector2(0, 0);
        bool flash = true;
        f32 speed = Skin().settings()->cursorFlashSpeed();
        if (speed > 0)
            flash = std::fmod(f32(engine::Time::realtimeSinceStartup()) * speed, 1.0f) < 0.5f;
        DrawContent(*this, position, c, state, c.text().View(), focus && flash ? lastSelectedCharacter : -1, firstSelectedCharacter, lastSelectedCharacter, scroll);
    }

    // ------------------------------------------------------------------------------------------------- measuring

    Vector2 GuiStyle::CalcSize(GuiContent* content) const
    {
        const GuiContent& c = content ? *content : *GuiContent::none();
        engine::Texture2D* image = imagePosition_ == ImagePosition::TextOnly ? nullptr : c.image();
        bool text = imagePosition_ != ImagePosition::ImageOnly && !c.text().Empty();
        Vector2 t(0, 0), i(0, 0);
        if (text)
        {
            t = S().generator.PreferredSize(c.text().View(), TextSettings(*this, -1, -1, Color(1, 1, 1, 1)));
            t = Vector2(std::ceil(t.x), std::ceil(t.y));
        }
        else if (!image && imagePosition_ != ImagePosition::ImageOnly)
            t.y = std::ceil(lineHeight());
        if (image)
            i = Vector2(f32(image->width()), f32(image->height()));
        Vector2 size = imagePosition_ == ImagePosition::ImageAbove ? Vector2(t.x > i.x ? t.x : i.x, t.y + i.y) : Vector2(t.x + i.x, t.y > i.y ? t.y : i.y);
        size.x += f32(padding()->horizontal());
        size.y += f32(padding()->vertical());
        if (fixedWidth_ != 0)
            size.x = fixedWidth_;
        if (fixedHeight_ != 0)
            size.y = fixedHeight_;
        return size;
    }

    Vector2 GuiStyle::CalcScreenSize(Vector2 contentSize) const
    {
        return Vector2(fixedWidth_ != 0 ? fixedWidth_ : std::ceil(contentSize.x + f32(padding()->horizontal())),
                       fixedHeight_ != 0 ? fixedHeight_ : std::ceil(contentSize.y + f32(padding()->vertical())));
    }

    f32 GuiStyle::CalcHeight(GuiContent* content, f32 width) const
    {
        if (fixedHeight_ != 0)
            return fixedHeight_;
        const GuiContent& c = content ? *content : *GuiContent::none();
        if (!wordWrap_ || c.text().Empty() || imagePosition_ == ImagePosition::ImageOnly)
            return CalcSize(content).y;
        f32 room = width - f32(padding()->horizontal());
        engine::Texture2D* image = imagePosition_ == ImagePosition::TextOnly ? nullptr : c.image();
        f32 ih = 0;
        if (image && imagePosition_ == ImagePosition::ImageLeft)
            room -= f32(image->width());
        else if (image)
            ih = f32(image->height());
        TextGenerationSettings settings = TextSettings(*this, room > 1 ? room : 1, -1, Color(1, 1, 1, 1));
        Vector2 t = S().generator.PreferredSize(c.text().View(), settings, room > 1 ? room : 1);
        f32 h = std::ceil(t.y) + ih;
        if (image && imagePosition_ == ImagePosition::ImageLeft && f32(image->height()) > h)
            h = f32(image->height());
        return h + f32(padding()->vertical());
    }

    void GuiStyle::CalcMinMaxWidth(GuiContent* content, f32& minWidth, f32& maxWidth) const
    {
        Vector2 size = CalcSize(content);
        maxWidth = size.x;
        minWidth = size.x;
        if (fixedWidth_ != 0 || !wordWrap_ || !content)
            return;
        // Wrapped: as narrow as its widest word.
        String text = content->text();
        TextGenerationSettings settings = TextSettings(*this, -1, -1, Color(1, 1, 1, 1));
        f32 widest = 0;
        usize start = 0;
        StringView all = text.View();
        for (usize i = 0; i <= all.Size(); i++)
            if (i == all.Size() || all[i] == ' ' || all[i] == '\n')
            {
                if (i > start)
                {
                    f32 w = S().generator.PreferredSize(all.Substr(start, i - start), settings).x;
                    widest = w > widest ? w : widest;
                }
                start = i + 1;
            }
        minWidth = std::ceil(widest) + f32(padding()->horizontal());
        if (minWidth > maxWidth)
            minWidth = maxWidth;
    }

    Vector2 GuiStyle::GetCursorPixelPosition(Rect position, GuiContent* content, i32 cursorStringIndex) const
    {
        const GuiContent& c = content ? *content : *GuiContent::none();
        ContentLayout l = LayOut(*this, position, c);
        Rect tr = l.text;
        StringView text = c.text().View();
        TextInfo info;
        TextIn(*this, tr, text, Color(1, 1, 1, 1), info);
        u32 b = ByteOfUnit(text, cursorStringIndex);
        // On the character at the index, else after the last one (of its line).
        const CharacterInfo* last = nullptr;
        for (const CharacterInfo& ch : info.characterInfo)
        {
            if (ch.index >= b)
                return Vector2(tr.x + ch.origin, tr.y + ch.ascender);
            last = &ch;
        }
        if (last)
        {
            if (last->character == U'\n' && !info.lineInfo.Empty())
            {
                // After a line feed: the start of the next line.
                const LineInfo& line = info.lineInfo.Back();
                return Vector2(tr.x + line.x, tr.y + line.ascender);
            }
            return Vector2(tr.x + last->xAdvance, tr.y + last->ascender);
        }
        // No text: where the first line starts by the alignment.
        f32 h = lineHeight();
        Vector2 at = Place(tr, Vector2(0, h), alignment_);
        return Vector2(at.x, at.y);
    }

    bool GuiStyle::LinkAtPoint(Rect position, const GuiContent& content, Vector2 point, bool href, String& out) const
    {
        ContentLayout l = LayOut(*this, position, content);
        Rect tr = l.text;
        TextInfo info;
        TextIn(*this, tr, content.text().View(), Color(1, 1, 1, 1), info);
        Vector2 p(point.x - tr.x, point.y - tr.y);
        for (u32 i = 0; i < info.characterInfo.Size(); i++)
        {
            const CharacterInfo& c = info.characterInfo[i];
            if (p.x < c.origin || p.x >= c.xAdvance || p.y < c.ascender || p.y >= c.descender)
                continue;
            for (const LinkInfo& link : info.linkInfo)
                if (link.href == href && i >= link.firstCharacterIndex && i < link.firstCharacterIndex + link.characterCount)
                {
                    out = link.id;
                    return true;
                }
            return false;
        }
        return false;
    }

    i32 GuiStyle::GetCursorStringIndex(Rect position, GuiContent* content, Vector2 cursorPixelPosition) const
    {
        const GuiContent& c = content ? *content : *GuiContent::none();
        ContentLayout l = LayOut(*this, position, c);
        Rect tr = l.text;
        StringView text = c.text().View();
        TextInfo info;
        TextIn(*this, tr, text, Color(1, 1, 1, 1), info);
        if (info.characterInfo.Empty())
            return 0;
        Vector2 p(cursorPixelPosition.x - tr.x, cursorPixelPosition.y - tr.y);
        // The line under the point (the first or the last when above or below them all).
        u32 line = 0;
        for (u32 i = 0; i < info.lineInfo.Size(); i++)
            if (p.y >= info.lineInfo[i].ascender)
                line = i;
        u32 best = ~0u;
        for (const CharacterInfo& ch : info.characterInfo)
            if (ch.lineNumber == line)
            {
                if (p.x < (ch.origin + ch.xAdvance) * 0.5f)
                    return UnitOfByte(text, ch.index);
                best = ch.index;
            }
        if (best == ~0u)
            return Utf16Length(text);
        // After the line's last character (before its line feed).
        for (usize i = 0; i < info.characterInfo.Size(); i++)
            if (info.characterInfo[i].index == best)
            {
                if (info.characterInfo[i].character == U'\n')
                    return UnitOfByte(text, best);
                return i + 1 < info.characterInfo.Size() ? UnitOfByte(text, info.characterInfo[i + 1].index) : Utf16Length(text);
            }
        return Utf16Length(text);
    }
}

namespace helix::ui::gui::detail
{
    TextFont* FontOf(const GuiStyle& style)
    {
        State& s = S();
        s.fallbacks.Clear();
        TextFont* primary = nullptr;
        FontAsset* a = style.font();
        if (!a)
            a = Skin().font();
        if (a)
            primary = a->textFont();
        Array<TextFont*> defaults;
        TextFont* d = overlay::DefaultFont(defaults);
        if (!primary)
            primary = d;
        else if (d && d != primary)
            s.fallbacks.Push(d);
        for (TextFont* f : defaults)
            if (f != primary)
                s.fallbacks.Push(f);
        return primary;
    }

    TextGenerationSettings TextSettings(const GuiStyle& style, f32 width, f32 height, Color color)
    {
        TextGenerationSettings t;
        t.font = FontOf(style);
        t.fallbacks = Span<TextFont* const>(S().fallbacks.Data(), S().fallbacks.Size());
        t.fontSize = style.EffectiveFontSize();
        t.color = color;
        t.fontStyle = u32(style.fontStyle());
        i32 column = i32(style.alignment()) % 3, row = i32(style.alignment()) / 3;
        t.alignment = column == 0 ? TextAlignment::Left : column == 1 ? TextAlignment::Center : TextAlignment::Right;
        t.verticalAlignment = row == 0 ? TextVerticalAlignment::Top : row == 1 ? TextVerticalAlignment::Middle : TextVerticalAlignment::Bottom;
        t.wrapping = style.wordWrap() ? TextWrappingMode::Normal : TextWrappingMode::NoWrap;
        t.overflow = style.clipping() == TextClipping::Ellipsis ? TextOverflowMode::Ellipsis : TextOverflowMode::Overflow;
        t.richText = style.richText();
        t.width = width;
        t.height = height;
        return t;
    }

    void DrawText(const TextInfo& info, Vector2 at, f32 opacity)
    {
        UIMesh& mesh = S().mesh;
        for (const TextRect& t : info.rects)
        {
            Color c = t.color;
            c.a *= opacity;
            mesh.Rectangle(Rect(at.x + t.rect.x, at.y + t.rect.y, t.rect.width, t.rect.height), Color(c.r * c.a, c.g * c.a, c.b * c.a, c.a));
        }
        for (const TextQuad& q : info.quads)
        {
            engine::Texture2D* texture = q.sprite ? static_cast<engine::Texture2D*>(const_cast<void*>(q.sprite)) : overlay::GlyphTexture(q);
            if (!texture)
                continue;
            UIVertexKind kind = UIVertexKind::Image;
            f32 scale = 1;
            if (!q.colorGlyph && q.font && !q.sprite)
            {
                GlyphRenderMode mode = q.font->Cache().Mode();
                kind = mode == GlyphRenderMode::Sdf ? UIVertexKind::GlyphSdf : mode == GlyphRenderMode::Msdf ? UIVertexKind::GlyphMsdf : UIVertexKind::GlyphCoverage;
                scale = 2.0f * f32(q.font->Cache().Padding()) * q.sdfScale;
            }
            Color c = q.color;
            f32 a = c.a * opacity;
            const Vector2 p[4] = {Vector2(at.x + q.corners[0].x, at.y + q.corners[0].y), Vector2(at.x + q.corners[1].x, at.y + q.corners[1].y),
                                  Vector2(at.x + q.corners[2].x, at.y + q.corners[2].y), Vector2(at.x + q.corners[3].x, at.y + q.corners[3].y)};
            mesh.TexturedQuad(p, q.uv, Color(c.r * a, c.g * a, c.b * a, a), texture, kind, 0.5f - (q.bold ? 0.08f : 0.0f), scale);
        }
    }
}
