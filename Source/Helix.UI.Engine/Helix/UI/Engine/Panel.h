#pragma once

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Math/Color.h>
#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/World/ElementWorld.h>
#include <Helix/UI/Markup/Hxml.h>
#include <Helix/UI/Rendering/Painter.h>
#include <Helix/UI/Rendering/UIMesh.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

namespace helix::engine
{
    class Texture2D;
}

// A panel: a root in an element world (the process's by default) with the elements under it, built from markup, laid
// out in a box of the screen's size and
// drawn into a UIMesh in tree order: each visible element's box (rounded corners, borders, background color and image),
// what its type paints, its text; overflow hidden clips the descendants to the padding box. Elements with text are
// measured by the text layout and set in their content box. Its event system takes the host's input; the root is not
// picked (only the elements of the markup are), and the element types attach their behavior once the panel is built.
namespace helix::ui
{
    // What the panel needs from its host: fonts for styles, textures for glyphs and background images.
    class PanelTextHost
    {
    public:
        virtual ~PanelTextHost() = default;
        // The font of a style (null: no text) and the fonts after its own chain.
        virtual TextFont* Font(const ComputedStyle& style, Array<TextFont*>& fallbacks) = 0;
        // The texture holding a glyph quad's image.
        virtual engine::Texture2D* Texture(const TextQuad& quad) = 0;
        // A background image by its url or resource: the texture (or an atlas page) with its uv rect and size.
        virtual bool Image(Atom path, UIImage& out) = 0;
    };

    class Panel
    {
    public:
        explicit Panel(ElementWorld& world = ElementWorld::Main());
        // Its root and the elements under it go.
        ~Panel();
        Panel(const Panel&) = delete;
        Panel& operator=(const Panel&) = delete;

        ElementWorld& world() { return world_; }
        ElementTree& tree() { return world_.tree(); }
        LayoutEngine& layout() { return world_.layout(); }
        StyleEngine& styles() { return world_.styles(); }
        EventSystem& events() { return events_; }
        ElementHandle root() const { return root_; }
        void SetTextHost(PanelTextHost* host) { host_ = host; }

        // The root's children made again from a document (errors of the markup and its sheets are returned).
        void Build(const HxmlDocument& document, HxmlResolver* resolver, Array<HxmlError>& errors);
        // Styles (with transitions over dt), the text's fonts, the layout in a box of width x height pixels, the text.
        void Update(f32 dt, f32 width, f32 height);
        // The visible elements into a mesh (cleared first).
        void Draw(UIMesh& mesh);
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
        void DrawElement(UIMesh& mesh, ElementHandle e, f32 opacity);

        ElementWorld& world_;
        ElementTree& tree_;
        LayoutEngine& layout_;
        StyleEngine& styles_;
        EventSystem events_;
        ElementHandle root_;
        PanelTextHost* host_ = nullptr;
        // The text of the panel's text elements, by their slot in the world.
        HashMap<u32, TextSlot> texts_;
        Array<ElementHandle> textElements_;
        TextGenerator generator_;
        Painter painter_;
    };
}
