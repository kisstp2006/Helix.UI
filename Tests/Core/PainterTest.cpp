#include <Helix/UI/Rendering/Painter.h>
#include <Helix/Testing/Test.h>

#include <cmath>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

namespace
{
    // The painted alpha at a point: the most of the triangles holding it (each interpolating its corners).
    f32 Coverage(const Painter& p, Vector2 at)
    {
        Span<const Vector2> pos = p.Positions();
        Span<const Color> col = p.Colors();
        Span<const u32> idx = p.Indices();
        f32 best = 0;
        for (usize t = 0; t + 2 < idx.Size(); t += 3)
        {
            Vector2 a = pos[idx[t]], b = pos[idx[t + 1]], c = pos[idx[t + 2]];
            f32 d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
            if (std::fabs(d) < 1e-9f)
                continue;
            f32 u = ((b.y - c.y) * (at.x - c.x) + (c.x - b.x) * (at.y - c.y)) / d;
            f32 v = ((c.y - a.y) * (at.x - c.x) + (a.x - c.x) * (at.y - c.y)) / d;
            f32 w = 1 - u - v;
            if (u < -1e-4f || v < -1e-4f || w < -1e-4f)
                continue;
            f32 alpha = col[idx[t]].a * u + col[idx[t + 1]].a * v + col[idx[t + 2]].a * w;
            best = alpha > best ? alpha : best;
        }
        return best;
    }

    // The color at a point from the first triangle holding it.
    Color ColorAt(const Painter& p, Vector2 at)
    {
        Span<const Vector2> pos = p.Positions();
        Span<const Color> col = p.Colors();
        Span<const u32> idx = p.Indices();
        for (usize t = 0; t + 2 < idx.Size(); t += 3)
        {
            Vector2 a = pos[idx[t]], b = pos[idx[t + 1]], c = pos[idx[t + 2]];
            f32 d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
            if (std::fabs(d) < 1e-9f)
                continue;
            f32 u = ((b.y - c.y) * (at.x - c.x) + (c.x - b.x) * (at.y - c.y)) / d;
            f32 v = ((c.y - a.y) * (at.x - c.x) + (a.x - c.x) * (at.y - c.y)) / d;
            f32 w = 1 - u - v;
            if (u < 0 || v < 0 || w < 0)
                continue;
            const Color &ca = col[idx[t]], &cb = col[idx[t + 1]], &cc = col[idx[t + 2]];
            return Color(ca.r * u + cb.r * v + cc.r * w, ca.g * u + cb.g * v + cc.g * w, ca.b * u + cb.b * v + cc.b * w, ca.a * u + cb.a * v + cc.a * w);
        }
        return Color(0, 0, 0, 0);
    }

    // The area of the fully opaque triangles.
    f32 OpaqueArea(const Painter& p)
    {
        Span<const Vector2> pos = p.Positions();
        Span<const Color> col = p.Colors();
        Span<const u32> idx = p.Indices();
        f32 area = 0;
        for (usize t = 0; t + 2 < idx.Size(); t += 3)
        {
            if (col[idx[t]].a < 0.999f || col[idx[t + 1]].a < 0.999f || col[idx[t + 2]].a < 0.999f)
                continue;
            Vector2 a = pos[idx[t]], b = pos[idx[t + 1]], c = pos[idx[t + 2]];
            area += std::fabs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
        }
        return area;
    }

    bool CoverageIs(const Painter& p, Vector2 at, f32 low, f32 high)
    {
        f32 c = Coverage(p, at);
        bool ok = c >= low && c <= high;
        if (!ok)
            std::printf("    coverage at %g %g is %g, expected %g to %g\n", at.x, at.y, c, low, high);
        return ok;
    }

