#include <Helix/UI/Engine/Panel.h>

namespace helix::ui
{
    namespace
    {
        constexpr f32 kBoldEdge = 0.08f;

        // The settings a text was laid out with are those of now (the fallbacks are compared apart).
        bool SameSettings(const TextGenerationSettings& a, const TextGenerationSettings& b)
        {
            return a.font == b.font && a.fontSize == b.fontSize && a.color == b.color && a.fontStyle == b.fontStyle && a.alignment == b.alignment &&
                   a.verticalAlignment == b.verticalAlignment && a.characterSpacing == b.characterSpacing && a.wordSpacing == b.wordSpacing &&
                   a.lineSpacing == b.lineSpacing && a.paragraphSpacing == b.paragraphSpacing && a.wrapping == b.wrapping && a.overflow == b.overflow &&
                   a.width == b.width && a.height == b.height;
        }
    }

    Panel::Panel() : layout_(tree_), styles_(tree_, layout_)
    {
        root_ = tree_.Create(Intern("Element"));
        // The root fills the panel.
        LayoutStyle s;
        s.width = Length::Percent(100);
        s.height = Length::Percent(100);
        layout_.SetStyle(root_, s);
    }

    void Panel::Build(const HxmlDocument& document, HxmlResolver* resolver, Array<HxmlError>& errors)
    {
        for (ElementHandle c = tree_.FirstChild(root_); c.IsValid(); c = tree_.FirstChild(root_))
            tree_.Destroy(c);
        styles_.ClearStyleSheets(root_);
        Instantiate(document, tree_, styles_, root_, resolver, errors);
    }

    void Panel::Settings(const ComputedStyle& style, TextSlot& slot) const
    {
        TextGenerationSettings& s = slot.settings;
        s = TextGenerationSettings();
        s.font = slot.font;
        s.fallbacks = Span<TextFont* const>(slot.fallbacks.Data(), slot.fallbacks.Size());
        f32 size = style.fontSize.unit == Length::Unit::Pixel ? style.fontSize.value : 12.0f;
        s.fontSize = size > 0 ? size : 12.0f;
        s.color = style.color;
        s.fontStyle = (style.fontStyle == FontStyle::Bold || style.fontStyle == FontStyle::BoldAndItalic ? TextFlags::Bold : 0) |
                      (style.fontStyle == FontStyle::Italic || style.fontStyle == FontStyle::BoldAndItalic ? TextFlags::Italic : 0);
        constexpr TextAlignment kHorizontal[] = {TextAlignment::Left, TextAlignment::Center, TextAlignment::Right};
        constexpr TextVerticalAlignment kVertical[] = {TextVerticalAlignment::Top, TextVerticalAlignment::Middle, TextVerticalAlignment::Bottom};
        s.alignment = kHorizontal[u32(style.textAlign) % 3];
        s.verticalAlignment = kVertical[u32(style.textAlign) / 3];
        s.wrapping = style.whiteSpace == WhiteSpace::Normal || style.whiteSpace == WhiteSpace::PreWrap ? TextWrappingMode::Normal : TextWrappingMode::NoWrap;
        s.overflow = style.textOverflow == TextOverflow::Ellipsis ? TextOverflowMode::Ellipsis : TextOverflowMode::Masking;
        // Spacing in pixels to the text layout's hundredths of an em.
        auto em = [&](const Length& l) { return l.unit == Length::Unit::Pixel ? l.value / s.fontSize * 100.0f : 0.0f; };
        s.characterSpacing = em(style.letterSpacing);
        s.wordSpacing = em(style.wordSpacing);
        s.paragraphSpacing = em(style.paragraphSpacing);
    }

    Vector2 Panel::Measure(void* user, ElementHandle e, f32 width, MeasureMode widthMode, f32, MeasureMode)
    {
        auto* panel = static_cast<Panel*>(user);
        if (e.index >= panel->texts_.Size())
            return Vector2(0, 0);
        const TextSlot& slot = panel->texts_[e.index];
        if (!slot.font)
            return Vector2(0, 0);
        // A generator per thread: the layout may measure on several jobs at once.
        thread_local TextGenerator generator;
        f32 limit = widthMode == MeasureMode::Undefined ? -1.0f : width;
        Vector2 size = generator.PreferredSize(panel->tree_.Text(e), slot.settings, slot.settings.wrapping == TextWrappingMode::Normal ? limit : -1.0f);
        // A little more than the measured width, so laying it out again in that width breaks no line.
        size.x = size.x > 0 ? size.x + 0.01f : 0;
        return size;
    }

