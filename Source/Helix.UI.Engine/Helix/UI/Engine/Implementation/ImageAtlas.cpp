#include <Helix/UI/Engine/ImageAtlas.h>

#include <Helix/Engine/Texture/Texture.h>

#include <cstring>

namespace helix::ui
{
    ImageAtlas::ImageAtlas() : atlas_(kPageSize, kPageSize, 4, 2) {}

    ImageAtlas::~ImageAtlas()
    {
        for (engine::PPtr<engine::Texture2D>& p : pages_)
            if (engine::Texture2D* t = p.Get())
                engine::Object::DestroyImmediate(t);
    }

    void ImageAtlas::Place(engine::Texture2D* texture, UIImage& out)
    {
        out.texture = texture;
        out.uv = Rect(0, 0, 1, 1);
        out.rowsUp = true;
        if (!texture)
            return;
        out.textureWidth = f32(texture->width());
        out.textureHeight = f32(texture->height());
        u64 id = texture->GetEntityId().value;
        for (u64 r : refused_)
            if (r == id)
                return;
        u64 key = font::FontAtlas::Key(u32(id), u32(id >> 32), 2);
        const font::AtlasPlace* place = atlas_.Find(key);
        if (!place)
        {
            u32 w = u32(texture->width()), h = u32(texture->height());
            if (!texture->isReadable() || texture->format() != engine::TextureFormat::RGBA32 || w == 0 || h == 0 || w > kMaxSide || h > kMaxSide)
            {
                refused_.Push(id);
                return;
            }
            Array<Color32> pixels = texture->GetPixels32(0);
            font::GlyphBitmap bitmap;
            bitmap.width = w;
            bitmap.height = h;
            bitmap.channels = 4;
            bitmap.pixels.Resize(usize(w) * h * 4);
            // The texture's rows go up, the atlas's down.
            for (u32 y = 0; y < h; y++)
                std::memcpy(bitmap.pixels.Data() + usize(y) * w * 4, pixels.Data() + usize(h - 1 - y) * w, usize(w) * 4);
            font::AtlasPlace placed;
            if (!atlas_.Add(key, bitmap, placed))
            {
                refused_.Push(id);
                return;
            }
            dirty_ = true;
            place = atlas_.Find(key);
        }
        if (place->page >= pages_.Size())
            dirty_ = true;
        // The page is known once uploaded; until then the texture itself draws.
        Upload();
        if (place->page < pages_.Size() && pages_[place->page].Get())
        {
            out.texture = pages_[place->page].Get();
            out.rowsUp = false;
            f32 s = 1.0f / f32(kPageSize);
            out.uv = Rect(f32(place->x) * s, f32(place->y) * s, f32(place->width) * s, f32(place->height) * s);
        }
    }

    void ImageAtlas::Upload()
    {
        if (!dirty_)
            return;
        dirty_ = false;
        for (u32 p = 0; p < atlas_.PageCount(); p++)
        {
            if (p >= pages_.Size())
                pages_.Push(engine::PPtr<engine::Texture2D>());
            u32 x0, y0, x1, y1;
            bool fresh = !pages_[p].Get();
            if (!fresh && !atlas_.Dirty(p, x0, y0, x1, y1))
                continue;
            engine::Texture2D* tex = pages_[p].Get();
            if (!tex)
            {
                // Raw bytes, as the UI's colors are (no sRGB decode on sampling).
                tex = engine::Texture2D::Create(i32(kPageSize), i32(kPageSize), engine::TextureFormat::RGBA32, false, true);
                tex->hideFlags = engine::HideFlags::DontSave;
                tex->SetFilterMode(engine::FilterMode::Bilinear);
                tex->SetWrapMode(engine::TextureWrapMode::Clamp);
                pages_[p] = engine::PPtr<engine::Texture2D>(tex);
            }
            tex->SetPixelData(Span<const u8>(atlas_.PagePixels(p), usize(kPageSize) * kPageSize * 4), 0);
            tex->Apply(false);
        }
        atlas_.ClearDirty();
    }
}
