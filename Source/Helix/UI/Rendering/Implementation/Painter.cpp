#include <Helix/UI/Rendering/Painter.h>

#include <cmath>
#include <utility>

namespace helix::ui
{
    namespace
    {
        constexpr f32 kPi = 3.14159265358979f;
        // How far a flattened curve may stray from the true one (pixels), and the antialiasing fringe.
        constexpr f32 kTolerance = 0.2f;
        constexpr f32 kFringe = 1.0f;

        Vector2 Add(Vector2 a, Vector2 b) { return Vector2(a.x + b.x, a.y + b.y); }
        Vector2 Sub(Vector2 a, Vector2 b) { return Vector2(a.x - b.x, a.y - b.y); }
        Vector2 Mul(Vector2 a, f32 s) { return Vector2(a.x * s, a.y * s); }
        f32 Dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
        f32 Cross(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }
        f32 Len(Vector2 a) { return std::sqrt(Dot(a, a)); }
        Vector2 Norm(Vector2 a)
        {
            f32 l = Len(a);
            return l > 1e-6f ? Mul(a, 1.0f / l) : Vector2(0, 0);
        }
        // The normal to the left of a direction on the screen (y down: rotated by -90 degrees).
        Vector2 Left(Vector2 d) { return Vector2(d.y, -d.x); }

        f32 Area(const Array<Vector2>& p)
        {
            f32 a = 0;
            for (usize i = 0, j = p.Size() - 1; i < p.Size(); j = i++)
                a += Cross(p[j], p[i]);
            return a * 0.5f;
        }

