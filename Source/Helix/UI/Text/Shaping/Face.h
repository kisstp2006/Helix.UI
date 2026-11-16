#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Font/Font/Font.h>
#include <Helix/Font/Layout/Kern.h>
#include <Helix/Font/Layout/Layout.h>

#include <memory>

// A font face as the shaper sees it: the font with its layout tables (GSUB, GPOS, GDEF, kern) read once. The face keeps
// its own copy of the file's bytes.
namespace helix::ui
{
    // Which glyphs a lookup may start at, as three 64 bit masks of the glyph id shifted by 4, 0 and 9 (a glyph the masks
    // do not all hold is certainly not covered): a quick test before the coverage's binary search.
    struct LookupDigest
    {
        static constexpr u32 kShifts[3] = {4, 0, 9};
        u64 masks[3] = {0, 0, 0};

        void Add(u32 first, u32 last)
        {
            for (u32 k = 0; k < 3; k++)
            {
                u32 a = first >> kShifts[k], b = last >> kShifts[k];
                if (b - a >= 63)
                    masks[k] = ~u64(0);
                else
                    for (u32 g = a; g <= b; g++)
                        masks[k] |= u64(1) << (g & 63);
            }
        }

        bool May(u32 glyph) const
        {
            for (u32 k = 0; k < 3; k++)
                if (!((masks[k] >> ((glyph >> kShifts[k]) & 63)) & 1))
                    return false;
            return true;
        }
    };

    class Face
    {
    public:
        // The face `index` of a font or collection file; false with the reason when it cannot be read.
        bool Open(Span<const u8> file, u32 index, String& error);
        bool IsOpen() const { return open_; }

        const font::Font& Font() const { return font_; }
        const font::Layout& Gsub() const { return gsub_; }
        const font::Layout& Gpos() const { return gpos_; }
        const font::GlyphDefinitions& Gdef() const { return gdef_; }
        const font::KernTable& Kern() const { return kern_; }
        bool HasGdef() const { return hasGdef_; }
        // The digest of each lookup of GSUB (or GPOS).
        const LookupDigest& Digest(bool gpos, u32 lookup) const { return (gpos ? gposDigests_ : gsubDigests_)[lookup]; }
        u32 UnitsPerEm() const { return font_.Head().unitsPerEm; }

        // The glyph of a code point (0 when the face has none), with a variation selector when one follows it.
        u32 Glyph(char32_t c, char32_t selector = 0) const;
        bool Has(char32_t c) const { return Glyph(c) != 0; }
        i32 Advance(u32 glyph) const { return font_.HMetrics(glyph).advance; }

        // The family and style names (name ids 1 and 2, or 16 and 17 when present).
        StringView Family() const;
        StringView Style() const;

    private:
        std::shared_ptr<Array<u8>> bytes_;
        font::Font font_;
        font::Layout gsub_, gpos_;
        font::GlyphDefinitions gdef_;
        font::KernTable kern_;
        Array<LookupDigest> gsubDigests_, gposDigests_;
        bool hasGdef_ = false;
        bool open_ = false;
    };
}
