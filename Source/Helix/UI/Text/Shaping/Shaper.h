#pragma once

#include <Helix/UI/Text/Bidi/Bidi.h>
#include <Helix/UI/Text/Shaping/Face.h>

// The text shaper: code points to positioned glyphs. Itemize splits a paragraph into runs of one script, one bidi level
// and one face of a fallback chain; Shape turns a run into glyphs with the face's OpenType layout (GSUB, GPOS with GDEF;
// the kern table when GPOS has no kerning), Arabic joining forms from the joining types, and variation selectors.
// Glyphs stay in logical order with the index of their cluster's first code point; positions are in font units.
namespace helix::ui
{
    inline constexpr u32 OpenTypeTag(const char (&name)[5]) { return font::Tag(name); }

    // A feature switched on or off for a run (on by default: ccmp, locl, rlig, liga, clig, calt, kern, mark, mkmk, curs,
    // and the Arabic forms; off: everything else).
    struct FontFeature
    {
        u32 tag = 0;
        bool enabled = true;
    };

    struct ShapingRun
    {
        u32 start = 0, end = 0;
        Script script = kScriptCommon;
        u8 level = 0;
        // Into the fallback chain.
        u32 face = 0;
        bool RightToLeft() const { return (level & 1) != 0; }
    };

    struct ShapedGlyph
    {
        u32 glyph = 0;
        // The first code point of the cluster (an index into the shaped text).
        u32 cluster = 0;
        i32 xAdvance = 0, yAdvance = 0, xOffset = 0, yOffset = 0;
        // GDEF class: 1 base, 2 ligature, 3 mark, 4 component.
        u8 glyphClass = 1;
    };

    // Runs of text[start, end): by script (Common and Inherited take their neighbors' script), by the levels of `bidi`
    // (resolved for the same text) and by face: each character in the first face of `faces` that has it, marks, joiners
    // and selectors with their base.
    void Itemize(Span<const char32_t> text, u32 start, u32 end, const Bidi& bidi, Span<const Face* const> faces, Array<ShapingRun>& out);

    // The OpenType script tag of a script ('latn', 'arab', 'hani', 'kana'; 'DFLT' for Common).
    u32 OpenTypeScript(Script script);

    class Shaper
    {
    public:
        // Shapes text[run.start, run.end) with `face`; the language system by its OpenType tag (0: the default).
        void Shape(const Face& face, Span<const char32_t> text, const ShapingRun& run, Array<ShapedGlyph>& out, u32 language = 0,
                   Span<const FontFeature> features = {});

    private:
        struct Item
        {
            u32 glyph;
            u32 cluster;
            u32 mask;
            u8 glyphClass;
            // Ligature tracking for mark to ligature: the ligature a glyph belongs to and its component.
            u16 ligature;
            u16 component;
            i32 xAdvance, yAdvance, xOffset, yOffset;
            // An attachment (mark or cursive): the glyph it hangs on (kNoAttach: none) and its kind.
            u32 attach;
            u8 attachKind;
        };

        void ApplyLookup(u32 lookup, bool gpos, u32 mask);
        bool ApplyAt(const font::Layout& layout, u32 lookup, u32 at, bool gpos);
        bool ApplySubtable(const font::Layout& layout, const font::LayoutLookup& lookup, const font::LayoutSubtable& st, u32 at, bool gpos);
        bool ApplyContext(const font::Layout& layout, const font::LayoutLookup& lookup, const font::LayoutSubtable& st, u32 at, bool gpos);
        bool Skipped(const font::LayoutLookup& lookup, u32 index) const;
        // The next (or previous) glyph the lookup sees after `index`, ~0u for none.
        u32 Next(const font::LayoutLookup& lookup, u32 index) const;
        u32 Prev(const font::LayoutLookup& lookup, u32 index) const;
        void Replace(u32 at, Span<const u16> glyphs);
        u8 ClassOf(u32 glyph, u32 cluster) const;
        void PositionValue(Item& item, u32 value);
        void ResolveAttachments(bool rightToLeft);

        const Face* face_ = nullptr;
        const char32_t* text_ = nullptr;
        Array<Item> items_;
        u16 nextLigature_ = 0;
        // Where the lookup goes on after a subtable applied.
        u32 next_ = 0;
        u32 depth_ = 0;
        bool rightToLeft_ = false;
    };
}