        bool InsidePolygon(const Array<Vector2>& poly, Vector2 p)
        {
            bool in = false;
            for (usize i = 0, j = poly.Size() - 1; i < poly.Size(); j = i++)
                if ((poly[i].y > p.y) != (poly[j].y > p.y) && p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
                    in = !in;
            return in;
        }

        bool InTriangle(Vector2 p, Vector2 a, Vector2 b, Vector2 c)
        {
            f32 d1 = Cross(Sub(b, a), Sub(p, a)), d2 = Cross(Sub(c, b), Sub(p, b)), d3 = Cross(Sub(a, c), Sub(p, c));
            bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
            return !(neg && pos);
        }

        // Ear clipping of a simple polygon of positive area: triangles as indices into it.
        void EarClip(const Array<Vector2>& poly, Array<u32>& out)
        {
            Array<u32> v;
            for (u32 i = 0; i < poly.Size(); i++)
                v.Push(i);
            u32 guard = 0;
            while (v.Size() > 3 && guard++ < 100000)
            {
                bool clipped = false;
                for (u32 i = 0; i < v.Size() && !clipped; i++)
                {
                    u32 a = v[(i + v.Size() - 1) % v.Size()], b = v[i], c = v[(i + 1) % v.Size()];
                    if (Cross(Sub(poly[b], poly[a]), Sub(poly[c], poly[b])) <= 0)
                        continue;
                    bool empty = true;
                    for (u32 k = 0; k < v.Size() && empty; k++)
                    {
                        u32 q = v[k];
                        if (q == a || q == b || q == c || (poly[q].x == poly[a].x && poly[q].y == poly[a].y) ||
                            (poly[q].x == poly[b].x && poly[q].y == poly[b].y) || (poly[q].x == poly[c].x && poly[q].y == poly[c].y))
                            continue;
                        empty = !InTriangle(poly[q], poly[a], poly[b], poly[c]);
                    }
                    if (!empty)
                        continue;
                    out.Push(a), out.Push(b), out.Push(c);
                    v.RemoveAt(i);
                    clipped = true;
                }
                // Degenerate input (collinear or touching points): clip the first convex vertex anyway.
                if (!clipped)
                {
                    out.Push(v[v.Size() - 1]), out.Push(v[0]), out.Push(v[1]);
                    v.RemoveAt(0);
                }
            }
            if (v.Size() == 3)
                out.Push(v[0]), out.Push(v[1]), out.Push(v[2]);
        }

        // A hole joined to its outline by a bridge from the hole's rightmost point to a point of the outline it sees.
        void Bridge(Array<Vector2>& outer, const Array<Vector2>& hole)
        {
            u32 m = 0;
            for (u32 i = 1; i < hole.Size(); i++)
                if (hole[i].x > hole[m].x)
                    m = i;
            Vector2 M = hole[m];
            // The nearest outline edge to the right of M, and its endpoint further right.
            f32 best = 1e30f;
            u32 bridge = 0;
            for (u32 i = 0; i < outer.Size(); i++)
            {
                Vector2 a = outer[i], b = outer[(i + 1) % outer.Size()];
                if ((a.y > M.y) == (b.y > M.y) && a.y != M.y)
                    continue;
                if (a.y == b.y)
                    continue;
                f32 x = a.x + (M.y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (x >= M.x && x - M.x < best)
                {
                    best = x - M.x;
                    bridge = a.x > b.x ? i : (i + 1) % u32(outer.Size());
                }
            }
            // A vertex inside the triangle M, hit point, bridge sees M better: the one with the smallest angle.
            Vector2 hit(M.x + best, M.y), P = outer[bridge];
            f32 bestAngle = 1e30f;
            for (u32 i = 0; i < outer.Size(); i++)
            {
                if (i == bridge || outer[i].x < M.x || !InTriangle(outer[i], M, hit, P))
                    continue;
                f32 angle = std::fabs(std::atan2(outer[i].y - M.y, outer[i].x - M.x));
                if (angle < bestAngle)
                {
                    bestAngle = angle;
                    bridge = i;
                }
            }
            Array<Vector2> merged;
            for (u32 i = 0; i <= bridge; i++)
                merged.Push(outer[i]);
            for (u32 k = 0; k <= hole.Size(); k++)
                merged.Push(hole[(m + k) % hole.Size()]);
            merged.Push(outer[bridge]);
            for (u32 i = bridge + 1; i < outer.Size(); i++)
                merged.Push(outer[i]);
            outer = Move(merged);
        }
    }

    void Painter::Clear()
    {
        positions_.Clear();
        colors_.Clear();
        indices_.Clear();
        BeginPath();
    }

    void Painter::BeginPath()
    {
        points_.Clear();
        subpaths_.Clear();
        hasPen_ = false;
    }

    void Painter::Point(Vector2 p)
    {
        if (subpaths_.Empty() || subpaths_.Back().closed)
        {
            Subpath s;
            s.first = u32(points_.Size());
            subpaths_.Push(s);
        }
        Subpath& s = subpaths_.Back();
        if (s.count > 0)
        {
            Vector2 last = points_.Back();
            if (std::fabs(last.x - p.x) < 1e-4f && std::fabs(last.y - p.y) < 1e-4f)
                return;
        }
        points_.Push(p);
        s.count++;
        pen_ = p;
        hasPen_ = true;
    }

    void Painter::MoveTo(Vector2 p)
    {
        Subpath s;
        s.first = u32(points_.Size());
        subpaths_.Push(s);
        Point(p);
    }

    void Painter::LineTo(Vector2 p)
    {
        if (!hasPen_)
            MoveTo(p);
        else
            Point(p);
    }

    void Painter::ClosePath()
    {
        if (!subpaths_.Empty() && subpaths_.Back().count > 0)
        {
            Subpath& s = subpaths_.Back();
            s.closed = true;
            pen_ = points_[s.first];
            // Drawing goes on from the subpath's start, in a new subpath.
            Subpath next;
            next.first = u32(points_.Size());
            subpaths_.Push(next);
            Point(pen_);
        }
    }

    void Painter::Arc(Vector2 center, f32 radius, f32 startAngle, f32 endAngle, ArcDirection direction)
    {
        f32 a0 = startAngle * kPi / 180.0f, a1 = endAngle * kPi / 180.0f;
        f32 sweep = a1 - a0;
        if (direction == ArcDirection::Clockwise)
            while (sweep < 0)
                sweep += 2 * kPi;
        else
            while (sweep > 0)
                sweep -= 2 * kPi;
        if (std::fabs(endAngle - startAngle) >= 360.0f)
            sweep = direction == ArcDirection::Clockwise ? 2 * kPi : -2 * kPi;
        f32 step = radius > kTolerance ? 2 * std::acos(1 - kTolerance / radius) : kPi / 4;
        u32 n = u32(std::ceil(std::fabs(sweep) / (step > 1e-3f ? step : 1e-3f)));
        n = n < 2 ? 2 : n > 512 ? 512 : n;
        for (u32 i = 0; i <= n; i++)
        {
            f32 a = a0 + sweep * f32(i) / f32(n);
            Vector2 p(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
            if (i == 0)
                LineTo(p);
            else
                Point(p);
        }
    }

    void Painter::ArcTo(Vector2 p1, Vector2 p2, f32 radius)
    {
        if (!hasPen_)
        {
            MoveTo(p1);
            return;
        }
        Vector2 d0 = Norm(Sub(pen_, p1)), d1 = Norm(Sub(p2, p1));
        f32 cosA = Dot(d0, d1);
        if (radius <= 0 || std::fabs(Cross(d0, d1)) < 1e-6f)
        {
            LineTo(p1);
            return;
        }
        // The tangent points at the distance radius / tan(half angle) from the corner.
        f32 half = std::acos(cosA < -1 ? -1 : cosA > 1 ? 1 : cosA) * 0.5f;
        f32 dist = radius / std::tan(half);
        Vector2 t0 = Add(p1, Mul(d0, dist)), t1 = Add(p1, Mul(d1, dist));
        Vector2 bisector = Norm(Add(d0, d1));
        Vector2 center = Add(p1, Mul(bisector, radius / std::sin(half)));
        f32 a0 = std::atan2(t0.y - center.y, t0.x - center.x) * 180.0f / kPi, a1 = std::atan2(t1.y - center.y, t1.x - center.x) * 180.0f / kPi;
        // The short way round from t0 to t1.
        bool clockwise = Cross(Sub(t0, center), Sub(t1, center)) > 0;
        LineTo(t0);
        Arc(center, radius, a0, a1, clockwise ? ArcDirection::Clockwise : ArcDirection::CounterClockwise);
    }

    void Painter::QuadraticCurveTo(Vector2 c, Vector2 p)
    {
        Vector2 p0 = hasPen_ ? pen_ : c;
        if (!hasPen_)
            MoveTo(c);
        f32 legs = Len(Sub(c, p0)) + Len(Sub(p, c));
        u32 n = u32(std::ceil(std::sqrt(legs / kTolerance) * 0.5f));
        n = n < 1 ? 1 : n > 256 ? 256 : n;
        for (u32 i = 1; i <= n; i++)
        {
            f32 t = f32(i) / f32(n), u = 1 - t;
            Point(Vector2(u * u * p0.x + 2 * u * t * c.x + t * t * p.x, u * u * p0.y + 2 * u * t * c.y + t * t * p.y));
        }
    }

    void Painter::BezierCurveTo(Vector2 c1, Vector2 c2, Vector2 p)
    {
        Vector2 p0 = hasPen_ ? pen_ : c1;
        if (!hasPen_)
            MoveTo(c1);
        f32 legs = Len(Sub(c1, p0)) + Len(Sub(c2, c1)) + Len(Sub(p, c2));
        u32 n = u32(std::ceil(std::sqrt(legs / kTolerance) * 0.5f));
        n = n < 1 ? 1 : n > 256 ? 256 : n;
        for (u32 i = 1; i <= n; i++)
        {
            f32 t = f32(i) / f32(n), u = 1 - t;
            f32 b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
            Point(Vector2(b0 * p0.x + b1 * c1.x + b2 * c2.x + b3 * p.x, b0 * p0.y + b1 * c1.y + b2 * c2.y + b3 * p.y));
        }
    }

    Color Painter::Premultiplied(Color c, f32 alpha) const
    {
        f32 a = c.a * opacity * alpha;
        return Color(c.r * a, c.g * a, c.b * a, a);
    }

    u32 Painter::Vertex(Vector2 p, Color c)
    {
        positions_.Push(p);
        colors_.Push(c);
        return u32(positions_.Size() - 1);
    }

    Color Painter::StrokeColorAt(f32 t) const
    {
        if (strokeGradient.Empty())
            return strokeColor;
        const GradientKey* prev = &strokeGradient[0];
        if (t <= prev->time)
            return prev->color;
        for (const GradientKey& k : strokeGradient)
        {
            if (t <= k.time)
            {
                f32 f = k.time > prev->time ? (t - prev->time) / (k.time - prev->time) : 1;
                return Color(prev->color.r + (k.color.r - prev->color.r) * f, prev->color.g + (k.color.g - prev->color.g) * f,
                             prev->color.b + (k.color.b - prev->color.b) * f, prev->color.a + (k.color.a - prev->color.a) * f);
            }
            prev = &k;
        }
        return strokeGradient.Back().color;
    }

    void Painter::FillPolygon(const Array<Array<Vector2>>& outers, const Array<Array<Array<Vector2>>>& holes, Color color)
    {
        Color solid = Premultiplied(color, 1), clear(0, 0, 0, 0);
        for (u32 o = 0; o < outers.Size(); o++)
        {
            Array<Vector2> poly = outers[o];
            if (Area(poly) < 0)
                for (u32 i = 0, j = u32(poly.Size()) - 1; i < j; i++, j--)
                    std::swap(poly[i], poly[j]);
            // Holes the other way round, rightmost first.
            Array<Array<Vector2>> hs = holes[o];
            for (Array<Vector2>& h : hs)
                if (Area(h) > 0)
                    for (u32 i = 0, j = u32(h.Size()) - 1; i < j; i++, j--)
                        std::swap(h[i], h[j]);
            for (u32 i = 1; i < hs.Size(); i++)
                for (u32 j = i; j > 0; j--)
                {
                    f32 a = -1e30f, b = -1e30f;
                    for (Vector2 p : hs[j - 1])
                        a = p.x > a ? p.x : a;
                    for (Vector2 p : hs[j])
                        b = p.x > b ? p.x : b;
                    if (b > a)
                        std::swap(hs[j - 1], hs[j]);
                }
            // The fringes along the outline and the holes, outwards from the filled area.
            auto fringe = [&](const Array<Vector2>& ring) {
                u32 n = u32(ring.Size());
                for (u32 i = 0; i < n; i++)
                {
                    Vector2 a = ring[i], b = ring[(i + 1) % n];
                    // Outlines run with positive area, holes the other way: the fill is on the same side of both.
                    Vector2 out = Mul(Left(Norm(Sub(b, a))), kFringe);
                    u32 base = Vertex(a, solid);
                    Vertex(b, solid);
                    Vertex(Add(b, out), clear);
                    Vertex(Add(a, out), clear);
                    for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u})
                        indices_.Push(base + k);
                }
            };
            fringe(poly);
            for (const Array<Vector2>& h : hs)
                fringe(h);
            for (const Array<Vector2>& h : hs)
                if (h.Size() >= 3)
                    Bridge(poly, h);
            Array<u32> tris;
            EarClip(poly, tris);
            u32 base = u32(positions_.Size());
            for (Vector2 p : poly)
                Vertex(p, solid);
            for (u32 t : tris)
                indices_.Push(base + t);
        }
    }