    void Square(Painter& p, f32 x0, f32 y0, f32 x1, f32 y1, bool clockwise)
    {
        p.MoveTo(Vector2(x0, y0));
        if (clockwise)
        {
            p.LineTo(Vector2(x1, y0));
            p.LineTo(Vector2(x1, y1));
            p.LineTo(Vector2(x0, y1));
        }
        else
        {
            p.LineTo(Vector2(x0, y1));
            p.LineTo(Vector2(x1, y1));
            p.LineTo(Vector2(x1, y0));
        }
        p.ClosePath();
    }
}

HX_TEST(Painter, FillsAPathWithAnAntialiasedEdge)
{
    Painter p;
    Square(p, 10, 10, 90, 90, true);
    p.Fill();
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(50, 9.8f), 0.01f, 0.99f));
    HX_CHECK(CoverageIs(p, Vector2(50, 5), 0, 0));
    HX_CHECK(CoverageIs(p, Vector2(95, 95), 0, 0));
    f32 area = OpaqueArea(p);
    HX_CHECK(area > 80 * 80 * 0.97f && area < 80 * 80 * 1.01f);
}

HX_TEST(Painter, AnInnerContourOfTheOtherDirectionIsAHole)
{
    Painter p;
    Square(p, 0, 0, 100, 100, true);
    Square(p, 30, 30, 70, 70, false);
    p.Fill(FillRule::NonZero);
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0, 0));
    HX_CHECK(CoverageIs(p, Vector2(15, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(85, 85), 0.999f, 1));
    f32 area = OpaqueArea(p);
    HX_CHECK(area > (10000 - 1600) * 0.97f && area < (10000 - 1600) * 1.01f);
}

HX_TEST(Painter, NonZeroFillsAnInnerContourOfTheSameDirectionEvenOddCutsIt)
{
    Painter p;
    Square(p, 0, 0, 100, 100, true);
    Square(p, 30, 30, 70, 70, true);
    p.Fill(FillRule::NonZero);
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0.999f, 1));
    p.Clear();
    Square(p, 0, 0, 100, 100, true);
    Square(p, 30, 30, 70, 70, true);
    p.Fill(FillRule::OddEven);
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0, 0));
    HX_CHECK(CoverageIs(p, Vector2(15, 50), 0.999f, 1));
    // An island in the hole is filled again.
    p.Clear();
    Square(p, 0, 0, 100, 100, true);
    Square(p, 20, 20, 80, 80, true);
    Square(p, 40, 40, 60, 60, true);
    p.Fill(FillRule::OddEven);
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(30, 50), 0, 0));
    HX_CHECK(CoverageIs(p, Vector2(10, 50), 0.999f, 1));
}

HX_TEST(Painter, ArcsAndCurvesFollowTheirShape)
{
    Painter p;
    p.Arc(Vector2(50, 50), 40, 0, 360);
    p.ClosePath();
    p.Fill();
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(88, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(80, 80), 0, 0));
    f32 area = OpaqueArea(p);
    f32 circle = 3.14159265f * 40 * 40;
    HX_CHECK(area > circle * 0.96f && area < circle * 1.01f);

    // A quadratic curve bulging down from a straight top.
    p.Clear();
    p.MoveTo(Vector2(0, 0));
    p.LineTo(Vector2(100, 0));
    p.QuadraticCurveTo(Vector2(50, 100), Vector2(0, 0));
    p.ClosePath();
    p.Fill();
    // The curve's middle is at y 50.
    HX_CHECK(CoverageIs(p, Vector2(50, 45), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(50, 55), 0, 0));

    // A cubic one.
    p.Clear();
    p.MoveTo(Vector2(0, 0));
    p.BezierCurveTo(Vector2(0, 100), Vector2(100, 100), Vector2(100, 0));
    p.ClosePath();
    p.Fill();
    // Its middle is at y 75.
    HX_CHECK(CoverageIs(p, Vector2(50, 70), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(50, 80), 0, 0));
}

