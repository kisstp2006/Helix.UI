#pragma once

#include <Helix/Engine/Object/Object.h>
#include <Helix/Font/Atlas/FontAtlas.h>
#include <Helix/UI/Rendering/UIMesh.h>

namespace helix::engine
{
    class Texture2D;
}

// The UI's dynamic atlas: small readable textures (RGBA32, up to 256 pixels a side) copied into shared pages, so images
// of many textures draw in one batch. Larger or unreadable textures draw from themselves.
namespace helix::ui
{
    class ImageAtlas
    {
    public:
        static constexpr u32 kPageSize = 2048, kMaxSide = 256;
        // The edge pixels repeated around each image (filtering at its edges stays inside it).
        static constexpr u32 kExtrude = 1;

        ImageAtlas();
        ~ImageAtlas();
        ImageAtlas(const ImageAtlas&) = delete;
        ImageAtlas& operator=(const ImageAtlas&) = delete;

        // Where an image of a texture is drawn from: an atlas page and its rectangle there, or the texture itself.
        void Place(engine::Texture2D* texture, UIImage& out);
        // Uploads the pages images were added to.
        void Upload();
        u32 PageCount() const { return atlas_.PageCount(); }

    private:
        font::FontAtlas atlas_;
        Array<engine::PPtr<engine::Texture2D>> pages_;
        // Textures that cannot go in (by entity id).
        Array<u64> refused_;
        // The update counts of the textures placed (a texture changed after it was placed draws from itself).
        struct Placed
        {
            u64 id = 0;
            u32 updateCount = 0;
        };
        Array<Placed> placed_;
        bool dirty_ = false;
    };
}
