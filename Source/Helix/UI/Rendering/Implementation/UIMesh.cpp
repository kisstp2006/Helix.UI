#include <Helix/UI/Rendering/UIMesh.h>

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
    }

    UIClip UIClip::Within(const Rect& inner, const Vector4& innerRadii) const
    {
        f32 x0 = rect.x > inner.x ? rect.x : inner.x;
        f32 y0 = rect.y > inner.y ? rect.y : inner.y;
        f32 x1 = rect.x + rect.width < inner.x + inner.width ? rect.x + rect.width : inner.x + inner.width;
        f32 y1 = rect.y + rect.height < inner.y + inner.height ? rect.y + rect.height : inner.y + inner.height;
        UIClip c;
        c.rect = Rect(x0, y0, Max0(x1 - x0), Max0(y1 - y0));
        f32 r = innerRadii.x;
        for (f32 v : {innerRadii.y, innerRadii.z, innerRadii.w})
            r = v > r ? v : r;
        c.radius = r;
        return c;
    }

    void UIMesh::Clear()
    {
        vertices_.Clear();
        indices_.Clear();
        batches_.Clear();
        clip_ = UIClip();
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
        const Rect& r = clip_.rect;
        v.clipRect = Vector4(r.x, r.y, r.x + r.width, r.y + r.height);
        v.size = Vector4(0, 0, clip_.radius, 0);
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
        f32 aspect = image.textureHeight > 0 ? image.textureWidth * image.uv.width / (image.textureHeight * image.uv.height) : 1;
        f32 areaAspect = area.width / area.height;
        switch (image.scaleMode)
        {
        case UIScaleMode::ScaleToFit:
        {
            // The whole image, as large as fits, centered.
            f32 w = areaAspect > aspect ? area.height * aspect : area.width, h = areaAspect > aspect ? area.height : area.width / aspect;
            quad(Rect(area.x + (area.width - w) * 0.5f, area.y + (area.height - h) * 0.5f, w, h), image.uv);
            break;
        }
        case UIScaleMode::ScaleAndCrop:
        {
            // The area filled, the image's middle cut to its shape.
            Rect uv = image.uv;
            if (areaAspect > aspect)
            {
                f32 keep = aspect / areaAspect;
                uv = Rect(uv.x, uv.y + uv.height * (1 - keep) * 0.5f, uv.width, uv.height * keep);
            }
            else
            {
                f32 keep = areaAspect / aspect;
                uv = Rect(uv.x + uv.width * (1 - keep) * 0.5f, uv.y, uv.width * keep, uv.height);
            }
            quad(area, uv);
            break;
        }
        default: quad(area, image.uv); break;
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