    void Painter::Fill(FillRule rule)
    {
        // The subpaths as closed contours, their nesting depth from the others.
        Array<Array<Vector2>> contours;
        for (const Subpath& s : subpaths_)
            if (s.count >= 3)
            {
                Array<Vector2> c;
                for (u32 i = 0; i < s.count; i++)
                    c.Push(points_[s.first + i]);
                contours.Push(Move(c));
            }
        u32 n = u32(contours.Size());
        Array<u32> depth, parent;
        depth.Resize(n);
        parent.Resize(n);
        for (u32 i = 0; i < n; i++)
        {
            depth[i] = 0;
            parent[i] = ~0u;
            f32 smallest = 1e30f;
            for (u32 j = 0; j < n; j++)
                if (i != j && InsidePolygon(contours[j], contours[i][0]))
                {
                    depth[i]++;
                    f32 a = std::fabs(Area(contours[j]));
                    if (a < smallest)
                    {
                        smallest = a;
                        parent[i] = j;
                    }
                }
        }
        // Even odd: odd depths are holes of their parent. Non zero: a contour inside one of the other direction is
        // its hole; one of the same direction is filled on its own.
        auto isHole = [&](u32 i) {
            if (parent[i] == ~0u)
                return false;
            if (rule == FillRule::OddEven)
                return (depth[i] & 1) != 0;
            return (Area(contours[i]) > 0) != (Area(contours[parent[i]]) > 0);
        };
        Array<Array<Vector2>> outers;
        Array<Array<Array<Vector2>>> holes;
        Array<u32> outerOf;
        outerOf.Resize(n);
        for (u32 i = 0; i < n; i++)
            if (!isHole(i))
            {
                outerOf[i] = u32(outers.Size());
                outers.Push(contours[i]);
                holes.Emplace();
            }
        for (u32 i = 0; i < n; i++)
            if (isHole(i) && !isHole(parent[i]))
                holes[outerOf[parent[i]]].Push(contours[i]);
        FillPolygon(outers, holes, fillColor);
    }

