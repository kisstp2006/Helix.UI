#pragma once

#include <Helix/UI/Text/Fonts/GlyphCache.h>

// A font of the text system: a face, its glyph images and the fonts it falls back to (not owned: an engine asset or the
// caller keeps them). The chain a text shapes with is the font, its fallbacks depth first, then the global fallbacks.
namespace helix::ui
{
    class TextFont
    {
    public:
        TextFont(std::shared_ptr<const Face> face, GlyphRenderMode mode = GlyphRenderMode::Sdf, u32 samplingSize = 64, u32 padding = 8,
                 u32 atlasSize = 1024)
            : face_(face), cache_(face, mode, samplingSize, padding, atlasSize)
        {
        }

        const Face& GetFace() const { return *face_; }
        GlyphCache& Cache() { return cache_; }
        FaceInfo Info(f32 pointSize) const { return FaceInfoOf(*face_, pointSize); }
        // Pixels per font unit at a point size.
        f32 Scale(f32 pointSize) const { return pointSize / f32(face_->UnitsPerEm() ? face_->UnitsPerEm() : 1000); }

        Array<TextFont*> fallbacks;

        // The font, its fallbacks (depth first, each font once) and then `global`'s chains.
        static void Chain(TextFont* font, Span<TextFont* const> global, Array<TextFont*>& out);

    private:
        std::shared_ptr<const Face> face_;
        GlyphCache cache_;
    };

    inline void TextFont::Chain(TextFont* font, Span<TextFont* const> global, Array<TextFont*>& out)
    {
        out.Clear();
        auto add = [&](auto& self, TextFont* f, u32 depth) -> void {
            if (!f || depth > 16)
                return;
            for (TextFont* seen : out)
                if (seen == f)
                    return;
            out.Push(f);
            for (TextFont* fb : f->fallbacks)
                self(self, fb, depth + 1);
        };
        add(add, font, 0);
        for (TextFont* g : global)
            add(add, g, 0);
    }
}
