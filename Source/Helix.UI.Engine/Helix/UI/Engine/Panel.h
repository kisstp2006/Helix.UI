#pragma once

#include <Helix/Math/Color.h>
#include <Helix/UI/Markup/Hxml.h>

// A panel: an element tree with its layout and styles, built from markup, laid out in a box of the screen's size and
// drawn as quads: each visible element's background and its borders as flat quads in their colors, in tree order.
namespace helix::ui
{
    // A vertex of the panel's quads: pixels from the top left, the color premultiplied by its alpha.
    struct PanelVertex
    {
        f32 x, y;
        Color color;
    };

    class Panel
    {
    public:
        Panel();

        ElementTree& tree() { return tree_; }
        LayoutEngine& layout() { return layout_; }
        StyleEngine& styles() { return styles_; }
        ElementHandle root() const { return root_; }

        // The root's children made again from a document (errors of the markup and its sheets are returned).
        void Build(const HxmlDocument& document, HxmlResolver* resolver, Array<HxmlError>& errors);
        // Styles (with transitions over dt), then the layout in a box of width x height pixels.
        void Update(f32 dt, f32 width, f32 height);
        // The quads (four vertices each, in drawing order) of the visible elements.
        void Quads(Array<PanelVertex>& out) const;

    private:
        ElementTree tree_;
        LayoutEngine layout_;
        StyleEngine styles_;
        ElementHandle root_;
    };
}