HX_TEST(Painter, StrokesEndInTheirCaps)
{
    Painter p;
    p.lineWidth = 10;
    p.MoveTo(Vector2(10, 50));
    p.LineTo(Vector2(90, 50));
    p.Stroke();
    HX_CHECK(CoverageIs(p, Vector2(50, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(50, 53), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(50, 58), 0, 0));
    // Butt: the line ends at its end.
    HX_CHECK(CoverageIs(p, Vector2(7, 50), 0, 0));

    p.Clear();
    p.lineCap = LineCap::Square;
    p.MoveTo(Vector2(10, 50));
    p.LineTo(Vector2(90, 50));
    p.Stroke();
    HX_CHECK(CoverageIs(p, Vector2(7, 50), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(7, 54), 0.999f, 1));
    HX_CHECK(CoverageIs(p, Vector2(2, 50), 0, 0));

    p.Clear();
    p.lineCap = LineCap::Round;
    p.MoveTo(Vector2(10, 50));
    p.LineTo(Vector2(90, 50));
    p.Stroke();
    HX_CHECK(CoverageIs(p, Vector2(7, 50), 0.999f, 1));
    // The square's corner is outside the circle.
    HX_CHECK(CoverageIs(p, Vector2(5.5f, 54.5f), 0, 0));
}

HX_TEST(Painter, JoinsMiterBevelOrRound)
{
    auto stroke = [](LineJoin join) {
        Painter p;
        p.lineWidth = 10;
        p.lineJoin = join;
        p.MoveTo(Vector2(10, 90));
        p.LineTo(Vector2(50, 10));
        p.LineTo(Vector2(90, 90));
        p.Stroke();
        return p;
    };
    // The miter's tip reaches 11.2 above the corner, the bevel's edge is 2.2 above it, the round one 5.
    Painter miter = stroke(LineJoin::Miter);
    HX_CHECK(CoverageIs(miter, Vector2(50, 4), 0.999f, 1));
    Painter bevel = stroke(LineJoin::Bevel);
    HX_CHECK(CoverageIs(bevel, Vector2(50, 9), 0.999f, 1));
    HX_CHECK(CoverageIs(bevel, Vector2(50, 5.5f), 0, 0));
    Painter round = stroke(LineJoin::Round);
    HX_CHECK(CoverageIs(round, Vector2(50, 5.5f), 0.999f, 1));
    HX_CHECK(CoverageIs(round, Vector2(50, 3), 0, 0));

    // Past the miter limit the miter becomes a bevel.
    Painter limited;
    limited.lineWidth = 10;
    limited.miterLimit = 2;
    limited.MoveTo(Vector2(10, 90));
    limited.LineTo(Vector2(50, 10));
    limited.LineTo(Vector2(90, 90));
    limited.Stroke();
    HX_CHECK(CoverageIs(limited, Vector2(50, 4), 0, 0));
}

HX_TEST(Painter, AGradientRunsAlongTheStrokeAndOpacityScalesIt)
{
    Painter p;
    p.lineWidth = 6;
    p.strokeGradient.Push({0, Color(1, 0, 0, 1)});
    p.strokeGradient.Push({1, Color(0, 0, 1, 1)});
    p.MoveTo(Vector2(0, 50));
    p.LineTo(Vector2(100, 50));
    p.Stroke();
    Color start = ColorAt(p, Vector2(5, 50)), end = ColorAt(p, Vector2(95, 50)), middle = ColorAt(p, Vector2(50, 50));
    HX_CHECK(start.r > 0.85f && start.b < 0.15f);
    HX_CHECK(end.b > 0.85f && end.r < 0.15f);
    HX_CHECK(std::fabs(middle.r - 0.5f) < 0.1f && std::fabs(middle.b - 0.5f) < 0.1f);

    Painter faded;
    faded.opacity = 0.5f;
    faded.fillColor = Color(1, 1, 1, 1);
    Square(faded, 0, 0, 10, 10, true);
    faded.Fill();
    Color c = ColorAt(faded, Vector2(5, 5));
    // Premultiplied.
    HX_CHECK(std::fabs(c.a - 0.5f) < 0.001f && std::fabs(c.r - 0.5f) < 0.001f);
}
