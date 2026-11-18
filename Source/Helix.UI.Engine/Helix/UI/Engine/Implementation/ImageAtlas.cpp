#include <Helix/UI/Engine/ImageAtlas.h>

#include <Helix/Core/Containers/HashMap.h>
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

    namespace
    {
        HashMap<u64, engine::EntityId>& External()
        {
            static HashMap<u64, engine::EntityId> s;
            return s;
        }
    }

    const void* ExternalTextureHandle(engine::Texture* texture)
    {
        if (!texture)
            return nullptr;
        External().Set(u64(uintptr_t(texture)), texture->GetEntityId());
        return texture;
    }

    engine::Texture* TextureOfHandle(const void* handle)
    {
        if (!handle)
            return nullptr;
        u64 key = u64(uintptr_t(handle));
        if (const engine::EntityId* id = External().Find(key))
        {
            engine::Texture* t = engine::EntityIdToObject<engine::Texture>(*id);
            if (!t)
                External().Remove(key);
            return t;
        }
        return static_cast<engine::Texture*>(const_cast<void*>(handle));
    }

    void ImageAtlas::Place(engine::Texture* any, UIImage& out)
    {
        out.texture = any;
        out.uv = Rect(0, 0, 1, 1);
        out.rowsUp = true;
        if (!any)
            return;
        out.textureWidth = f32(any->width());
        out.textureHeight = f32(any->height());
        // Only images go into the atlas (a render texture is drawn from itself: a camera draws into it each frame; it may
        // go before the mesh is built again).
        if (!any->Is<engine::Texture2D>())
        {
            out.texture = ExternalTextureHandle(any);
            return;
        }
        auto* texture = static_cast<engine::Texture2D*>(any);
        u64 id = texture->GetEntityId().value;
        for (u64 r : refused_)
            if (r == id)
                return;
        // A texture written to after it was placed (a runtime image) is a dynamic one: its copy would be stale.
        for (const Placed& p : placed_)
            if (p.id == id && p.updateCount != texture->updateCount())
            {
                refused_.Push(id);
                return;
            }
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
            // The image with its edge pixels repeated once around it, so filtering at its edges stays inside it (as
            // the texture's own clamp would).
            font::GlyphBitmap bitmap;
            bitmap.width = w + 2 * kExtrude;
            bitmap.height = h + 2 * kExtrude;
            bitmap.channels = 4;
            bitmap.pixels.Resize(usize(bitmap.width) * bitmap.height * 4);
            for (u32 y = 0; y < bitmap.height; y++)
            {
                // The texture's rows go up, the atlas's down.
                u32 sy = y < kExtrude ? 0 : y - kExtrude >= h ? h - 1 : y - kExtrude;
                const Color32* row = pixels.Data() + usize(h - 1 - sy) * w;
                u8* to = bitmap.pixels.Data() + usize(y) * bitmap.width * 4;
                for (u32 x = 0; x < bitmap.width; x++)
                {
                    u32 sx = x < kExtrude ? 0 : x - kExtrude >= w ? w - 1 : x - kExtrude;
                    std::memcpy(to + usize(x) * 4, row + sx, 4);
                }
            }
            font::AtlasPlace placed;
            if (!atlas_.Add(key, bitmap, placed))
            {
                refused_.Push(id);
                return;
            }
            dirty_ = true;
            placed_.Push(Placed{id, texture->updateCount()});
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
            out.uv = Rect(f32(place->x + kExtrude) * s, f32(place->y + kExtrude) * s, f32(place->width - 2 * kExtrude) * s,
                          f32(place->height - 2 * kExtrude) * s);
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
