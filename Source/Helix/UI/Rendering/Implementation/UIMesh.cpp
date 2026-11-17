#include <Helix/UI/Rendering/UIMesh.h>

#include <cmath>

namespace helix::ui
{
    namespace
    {
        Color Premultiply(Color c, f32 opacity)
        {
            f32 a = c.a * opacity;
            return Color(c.r * a, c.g * a, c.b * a, a);
        }

        f32 Max0(f32 v) { return v > 0 ? v : 0; }

        f32 Resolve(const Length& l, f32 size) { return l.unit == Length::Unit::Percent ? l.value * size / 100.0f : l.unit == Length::Unit::Pixel ? l.value : 0.0f; }
    }

    UIClip UIClip::Within(const Rect& inner, const Vector4& innerRadiiX, const Vector4& innerRadiiY, const Affine& frame) const
    {
        // This clip's rectangle in the inner one's space.
        Affine toInner = frame.Inverse();
        Rect o = (toInner * toClip.Inverse()).Bounds(rect);
        f32 x0 = o.x > inner.x ? o.x : inner.x;
        f32 y0 = o.y > inner.y ? o.y : inner.y;
        f32 x1 = o.x + o.width < inner.x + inner.width ? o.x + o.width : inner.x + inner.width;
        f32 y1 = o.y + o.height < inner.y + inner.height ? o.y + o.height : inner.y + inner.height;
        UIClip c;
        c.rect = Rect(x0, y0, Max0(x1 - x0), Max0(y1 - y0));
        c.toClip = toInner;
        auto largest = [](const Vector4& v) {
            f32 r = v.x;
            for (f32 x : {v.y, v.z, v.w})
                r = x > r ? x : r;
            return r;
        };
        c.radius = largest(innerRadiiX);
        c.radiusY = largest(innerRadiiY);
        return c;
    }

    void UIMesh::Clear()
    {
        vertices_.Clear();
        indices_.Clear();
        batches_.Clear();
        clip_ = UIClip();
        SetTransform(Affine());
    }

    void UIMesh::SetTransform(const Affine& transform)
    {
        transform_ = transform;
        identity_ = transform.IsIdentity();
        scale_ = identity_ ? 1.0f : transform.Scale();
    }

    void UIMesh::Push(UIVertex v)
    {
        if (!identity_)
        {
            v.z = transform_.ApplyZ(v.position);
            v.position = transform_.Apply(v.position);
            u32 kind = u32(v.parameters.x + 0.5f);
            if (kind >= u32(UIVertexKind::ShapeFill))
            {
                // A shape's distances in pixels: its point, size, radii and borders scaled.
                v.uvLocal.z *= scale_, v.uvLocal.w *= scale_;
                v.size.x *= scale_, v.size.y *= scale_;
                v.radii = Vector4(v.radii.x * scale_, v.radii.y * scale_, v.radii.z * scale_, v.radii.w * scale_);
                v.borders = Vector4(v.borders.x * scale_, v.borders.y * scale_, v.borders.z * scale_, v.borders.w * scale_);
            }
            else if (kind == u32(UIVertexKind::GlyphSdf) || kind == u32(UIVertexKind::GlyphMsdf))
                v.parameters.z *= scale_;
        }
        // Its place in the clip, in pixels (the clip's space is the clipping element's; the place is affine in the
        // screen position, so the shader's interpolation keeps it exact).
        Vector2 q = clip_.toClip.Apply(v.position);
        f32 k = clip_.toClip.IsIdentity() ? 1.0f : 1.0f / clip_.toClip.Scale();
        const Rect& r = clip_.rect;
        v.clipRect = Vector4((q.x - r.x) * k, (q.y - r.y) * k, r.width * k, r.height * k);
        v.size.z = clip_.radius * k;
        v.size.w = clip_.radiusY * k;
        vertices_.Push(v);
    }

