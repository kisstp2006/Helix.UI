#pragma once

#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>

#include <cmath>

// An affine map of the UI (y down, angles clockwise on the screen, z away from the viewer): x' = a x + c y + e z + tx,
// y' = b x + d y + f z + ty, z' = g x + h y + k z + tz. Elements lie in the plane z = 0 of their space: a 3D rotation
// moves their points off it (the depth a world space panel shows); on the panel they are where the map puts x and y
// (its view along z), so picking and clipping use the map's 2D part.
namespace helix::ui
{
    struct Affine
    {
        f32 a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
        f32 e = 0, f = 0, g = 0, h = 0, k = 1, tz = 0;

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

        // A turn about an axis through the origin (z: the screen's own rotation).
        static Affine Rotation(f32 degrees, Vector3 axis)
        {
            f32 len = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
            if (len == 0)
                return Affine();
            f32 x = axis.x / len, y = axis.y / len, z = axis.z / len;
            f32 r = degrees * 0.017453292519943295f;
            f32 cs = std::cos(r), sn = std::sin(r), t = 1 - cs;
            Affine m;
            m.a = t * x * x + cs, m.c = t * x * y - sn * z, m.e = t * x * z + sn * y;
            m.b = t * x * y + sn * z, m.d = t * y * y + cs, m.f = t * y * z - sn * x;
            m.g = t * x * z - sn * y, m.h = t * y * z + sn * x, m.k = t * z * z + cs;
            return m;
        }

        // No depth: points stay in the plane z = 0.
        bool Flat() const { return e == 0 && f == 0 && g == 0 && h == 0 && k == 1 && tz == 0; }
        bool IsIdentity() const { return a == 1 && b == 0 && c == 0 && d == 1 && tx == 0 && ty == 0 && Flat(); }
        // Only a translation and a scale on each axis (rectangles stay rectangles on the axes, at no depth).
        bool AxisAligned() const { return b == 0 && c == 0 && g == 0 && h == 0 && tz == 0; }

        Vector2 Apply(Vector2 p) const { return Vector2(a * p.x + c * p.y + tx, b * p.x + d * p.y + ty); }
        // The depth a point of the plane z = 0 gets.
        f32 ApplyZ(Vector2 p) const { return g * p.x + h * p.y + tz; }
        // This map after o: (this * o)(p) = this(o(p)).
        Affine operator*(const Affine& o) const
        {
            Affine m;
            m.a = a * o.a + c * o.b + e * o.g;
            m.b = b * o.a + d * o.b + f * o.g;
            m.g = g * o.a + h * o.b + k * o.g;
            m.c = a * o.c + c * o.d + e * o.h;
            m.d = b * o.c + d * o.d + f * o.h;
            m.h = g * o.c + h * o.d + k * o.h;
            m.e = a * o.e + c * o.f + e * o.k;
            m.f = b * o.e + d * o.f + f * o.k;
            m.k = g * o.e + h * o.f + k * o.k;
            m.tx = a * o.tx + c * o.ty + e * o.tz + tx;
            m.ty = b * o.tx + d * o.ty + f * o.tz + ty;
            m.tz = g * o.tx + h * o.ty + k * o.tz + tz;
            return m;
        }
        // The inverse of the 2D part: from the panel back to the plane of the element.
        Affine Inverse() const
        {
            f32 det = a * d - b * c;
            if (det == 0)
                return Affine{0, 0, 0, 0, 0, 0};
            f32 q = 1.0f / det;
            Affine m;
            m.a = d * q, m.b = -b * q, m.c = -c * q, m.d = a * q;
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
