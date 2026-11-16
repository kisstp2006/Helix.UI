#pragma once

#include <Helix/Math/Color.h>
#include <Helix/UI/Markup/Hxml.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

namespace helix::engine
{
    class Texture2D;
}

// A panel: an element tree with its layout and styles, built from markup, laid out in a box of the screen's size and
// drawn as quads in tree order: each visible element's background and borders as flat quads, then its text (marks,
// glyphs, underlines). Elements with text are measured by the text layout and set in their content box.
namespace helix::ui
{
    // A vertex of the panel's quads: pixels from the top left, the color premultiplied by its alpha, the image's uv,
    // and what the shader does (kind: 0 flat, 1 SDF, 2 MSDF, 3 color image, 4 coverage; the distance fields' edge and
    // scale).
    struct PanelVertex
    {
        f32 x, y;
        Color color;
        f32 u, v;
        f32 kind, edge, scale, unused;
    };

    // Consecutive indices drawn with one texture (null: flat quads only).
    struct PanelBatch
    {
        engine::Texture2D* texture = nullptr;
        u32 firstIndex = 0, indexCount = 0;
    };

    // What the panel's text needs from its host: fonts for styles and textures for glyph images.
    class PanelTextHost
    {
    public:
        virtual ~PanelTextHost() = default;
        // The font of a style (null: no text) and the fonts after its own chain.
        virtual TextFont* Font(const ComputedStyle& style, Array<TextFont*>& fallbacks) = 0;
        // The texture holding a glyph quad's image.
        virtual engine::Texture2D* Texture(const TextQuad& quad) = 0;
    };

    class Panel
    {
    public:
        Panel();

        ElementTree& tree() { return tree_; }
        LayoutEngine& layout() { return layout_; }
        StyleEngine& styles() { return styles_; }
        ElementHandle root() const { return root_; }
        void SetTextHost(PanelTextHost* host) { host_ = host; }

        // The root's children made again from a document (errors of the markup and its sheets are returned).
        void Build(const HxmlDocument& document, HxmlResolver* resolver, Array<HxmlError>& errors);
        // Styles (with transitions over dt), the text's fonts, the layout in a box of width x height pixels, the text.
        void Update(f32 dt, f32 width, f32 height);
        // The quads of the visible elements as indexed vertices in batches.
        void Draw(Array<PanelVertex>& vertices, Array<u32>& indices, Array<PanelBatch>& batches);
        // The text laid out in an element (null when it has none).
        const TextInfo* TextOf(ElementHandle e) const;

    private:
        struct TextSlot
        {
            u32 generation = ~0u;
            bool measured = false;
            TextFont* font = nullptr;
            Array<TextFont*> fallbacks, laidOutFallbacks;
            TextGenerationSettings settings, laidOut;
            // What the laid out text was made from.
            u32 textVersion = ~0u;
            f32 width = -2, height = -2;
            TextInfo info;
        };

        static Vector2 Measure(void* user, ElementHandle e, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode);
        void Settings(const ComputedStyle& style, TextSlot& slot) const;

        ElementTree tree_;
        LayoutEngine layout_;
        StyleEngine styles_;
        ElementHandle root_;
        PanelTextHost* host_ = nullptr;
        Array<TextSlot> texts_;
        Array<ElementHandle> textElements_;
        TextGenerator generator_;
    };
}