    void Painter::StrokePolyline(Span<const Vector2> pts, bool closed, f32 startLength, f32 totalLength)
    {
        u32 n = u32(pts.Size());
        if (n < 2)
            return;
        f32 half = lineWidth * 0.5f;
        Color clear(0, 0, 0, 0);
        auto colorAt = [&](f32 length) { return Premultiplied(StrokeColorAt(totalLength > 0 ? length / totalLength : 0), 1); };
        // A fringe along an outer edge a-b (out: the outward normal) with the edge's colors.
        auto edge = [&](Vector2 a, Vector2 b, Vector2 out, Color ca, Color cb) {
            u32 base = Vertex(a, ca);
            Vertex(b, cb);
            Vertex(Add(b, Mul(out, kFringe)), clear);
            Vertex(Add(a, Mul(out, kFringe)), clear);
            for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u})
                indices_.Push(base + k);
        };
        auto fan = [&](Vector2 center, Vector2 from, f32 angle, f32 sweep, Color c) {
            u32 steps = u32(std::ceil(std::fabs(sweep) / (half > kTolerance ? 2 * std::acos(1 - kTolerance / half) : kPi / 4)));
            steps = steps < 1 ? 1 : steps > 128 ? 128 : steps;
            (void)from;
            Vector2 prev(center.x + std::cos(angle) * half, center.y + std::sin(angle) * half);
            for (u32 i = 1; i <= steps; i++)
            {
                f32 a = angle + sweep * f32(i) / f32(steps);
                Vector2 p(center.x + std::cos(a) * half, center.y + std::sin(a) * half);
                u32 base = Vertex(center, c);
                Vertex(prev, c);
                Vertex(p, c);
                indices_.Push(base), indices_.Push(base + 1), indices_.Push(base + 2);
                edge(prev, p, Norm(Sub(Mul(Add(prev, p), 0.5f), center)), c, c);
                prev = p;
            }
        };
        u32 segments = closed ? n : n - 1;
        f32 length = startLength;
        for (u32 i = 0; i < segments; i++)
        {
            Vector2 a = pts[i], b = pts[(i + 1) % n];
            Vector2 d = Norm(Sub(b, a));
            f32 seg = Len(Sub(b, a));
            Color ca = colorAt(length), cb = colorAt(length + seg);
            // Square caps reach half the width past the ends.
            if (!closed && lineCap == LineCap::Square)
            {
                if (i == 0)
                    a = Sub(a, Mul(d, half));
                if (i == segments - 1)
                    b = Add(b, Mul(d, half));
            }
            Vector2 nrm = Mul(Left(d), half);
            Vector2 a0 = Add(a, nrm), a1 = Sub(a, nrm), b0 = Add(b, nrm), b1 = Sub(b, nrm);
            u32 base = Vertex(a0, ca);
            Vertex(b0, cb);
            Vertex(b1, cb);
            Vertex(a1, ca);
            for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u})
                indices_.Push(base + k);
            Vector2 out = Left(d);
            edge(a0, b0, out, ca, cb);
            edge(b1, a1, Mul(out, -1), cb, ca);
            // The ends of an open line: butt and square ends get fringes, round ones half circles.
            if (!closed && (i == 0 || i == segments - 1))
                for (int end = 0; end < 2; end++)
                {
                    bool first = end == 0;
                    if ((first && i != 0) || (!first && i != segments - 1))
                        continue;
                    Vector2 p = first ? a : b;
                    Color c = first ? ca : cb;
                    Vector2 back = first ? Mul(d, -1) : d;
                    if (lineCap == LineCap::Round)
                    {
                        f32 angle = std::atan2(back.y, back.x) - kPi * 0.5f;
                        fan(p, p, angle, kPi, c);
                    }
                    else
                    {
                        Vector2 l = Add(p, Mul(Left(d), half)), r = Sub(p, Mul(Left(d), half));
                        edge(first ? r : l, first ? l : r, back, c, c);
                    }
                }
            length += seg;
            // The join with the next segment.
            if (i + 1 < segments || closed)
            {
                Vector2 c2 = pts[(i + 2) % n];
                Vector2 d2 = Norm(Sub(c2, b));
                f32 turn = Cross(d, d2);
                if (std::fabs(turn) < 1e-5f && Dot(d, d2) > 0)
                    continue;
                // The outer side of the turn.
                f32 sign = turn > 0 ? 1.0f : -1.0f;
                Vector2 o1 = Add(b, Mul(Left(d), half * sign)), o2 = Add(b, Mul(Left(d2), half * sign));
                Color cj = colorAt(length);
                if (lineJoin == LineJoin::Round)
                {
                    f32 start = std::atan2(o1.y - b.y, o1.x - b.x), end = std::atan2(o2.y - b.y, o2.x - b.x);
                    f32 sweep = end - start;
                    while (sweep > kPi)
                        sweep -= 2 * kPi;
                    while (sweep < -kPi)
                        sweep += 2 * kPi;
                    fan(b, o1, start, sweep, cj);
                    continue;
                }
                Vector2 bis = Norm(Add(Left(d), Left(d2)));
                f32 cosHalf = Dot(bis, Left(d));
                f32 miterLength = cosHalf > 1e-4f ? 1.0f / cosHalf : 1e9f;
                if (lineJoin == LineJoin::Miter && miterLength <= miterLimit)
                {
                    Vector2 m = Add(b, Mul(bis, half * miterLength * sign));
                    u32 j = Vertex(b, cj);
                    Vertex(o1, cj);
                    Vertex(m, cj);
                    Vertex(o2, cj);
                    for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u})
                        indices_.Push(j + k);
                    edge(o1, m, Norm(Sub(m, b)), cj, cj);
                    edge(m, o2, Norm(Sub(m, b)), cj, cj);
                }
                else
                {
                    u32 j = Vertex(b, cj);
                    Vertex(o1, cj);
                    Vertex(o2, cj);
                    indices_.Push(j), indices_.Push(j + 1), indices_.Push(j + 2);
                    edge(o1, o2, Mul(bis, sign), cj, cj);
                }
            }
        }
    }

    void Painter::Stroke()
    {
        f32 total = 0;
        for (const Subpath& s : subpaths_)
        {
            for (u32 i = 1; i < s.count; i++)
                total += Len(Sub(points_[s.first + i], points_[s.first + i - 1]));
            if (s.closed && s.count > 2)
                total += Len(Sub(points_[s.first], points_[s.first + s.count - 1]));
        }
        f32 length = 0;
        for (const Subpath& s : subpaths_)
        {
            if (s.count < 2)
                continue;
            StrokePolyline(Span<const Vector2>(points_.Data() + s.first, s.count), s.closed, length, total);
            for (u32 i = 1; i < s.count; i++)
                length += Len(Sub(points_[s.first + i], points_[s.first + i - 1]));
            if (s.closed)
                length += Len(Sub(points_[s.first], points_[s.first + s.count - 1]));
        }
    }
}
