#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Vector.h>

// The Painter: vector paths tessellated into triangles of flat color. A path is lines, arcs and Bezier curves in
// subpaths; Fill fills its closed subpaths (non zero or even odd: subpaths inside others become holes), Stroke draws
// its lines with a width, joins, caps and a color or a gradient along the stroke. Edges get a pixel wide fringe that
// fades to transparent (antialiasing). Pixels, y down; angles in degrees, clockwise on the screen.
namespace helix::ui
{
    enum class LineJoin : u8
    {
        Miter,
        Bevel,
        Round
    };

    enum class LineCap : u8
    {
        Butt,
        Round,
        Square
    };

    enum class FillRule : u8
    {
        NonZero,
        OddEven
    };

    enum class ArcDirection : u8
    {
        Clockwise,
        CounterClockwise
    };

    // A color key of a gradient: the place (0 to 1) and its color.
    struct GradientKey
    {
        f32 time = 0;
        Color color;
    };

    class Painter
    {
    public:
        f32 lineWidth = 1;
        Color strokeColor = Color(0, 0, 0, 1);
        Color fillColor = Color(1, 1, 1, 1);
        LineJoin lineJoin = LineJoin::Miter;
        LineCap lineCap = LineCap::Butt;
        f32 miterLimit = 10;
        // A gradient along the stroke (empty: strokeColor).
        Array<GradientKey> strokeGradient = Array<GradientKey>();
        // Multiplies every color's alpha (the element's opacity).
        f32 opacity = 1;

        void BeginPath();
        void ClosePath();
        void MoveTo(Vector2 p);
        void LineTo(Vector2 p);
        // A circular arc around a center from one angle to another.
        void Arc(Vector2 center, f32 radius, f32 startAngle, f32 endAngle, ArcDirection direction = ArcDirection::Clockwise);
        // The arc of a radius tangent to the lines from the pen to p1 and from p1 to p2.
        void ArcTo(Vector2 p1, Vector2 p2, f32 radius);
        void QuadraticCurveTo(Vector2 control, Vector2 p);
        void BezierCurveTo(Vector2 control1, Vector2 control2, Vector2 p);
        void Fill(FillRule rule = FillRule::NonZero);
        void Stroke();

        // What was painted: positions, premultiplied colors and triangles.
        Span<const Vector2> Positions() const { return Span<const Vector2>(positions_.Data(), positions_.Size()); }
        Span<const Color> Colors() const { return Span<const Color>(colors_.Data(), colors_.Size()); }
        Span<const u32> Indices() const { return Span<const u32>(indices_.Data(), indices_.Size()); }
        void Clear();

    private:
        struct Subpath
        {
            u32 first = 0, count = 0;
            bool closed = false;
        };
        void Point(Vector2 p);
        u32 Vertex(Vector2 p, Color c);
        Color Premultiplied(Color c, f32 alpha) const;
        Color StrokeColorAt(f32 t) const;
        void FillPolygon(const Array<Array<Vector2>>& outers, const Array<Array<Array<Vector2>>>& holes, Color color);
        void StrokePolyline(Span<const Vector2> points, bool closed, f32 startLength, f32 totalLength);

        Array<Vector2> points_;
        Array<Subpath> subpaths_;
        Vector2 pen_;
        bool hasPen_ = false;
        Array<Vector2> positions_;
        Array<Color> colors_;
        Array<u32> indices_;
    };
}
