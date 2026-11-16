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

    Panel::Panel(ElementWorld& world)
        : world_(world), tree_(world.tree()), layout_(world.layout()), styles_(world.styles()),
          events_(world.tree(), world.layout(), world.styles(), &world.callbacks())
    {
        root_ = tree_.Create(Intern("Element"));
        tree_.SetPicking(root_, PickingMode::Ignore);
        events_.SetRoot(root_);
        // The root fills the panel.
        LayoutStyle s;
        s.width = Length::Percent(100);
        s.height = Length::Percent(100);
        layout_.SetStyle(root_, s);
    }

    Panel::~Panel()
    {
        tree_.ForEachInSubtree(root_, [&](ElementHandle e, u32) { world_.callbacks().Clear(e); });
        styles_.ClearStyleSheets(root_);
        tree_.Destroy(root_);
    }

    void Panel::Build(const HxmlDocument& document, HxmlResolver* resolver, Array<HxmlError>& errors)
    {
        for (ElementHandle c = tree_.FirstChild(root_); c.IsValid(); c = tree_.FirstChild(root_))
            tree_.Destroy(c);
        styles_.ClearStyleSheets(root_);
        Instantiate(document, tree_, styles_, root_, resolver, errors);
        events_.Forget();
        // The types' behavior on the new elements.
        tree_.ForEachInSubtree(root_, [&](ElementHandle e, u32) {
            if (const ElementType* type = FindElementType(tree_.Type(e)); type && type->attach)
                type->attach(events_, e);
        });
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
        const TextSlot* found = panel->texts_.Find(e.index);
        if (!found || found->generation != e.generation)
            return Vector2(0, 0);
        const TextSlot& slot = *found;
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
                TextSlot* old = texts_.Find(e.index);
                if (old && old->measured && old->generation == e.generation)
                    layout_.SetMeasure(e, nullptr, nullptr);
                if (old)
                    texts_.Remove(e.index);
                return;
            }
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
        {
            TextSlot& slot = texts_[e.index];
            slot.settings.fallbacks = Span<TextFont* const>(slot.fallbacks.Data(), slot.fallbacks.Size());
        }
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
        const TextSlot* slot = tree_.IsAlive(e) ? texts_.Find(e.index) : nullptr;
        if (!slot || slot->generation != e.generation || !slot->font)
            return nullptr;
        return &slot->info;
    }

    void Panel::Draw(UIMesh& mesh)
    {
        mesh.Clear();
        DrawElement(mesh, root_, 1.0f);
    }

    void Panel::DrawElement(UIMesh& mesh, ElementHandle e, f32 opacity)
    {
        const ComputedStyle& s = styles_.Computed(e);
        if (s.layout.display == Display::None)
            return;
        opacity *= s.opacity;
        Rect r = layout_.WorldRect(e);
        const LayoutBox& b = layout_.Box(e);
        // Radii in pixels (percentages of the smaller side), no larger than half the box.
        f32 smaller = r.width < r.height ? r.width : r.height;
        auto radius = [&](const Length& l) {
            f32 v = l.unit == Length::Unit::Percent ? l.value * smaller / 100.0f : l.unit == Length::Unit::Pixel ? l.value : 0.0f;
            return v < 0 ? 0 : v > smaller * 0.5f ? smaller * 0.5f : v;
        };
        Vector4 radii(radius(s.borderRadius[kTopLeft]), radius(s.borderRadius[kTopRight]), radius(s.borderRadius[kBottomRight]),
                      radius(s.borderRadius[kBottomLeft]));
        const f32 borders[4] = {b.border[kLeft], b.border[kTop], b.border[kRight], b.border[kBottom]};
        if (s.visibility == Visibility::Visible && opacity > 0)
        {
            UIShape shape;
            shape.rect = r;
            shape.radii = radii;
            for (u32 k = 0; k < 4; k++)
            {
                shape.borders[k] = borders[k];
                shape.borderColors[k] = s.borderColor[k];
            }
            shape.fill = s.backgroundColor;
            mesh.Shape(shape, opacity);
            if (s.backgroundImage != kNoAtom && host_)
            {
                UIImage image;
                if (host_->Image(s.backgroundImage, image))
                {
                    image.tint = s.backgroundTint;
                    image.scaleMode = UIScaleMode(u8(s.backgroundScaleMode));
                    for (u32 k = 0; k < 4; k++)
                        image.slices[k] = s.slices[k];
                    image.sliceScale = s.sliceScale;
                    mesh.Image(r, image, radii, borders, opacity);
                }
            }
            f32 ox = r.x + b.border[kLeft] + b.padding[kLeft], oy = r.y + b.border[kTop] + b.padding[kTop];
            f32 cw = r.width - b.border[kLeft] - b.border[kRight] - b.padding[kLeft] - b.padding[kRight];
            f32 ch = r.height - b.border[kTop] - b.border[kBottom] - b.padding[kTop] - b.padding[kBottom];
            // What the element's type paints, in its content box.
            if (const ElementType* type = FindElementType(tree_.Type(e)); type && type->paint)
            {
                painter_.Clear();
                painter_.opacity = opacity;
                type->paint(painter_, tree_, e, cw > 0 ? cw : 0, ch > 0 ? ch : 0);
                Span<const Vector2> positions = painter_.Positions();
                Array<Vector2> moved;
                moved.Reserve(positions.Size());
                for (Vector2 p : positions)
                    moved.Push(Vector2(p.x + ox, p.y + oy));
                mesh.Triangles(Span<const Vector2>(moved.Data(), moved.Size()), painter_.Colors(), painter_.Indices());
            }
            if (const TextInfo* text = TextOf(e); text && host_)
            {
                auto premultiply = [&](Color c) {
                    f32 a = c.a * opacity;
                    return Color(c.r * a, c.g * a, c.b * a, a);
                };
                auto rect = [&](const TextRect& t) {
                    Color c = t.color;
                    c.a *= opacity;
                    mesh.Rectangle(Rect(ox + t.rect.x, oy + t.rect.y, t.rect.width, t.rect.height), Color(c.r * c.a, c.g * c.a, c.b * c.a, c.a));
                };
                for (const TextRect& t : text->rects)
                    if (t.behind)
                        rect(t);
                for (const TextQuad& q : text->quads)
                {
                    engine::Texture2D* texture = host_->Texture(q);
                    if (!texture)
                        continue;
                    UIVertexKind kind = UIVertexKind::Image;
                    f32 scale = 1;
                    if (!q.colorGlyph && q.font)
                    {
                        GlyphRenderMode mode = q.font->Cache().Mode();
                        kind = mode == GlyphRenderMode::Sdf ? UIVertexKind::GlyphSdf : mode == GlyphRenderMode::Msdf ? UIVertexKind::GlyphMsdf : UIVertexKind::GlyphCoverage;
                        scale = 2.0f * f32(q.font->Cache().Padding()) * q.sdfScale;
                    }
                    const Vector2 p[4] = {Vector2(ox + q.corners[0].x, oy + q.corners[0].y), Vector2(ox + q.corners[1].x, oy + q.corners[1].y),
                                          Vector2(ox + q.corners[2].x, oy + q.corners[2].y), Vector2(ox + q.corners[3].x, oy + q.corners[3].y)};
                    mesh.TexturedQuad(p, q.uv, premultiply(q.color), texture, kind, 0.5f - (q.bold ? kBoldEdge : 0.0f), scale);
                }
                for (const TextRect& t : text->rects)
                    if (!t.behind)
                        rect(t);
            }
        }
        // Overflow hidden: the descendants inside the padding box, its corners rounded inside the borders.
        UIClip saved = mesh.Clip();
        if (s.overflow == Overflow::Hidden)
        {
            Rect inner(r.x + borders[0], r.y + borders[1], r.width - borders[0] - borders[2], r.height - borders[1] - borders[3]);
            auto in = [](f32 radius, f32 a, f32 c) {
                f32 m = a > c ? a : c;
                return radius > m ? radius - m : 0.0f;
            };
            Vector4 innerRadii(in(radii.x, borders[0], borders[1]), in(radii.y, borders[2], borders[1]), in(radii.z, borders[2], borders[3]),
                               in(radii.w, borders[0], borders[3]));
            mesh.SetClip(saved.Within(inner, innerRadii));
        }
        for (ElementHandle c = tree_.FirstChild(e); c.IsValid(); c = tree_.NextSibling(c))
            DrawElement(mesh, c, opacity);
        mesh.SetClip(saved);
    }
}
