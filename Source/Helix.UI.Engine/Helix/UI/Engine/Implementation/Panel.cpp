#include <Helix/UI/Engine/Panel.h>

namespace helix::ui
{
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

    void Panel::Update(f32 dt, f32 width, f32 height)
    {
        styles_.Update(root_, dt);
        layout_.Update(root_, width, height);
    }

    void Panel::Quads(Array<PanelVertex>& out) const
    {
        out.Clear();
        auto quad = [&](f32 x0, f32 y0, f32 x1, f32 y1, Color c, f32 opacity) {
            f32 a = c.a * opacity;
            if (a <= 0 || x1 <= x0 || y1 <= y0)
                return;
            Color p(c.r * a, c.g * a, c.b * a, a);
            out.Push({x0, y0, p});
            out.Push({x1, y0, p});
            out.Push({x1, y1, p});
            out.Push({x0, y1, p});
        };
        // Tree order, each element's opacity times its ancestors'; hidden and undisplayed subtrees left out.
        struct Walker
        {
            const Panel& panel;
            const decltype(quad)& emit;
            void Walk(ElementHandle e, f32 opacity)
            {
                const ComputedStyle& s = panel.styles_.Computed(e);
                if (s.layout.display == Display::None)
                    return;
                opacity *= s.opacity;
                Rect r = panel.layout_.WorldRect(e);
                if (s.visibility == Visibility::Visible)
                {
                    const LayoutBox& b = panel.layout_.Box(e);
                    f32 x0 = r.x, y0 = r.y, x1 = r.x + r.width, y1 = r.y + r.height;
                    emit(x0 + b.border[kLeft], y0 + b.border[kTop], x1 - b.border[kRight], y1 - b.border[kBottom], s.backgroundColor, opacity);
                    emit(x0, y0, x1, y0 + b.border[kTop], s.borderColor[kTop], opacity);
                    emit(x0, y1 - b.border[kBottom], x1, y1, s.borderColor[kBottom], opacity);
                    emit(x0, y0 + b.border[kTop], x0 + b.border[kLeft], y1 - b.border[kBottom], s.borderColor[kLeft], opacity);
                    emit(x1 - b.border[kRight], y0 + b.border[kTop], x1, y1 - b.border[kBottom], s.borderColor[kRight], opacity);
                }
                for (ElementHandle c = panel.tree_.FirstChild(e); c.IsValid(); c = panel.tree_.NextSibling(c))
                    Walk(c, opacity);
            }
        };
        Walker{*this, quad}.Walk(root_, 1.0f);
    }
}
