#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Vector.h>

// A vector image: triangles of colors (premultiplied) in its own pixels, made by a painter (Painter2D.SaveToVectorImage)
// and drawn by MeshGenerationContext.DrawVectorImage.
namespace helix::ui
{
    HX_CLASS(Namespace = "HelixEngine.UI")
    class VectorImage : public engine::ScriptableObject
    {
    public:
        HX_PROPERTY(ReadOnly) f32 width() const { return size_.x; }
        HX_PROPERTY(ReadOnly) f32 height() const { return size_.y; }

        // ---------------------------------------------------------------------------------------- for C++
        void Set(Span<const Vector2> positions, Span<const Color> colors, Span<const u32> indices);
        Span<const Vector2> positions() const { return Span<const Vector2>(positions_.Data(), positions_.Size()); }
        Span<const Color> colors() const { return Span<const Color>(colors_.Data(), colors_.Size()); }
        Span<const u32> indices() const { return Span<const u32>(indices_.Data(), indices_.Size()); }

    private:
        Array<Vector2> positions_;
        Array<Color> colors_;
        Array<u32> indices_;
        Vector2 size_ = Vector2(0, 0);
    };
}