    f32 UIMesh::Slot(const void* texture, u32 indexCount)
    {
        u32 slot = 0;
        bool found = !batches_.Empty();
        if (found && texture)
        {
            UIBatch& b = batches_.Back();
            found = false;
            for (u32 k = 0; k < UIBatch::kSlots && !found; k++)
                if (b.textures[k] == texture || !b.textures[k])
                {
                    b.textures[k] = texture;
                    slot = k;
                    found = true;
                }
        }
        if (!found)
        {
            UIBatch b;
            b.firstIndex = u32(indices_.Size());
            b.textures[0] = texture;
            batches_.Push(b);
            slot = 0;
        }
        batches_.Back().indexCount += indexCount;
        return f32(slot);
    }

    UIVertex UIMesh::Base(UIVertexKind kind) const
    {
        UIVertex v{};
        v.parameters = Vector4(f32(u8(kind)), 0, 0, 0);
        return v;
    }

    void UIMesh::QuadIndices(u32 base)
    {
        for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u})
            indices_.Push(base + k);
    }

    void UIMesh::Shape(const UIShape& s, f32 opacity)
    {
        const Rect& r = s.rect;
        if (r.width <= 0 || r.height <= 0)
            return;
        auto quad = [&](UIVertexKind kind, Color color, f32 side) {
            if (color.a <= 0)
                return;
            f32 slot = Slot(nullptr, 6);
            UIVertex v = Base(kind);
            v.color = Premultiply(color, opacity);
            v.parameters.y = side;
            v.parameters.w = slot;
            v.size = Vector4(r.width, r.height, clip_.radius, 0);
            v.radii = s.radii;
            v.borders = Vector4(s.borders[0], s.borders[1], s.borders[2], s.borders[3]);
            u32 base = u32(vertices_.Size());
            const Vector2 local[4] = {Vector2(0, 0), Vector2(r.width, 0), Vector2(r.width, r.height), Vector2(0, r.height)};
            for (const Vector2& l : local)
            {
                v.position = Vector2(r.x + l.x, r.y + l.y);
                v.uvLocal = Vector4(0, 0, l.x, l.y);
                Push(v);
            }
            QuadIndices(base);
        };
        quad(UIVertexKind::ShapeFill, s.fill, 0);
        if (s.borders[0] <= 0 && s.borders[1] <= 0 && s.borders[2] <= 0 && s.borders[3] <= 0)
            return;
        bool same = true;
        for (u32 k = 1; k < 4; k++)
            same = same && s.borderColors[k] == s.borderColors[0];
        // Sides without width draw nothing; one color draws the whole ring at once.
        if (same)
            quad(UIVertexKind::ShapeBorder, s.borderColors[0], -1);
        else
            for (u32 k = 0; k < 4; k++)
                if (s.borders[k] > 0)
                    quad(UIVertexKind::ShapeBorder, s.borderColors[k], f32(k));
    }

    void UIMesh::Image(const Rect& rect, const UIImage& image, const Vector4& shapeRadii, const f32 (&borders)[4], f32 opacity)
    {
        if (!image.texture || rect.width <= 0 || rect.height <= 0)
            return;
        // The area inside the borders.
        Rect area(rect.x + borders[0], rect.y + borders[1], Max0(rect.width - borders[0] - borders[2]), Max0(rect.height - borders[1] - borders[3]));
        if (area.width <= 0 || area.height <= 0)
            return;
        Color tint = Premultiply(image.tint, opacity);
        if (tint.a <= 0)
            return;
        auto quad = [&](const Rect& to, const Rect& uv) {
            if (to.width <= 0 || to.height <= 0)
                return;
            f32 slot = Slot(image.texture, 6);
            UIVertex v = Base(UIVertexKind::ShapeImage);
            v.color = tint;
            v.parameters.w = slot;
            v.size = Vector4(rect.width, rect.height, clip_.radius, 0);
            v.radii = shapeRadii;
            v.borders = Vector4(borders[0], borders[1], borders[2], borders[3]);
            u32 base = u32(vertices_.Size());
            const f32 xs[4] = {to.x, to.x + to.width, to.x + to.width, to.x};
            const f32 ys[4] = {to.y, to.y, to.y + to.height, to.y + to.height};
            const f32 us[4] = {uv.x, uv.x + uv.width, uv.x + uv.width, uv.x};
            const f32 vs[4] = {uv.y, uv.y, uv.y + uv.height, uv.y + uv.height};
            for (u32 k = 0; k < 4; k++)
            {
                v.position = Vector2(xs[k], ys[k]);
                v.uvLocal = Vector4(us[k], image.rowsUp ? 1 - vs[k] : vs[k], xs[k] - rect.x, ys[k] - rect.y);
                Push(v);
            }
            QuadIndices(base);
        };
        bool sliced = image.slices[0] > 0 || image.slices[1] > 0 || image.slices[2] > 0 || image.slices[3] > 0;
        if (sliced)
        {
            // Nine parts: the corners keep their size, the edges stretch along, the middle both ways.
            f32 s = image.sliceScale > 0 ? image.sliceScale : 1;
            f32 dl = image.slices[0] * s, dt = image.slices[1] * s, dr = image.slices[2] * s, db = image.slices[3] * s;
            // Slices larger than the area shrink together.
            f32 fx = dl + dr > area.width ? area.width / (dl + dr) : 1, fy = dt + db > area.height ? area.height / (dt + db) : 1;
            dl *= fx, dr *= fx, dt *= fy, db *= fy;
            f32 ul = image.slices[0] / image.textureWidth * image.uv.width, ur = image.slices[2] / image.textureWidth * image.uv.width;
            f32 vt = image.slices[1] / image.textureHeight * image.uv.height, vb = image.slices[3] / image.textureHeight * image.uv.height;
            const f32 xs[4] = {area.x, area.x + dl, area.x + area.width - dr, area.x + area.width};
            const f32 ys[4] = {area.y, area.y + dt, area.y + area.height - db, area.y + area.height};
            const f32 us[4] = {image.uv.x, image.uv.x + ul, image.uv.x + image.uv.width - ur, image.uv.x + image.uv.width};
            const f32 vs[4] = {image.uv.y, image.uv.y + vt, image.uv.y + image.uv.height - vb, image.uv.y + image.uv.height};
            for (u32 j = 0; j < 3; j++)
                for (u32 i = 0; i < 3; i++)
                    quad(Rect(xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j]), Rect(us[i], vs[j], us[i + 1] - us[i], vs[j + 1] - vs[j]));
            return;
        }
        // The image's size: lengths (auto keeps the texture's aspect), or covering or fitting the area.
        f32 tw = image.textureWidth * image.uv.width, th = image.textureHeight * image.uv.height;
        if (tw <= 0 || th <= 0)
            return;
        f32 w = tw, h = th;
        if (image.sizeMode == UISizeMode::Cover || image.sizeMode == UISizeMode::Contain)
        {
            f32 kx = area.width / tw, ky = area.height / th;
            f32 k = image.sizeMode == UISizeMode::Cover ? (kx > ky ? kx : ky) : (kx < ky ? kx : ky);
            w = tw * k, h = th * k;
        }
        else
        {
            bool autoX = image.size[0].IsAuto(), autoY = image.size[1].IsAuto();
            f32 rx = Resolve(image.size[0], area.width), ry = Resolve(image.size[1], area.height);
            if (!autoX && !autoY)
                w = rx, h = ry;
            else if (!autoX)
                w = rx, h = rx * th / tw;
            else if (!autoY)
                h = ry, w = ry * tw / th;
        }
        // Round: as many whole tiles as fit best, stretched to fill the area.
        f32 sizes[2] = {w, h};
        const f32 extents[2] = {area.width, area.height};
        for (u32 axis = 0; axis < 2; axis++)
            if (image.repeat[axis] == UIRepeat::Round && sizes[axis] > 0)
            {
                f32 n = f32(i32(extents[axis] / sizes[axis] + 0.5f));
                sizes[axis] = extents[axis] / (n < 1 ? 1 : n);
            }
        w = sizes[0], h = sizes[1];
        if (w < 0.25f || h < 0.25f)
            return;
        // The tiles' places per axis: the position (an offset from an edge, percentages of the free room), then
        // the repetition.
        const f32 starts[2] = {area.x, area.y};
        Array<f32> places[2];
        for (u32 axis = 0; axis < 2; axis++)
        {
            f32 size = sizes[axis], extent = extents[axis], start = starts[axis];
            f32 free = extent - size;
            f32 offset = Resolve(image.offset[axis], free);
            f32 at = image.edge[axis] == UIEdge::Start ? offset : image.edge[axis] == UIEdge::Center ? free * 0.5f + offset : free - offset;
            Array<f32>& out = places[axis];
            switch (image.repeat[axis])
            {
            case UIRepeat::Repeat:
            case UIRepeat::Round:
            {
                f32 first = image.repeat[axis] == UIRepeat::Round ? 0.0f : at - size * std::ceil(at / size);
                for (f32 x = first; x < extent && out.Size() < 1024; x += size)
                    if (x + size > 0)
                        out.Push(start + x);
                break;
            }
            case UIRepeat::Space:
            {
                u32 n = u32(extent / size);
                if (n >= 2)
                {
                    f32 gap = (extent - f32(n) * size) / f32(n - 1);
                    for (u32 k = 0; k < n && k < 1024; k++)
                        out.Push(start + f32(k) * (size + gap));
                }
                else
                    out.Push(start + at);
                break;
            }
            default: out.Push(start + at); break;
            }
        }
        // Each tile cut to the area (its uv cut with it).
        for (f32 y : places[1])
            for (f32 x : places[0])
            {
                f32 x0 = x > area.x ? x : area.x, y0 = y > area.y ? y : area.y;
                f32 x1 = x + w < area.x + area.width ? x + w : area.x + area.width;
                f32 y1 = y + h < area.y + area.height ? y + h : area.y + area.height;
                if (x1 <= x0 || y1 <= y0)
                    continue;
                Rect uv(image.uv.x + (x0 - x) / w * image.uv.width, image.uv.y + (y0 - y) / h * image.uv.height, (x1 - x0) / w * image.uv.width,
                        (y1 - y0) / h * image.uv.height);
                quad(Rect(x0, y0, x1 - x0, y1 - y0), uv);
            }
    }

    void UIMesh::TexturedQuad(const Vector2 (&corners)[4], const Rect& uv, Color color, const void* texture, UIVertexKind kind, f32 edge, f32 scale)
    {
        if (color.a <= 0)
            return;
        f32 slot = Slot(texture, 6);
        UIVertex v = Base(kind);
        v.color = color;
        v.parameters = Vector4(f32(u8(kind)), edge, scale, slot);
        const f32 us[4] = {uv.x, uv.x + uv.width, uv.x + uv.width, uv.x};
        const f32 vs[4] = {uv.y, uv.y, uv.y + uv.height, uv.y + uv.height};
        u32 base = u32(vertices_.Size());
        for (u32 k = 0; k < 4; k++)
        {
            v.position = corners[k];
            v.uvLocal = Vector4(us[k], vs[k], 0, 0);
            Push(v);
        }
        QuadIndices(base);
    }

    void UIMesh::Rectangle(const Rect& rect, Color color)
    {
        if (rect.width <= 0 || rect.height <= 0 || color.a <= 0)
            return;
        const Vector2 corners[4] = {Vector2(rect.x, rect.y), Vector2(rect.x + rect.width, rect.y), Vector2(rect.x + rect.width, rect.y + rect.height),
                                    Vector2(rect.x, rect.y + rect.height)};
        TexturedQuad(corners, Rect(), color, nullptr, UIVertexKind::Flat, 0, 0);
    }

    void UIMesh::Triangles(Span<const Vector2> positions, Span<const Color> colors, Span<const u32> indices)
    {
        if (indices.Empty())
            return;
        f32 slot = Slot(nullptr, u32(indices.Size()));
        u32 base = u32(vertices_.Size());
        UIVertex v = Base(UIVertexKind::Flat);
        v.parameters.w = slot;
        for (usize i = 0; i < positions.Size(); i++)
        {
            v.position = positions[i];
            v.color = colors[i];
            Push(v);
        }
        for (u32 i : indices)
            indices_.Push(base + i);
    }
}
