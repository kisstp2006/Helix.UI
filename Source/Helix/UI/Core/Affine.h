#pragma once

#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>

#include <cmath>

// A 2D affine map of the UI (y down, angles clockwise on the screen): x' = a x + c y + tx, y' = b x + d y + ty.
namespace helix::ui
{
    struct Affine
    {
        f32 a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;

        static Affine Translation(Vector2 t)
        {
            Affine m;
            m.tx = t.x, m.ty = t.y;
            return m;
        }
        static Affine Scaling(Vector2 s)
        {
            Affine m;
            m.a = s.x, m.d = s.y;
            return m;
        }
        static Affine Rotation(f32 degrees)
        {
            f32 r = degrees * 0.017453292519943295f;
            f32 cs = std::cos(r), sn = std::sin(r);
            Affine m;
            m.a = cs, m.b = sn, m.c = -sn, m.d = cs;
            return m;
        }

        bool IsIdentity() const { return a == 1 && b == 0 && c == 0 && d == 1 && tx == 0 && ty == 0; }
        // Only a translation and a scale on each axis (rectangles stay rectangles on the axes).
        bool AxisAligned() const { return b == 0 && c == 0; }

        Vector2 Apply(Vector2 p) const { return Vector2(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty); }
        // This map after o: (this * o)(p) = this(o(p)).
        Affine operator*(const Affine& o) const
        {
            Affine m;
            m.a = a * o.a + c * o.b;
            m.b = b * o.a + d * o.b;
            m.c = a * o.c + c * o.d;
            m.d = b * o.c + d * o.d;
            m.tx = a * o.tx + c * o.ty + tx;
            m.ty = b * o.tx + d * o.ty + ty;
            return m;
        }
        Affine Inverse() const
        {
            f32 det = a * d - b * c;
            if (det == 0)
                return Affine{0, 0, 0, 0, 0, 0};
            f32 k = 1.0f / det;
            Affine m;
            m.a = d * k, m.b = -b * k, m.c = -c * k, m.d = a * k;
            m.tx = -(m.a * tx + m.c * ty);
            m.ty = -(m.b * tx + m.d * ty);
            return m;
        }
        // How many screen pixels a unit of the mapped space spans (the mean of the axes' scales).
        f32 Scale() const { return std::sqrt(std::fabs(a * d - b * c)); }
        // The bounds of a rectangle's mapped corners.
        Rect Bounds(const Rect& r) const
        {
            const Vector2 p[4] = {Apply(Vector2(r.x, r.y)), Apply(Vector2(r.x + r.width, r.y)), Apply(Vector2(r.x + r.width, r.y + r.height)),
                                  Apply(Vector2(r.x, r.y + r.height))};
            f32 x0 = p[0].x, y0 = p[0].y, x1 = p[0].x, y1 = p[0].y;
            for (const Vector2& q : p)
            {
                x0 = q.x < x0 ? q.x : x0, y0 = q.y < y0 ? q.y : y0;
                x1 = q.x > x1 ? q.x : x1, y1 = q.y > y1 ? q.y : y1;
            }
            return Rect(x0, y0, x1 - x0, y1 - y0);
        }
    };
}