    void Panel::Update(f32 dt, f32 width, f32 height)
    {
        styles_.Update(root_, dt);
        // The text elements: their fonts and settings now, on this thread (the measures only read them).
        textElements_.Clear();
        tree_.ForEachInSubtree(root_, [&](ElementHandle e, u32) {
            if (tree_.Text(e).Empty())
            {
                if (e.index < texts_.Size() && texts_[e.index].measured && texts_[e.index].generation == e.generation)
                {
                    layout_.SetMeasure(e, nullptr, nullptr);
                    texts_[e.index] = TextSlot();
                }
                return;
            }
            if (e.index >= texts_.Size())
                texts_.Resize(e.index + 1);
            TextSlot& slot = texts_[e.index];
            if (slot.generation != e.generation)
            {
                slot = TextSlot();
                slot.generation = e.generation;
            }
            const ComputedStyle& style = styles_.Computed(e);
            slot.fallbacks.Clear();
            TextFont* font = host_ ? host_->Font(style, slot.fallbacks) : nullptr;
            bool changed = font != slot.font;
            slot.font = font;
            Settings(style, slot);
            if (!slot.measured)
            {
                layout_.SetMeasure(e, &Panel::Measure, this);
                slot.measured = true;
            }
            else if (changed)
                tree_.MarkLayoutDirty(e);
            textElements_.Push(e);
        });
        // The fallback spans point into the slots, which may have moved while the list grew.
        for (ElementHandle e : textElements_)
            texts_[e.index].settings.fallbacks = Span<TextFont* const>(texts_[e.index].fallbacks.Data(), texts_[e.index].fallbacks.Size());
        layout_.Update(root_, width, height);
        // Each text in its element's content box, laid out again when the text or the box changed.
        for (ElementHandle e : textElements_)
        {
            TextSlot& slot = texts_[e.index];
            if (!slot.font)
            {
                slot.info.Clear();
                continue;
            }
            const LayoutBox& b = layout_.Box(e);
            f32 w = b.width - b.border[kLeft] - b.border[kRight] - b.padding[kLeft] - b.padding[kRight];
            f32 h = b.height - b.border[kTop] - b.border[kBottom] - b.padding[kTop] - b.padding[kBottom];
            slot.settings.width = w > 0 ? w : 0;
            slot.settings.height = h > 0 ? h : 0;
            bool sameFallbacks = slot.fallbacks.Size() == slot.laidOutFallbacks.Size();
            for (usize i = 0; sameFallbacks && i < slot.fallbacks.Size(); i++)
                sameFallbacks = slot.fallbacks[i] == slot.laidOutFallbacks[i];
            if (slot.textVersion == tree_.TextVersion() && slot.width == w && slot.height == h && sameFallbacks && SameSettings(slot.settings, slot.laidOut))
                continue;
            generator_.Generate(tree_.Text(e), slot.settings, slot.info);
            slot.textVersion = tree_.TextVersion();
            slot.width = w;
            slot.height = h;
            slot.laidOut = slot.settings;
            slot.laidOutFallbacks = slot.fallbacks;
        }
    }

    const TextInfo* Panel::TextOf(ElementHandle e) const
    {
        if (!tree_.IsAlive(e) || e.index >= texts_.Size() || texts_[e.index].generation != e.generation || !texts_[e.index].font)
            return nullptr;
        return &texts_[e.index].info;
    }

