#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>

// The geometry of a panel for the shader Hidden/UI/Default: indexed vertices in batches, each batch drawing with up to
// four textures (its slots; a vertex names its slot). Shapes (rounded boxes with per side borders) are quads whose
// fragments compute the box's distance; images and glyphs are textured quads; painted paths are triangles of flat
// color. Every vertex carries the rounded rectangle it is clipped to. Pixels from the top left, colors premultiplied.
namespace helix::ui
{
    enum class UIVertexKind : u8
    {
        Flat = 0,
        GlyphSdf = 1,
        GlyphMsdf = 2,
        Image = 3,
        GlyphCoverage = 4,
        ShapeFill = 5,
        ShapeBorder = 6,
        ShapeImage = 7
    };

    struct UIVertex
    {
        Vector2 position;
        Color color;
        // The texture's uv, and the point's place in its shape's box.
        Vector4 uvLocal;
        // Kind, edge or side, scale, texture slot.
        Vector4 parameters;
        // The shape box's size (xy) and the clip's corner radius (z).
        Vector4 size;
        // Corner radii (top left, top right, bottom right, bottom left) and border widths (left, top, right, bottom).
        Vector4 radii, borders;
        // The clip rectangle (x0, y0, x1, y1).
        Vector4 clipRect;
    };

    // A rounded clip rectangle (the whole plane when unbounded) with one corner radius (the shader takes one).
    struct UIClip
    {
        Rect rect = Rect(-1e6f, -1e6f, 2e6f, 2e6f);
        f32 radius = 0;
        // The intersection with an inner clip (rectangles intersect; the corners are the inner one's).
        UIClip Within(const Rect& inner, const Vector4& innerRadii) const;
    };

    struct UIBatch
    {
        static constexpr u32 kSlots = 4;
        // Opaque handles of the textures (the host's), null for an unused slot.
        const void* textures[kSlots] = {};
        u32 firstIndex = 0, indexCount = 0;
    };

    // A box with rounded corners and borders.
    struct UIShape
    {
        Rect rect;
        Vector4 radii;
        // Left, top, right, bottom.
        f32 borders[4] = {0, 0, 0, 0};
        Color fill = Color(0, 0, 0, 0);
        Color borderColors[4];
    };

    enum class UIScaleMode : u8
    {
        StretchToFill,
        ScaleAndCrop,
        ScaleToFit
    };

    // An image: a texture's uv rectangle (0 to 1, v down) into a rect, tinted, optionally 9-sliced (the slices in
    // texture pixels, drawn at `sliceScale` screen pixels per texture pixel), inside a rounded box.
    struct UIImage
    {
        const void* texture = nullptr;
        Rect uv = Rect(0, 0, 1, 1);
        // The texture's size in pixels (for the scale modes and the slices).
        f32 textureWidth = 1, textureHeight = 1;
        Color tint = Color(1, 1, 1, 1);
        UIScaleMode scaleMode = UIScaleMode::StretchToFill;
        f32 slices[4] = {0, 0, 0, 0};
        f32 sliceScale = 1;
        // The texture's rows go up (a texture's own: v 0 at the bottom); the uv rectangle is then the whole of it.
        bool rowsUp = false;
    };

    class UIMesh
    {
    public:
        void Clear();
        Span<const UIVertex> Vertices() const { return Span<const UIVertex>(vertices_.Data(), vertices_.Size()); }
        Span<const u32> Indices() const { return Span<const u32>(indices_.Data(), indices_.Size()); }
        Span<const UIBatch> Batches() const { return Span<const UIBatch>(batches_.Data(), batches_.Size()); }

        void SetClip(const UIClip& clip) { clip_ = clip; }
        const UIClip& Clip() const { return clip_; }

        // A filled and bordered box (the fill inside the borders; a border of four colors as four sides).
        void Shape(const UIShape& shape, f32 opacity);
        // An image in a rect: its scale mode and slices, cut to the rounded box `shapeRadii` (inside `borders`).
        void Image(const Rect& rect, const UIImage& image, const Vector4& shapeRadii, const f32 (&borders)[4], f32 opacity);
        // A textured quad (corners clockwise from the top left) of a kind with its parameters (edge, scale).
        void TexturedQuad(const Vector2 (&corners)[4], const Rect& uv, Color color, const void* texture, UIVertexKind kind, f32 edge, f32 scale);
        // A flat rectangle.
        void Rectangle(const Rect& rect, Color color);
        // Flat triangles (painted paths): positions and premultiplied colors with indices into them.
        void Triangles(Span<const Vector2> positions, Span<const Color> colors, Span<const u32> indices);

        // Draw calls the batches need.
        u32 DrawCalls() const { return u32(batches_.Size()); }

    private:
        // The batch and slot a texture goes to (a new batch when the current one's slots are taken).
        f32 Slot(const void* texture, u32 indexCount);
        UIVertex Base(UIVertexKind kind) const;
        void Push(const UIVertex& v) { vertices_.Push(v); }
        void QuadIndices(u32 base);

        Array<UIVertex> vertices_;
        Array<u32> indices_;
        Array<UIBatch> batches_;
        UIClip clip_;
    };
}