    void Panel::Draw(Array<PanelVertex>& vertices, Array<u32>& indices, Array<PanelBatch>& batches)
    {
        vertices.Clear();
        indices.Clear();
        batches.Clear();
        auto premultiply = [](Color c, f32 opacity) {
            f32 a = c.a * opacity;
            return Color(c.r * a, c.g * a, c.b * a, a);
        };
        // A quad into the current batch (a new one when its texture differs; flat quads go with any).
        auto quad = [&](const Vector2 (&p)[4], const Rect& uv, Color color, engine::Texture2D* texture, f32 kind, f32 edge, f32 scale) {
            if (color.a <= 0)
                return;
            if (batches.Empty() || (texture && batches.Back().texture && batches.Back().texture != texture))
                batches.Push({texture, u32(indices.Size()), 0});
            if (texture && !batches.Back().texture)
                batches.Back().texture = texture;
            u32 base = u32(vertices.Size());
            f32 us[4] = {uv.x, uv.x + uv.width, uv.x + uv.width, uv.x};
            f32 vs[4] = {uv.y, uv.y, uv.y + uv.height, uv.y + uv.height};
            for (u32 k = 0; k < 4; k++)
                vertices.Push({p[k].x, p[k].y, color, us[k], vs[k], kind, edge, scale, 0});
            for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u})
                indices.Push(base + k);
            batches.Back().indexCount += 6;
        };
        auto rect = [&](f32 x0, f32 y0, f32 x1, f32 y1, Color c) {
            if (x1 <= x0 || y1 <= y0)
                return;
            const Vector2 p[4] = {Vector2(x0, y0), Vector2(x1, y0), Vector2(x1, y1), Vector2(x0, y1)};
            quad(p, Rect(), c, nullptr, 0, 0, 0);
        };
        auto walk = [&](auto& self, ElementHandle e, f32 opacity) -> void {
            const ComputedStyle& s = styles_.Computed(e);
            if (s.layout.display == Display::None)
                return;
            opacity *= s.opacity;
            Rect r = layout_.WorldRect(e);
            const LayoutBox& b = layout_.Box(e);
            if (s.visibility == Visibility::Visible)
            {
                f32 x0 = r.x, y0 = r.y, x1 = r.x + r.width, y1 = r.y + r.height;
                rect(x0 + b.border[kLeft], y0 + b.border[kTop], x1 - b.border[kRight], y1 - b.border[kBottom], premultiply(s.backgroundColor, opacity));
                rect(x0, y0, x1, y0 + b.border[kTop], premultiply(s.borderColor[kTop], opacity));
                rect(x0, y1 - b.border[kBottom], x1, y1, premultiply(s.borderColor[kBottom], opacity));
                rect(x0, y0 + b.border[kTop], x0 + b.border[kLeft], y1 - b.border[kBottom], premultiply(s.borderColor[kLeft], opacity));
                rect(x1 - b.border[kRight], y0 + b.border[kTop], x1, y1 - b.border[kBottom], premultiply(s.borderColor[kRight], opacity));
                if (const TextInfo* text = TextOf(e); text && host_)
                {
                    f32 ox = r.x + b.border[kLeft] + b.padding[kLeft], oy = r.y + b.border[kTop] + b.padding[kTop];
                    for (const TextRect& t : text->rects)
                        if (t.behind)
                            rect(ox + t.rect.x, oy + t.rect.y, ox + t.rect.x + t.rect.width, oy + t.rect.y + t.rect.height, premultiply(t.color, opacity));
                    for (const TextQuad& q : text->quads)
                    {
                        engine::Texture2D* texture = host_->Texture(q);
                        if (!texture)
                            continue;
                        f32 kind = q.colorGlyph ? 3.0f : 1.0f;
                        f32 scale = 1;
                        if (!q.colorGlyph && q.font)
                        {
                            GlyphRenderMode mode = q.font->Cache().Mode();
                            kind = mode == GlyphRenderMode::Sdf ? 1.0f : mode == GlyphRenderMode::Msdf ? 2.0f : 4.0f;
                            scale = 2.0f * f32(q.font->Cache().Padding()) * q.sdfScale;
                        }
                        const Vector2 p[4] = {Vector2(ox + q.corners[0].x, oy + q.corners[0].y), Vector2(ox + q.corners[1].x, oy + q.corners[1].y),
                                              Vector2(ox + q.corners[2].x, oy + q.corners[2].y), Vector2(ox + q.corners[3].x, oy + q.corners[3].y)};
                        quad(p, q.uv, premultiply(q.color, opacity), texture, kind, 0.5f - (q.bold ? kBoldEdge : 0.0f), scale);
                    }
                    for (const TextRect& t : text->rects)
                        if (!t.behind)
                            rect(ox + t.rect.x, oy + t.rect.y, ox + t.rect.x + t.rect.width, oy + t.rect.y + t.rect.height, premultiply(t.color, opacity));
                }
            }
            for (ElementHandle c = tree_.FirstChild(e); c.IsValid(); c = tree_.NextSibling(c))
                self(self, c, opacity);
        };
        walk(walk, root_, 1.0f);
    }
}
