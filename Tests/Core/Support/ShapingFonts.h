#pragma once

#include <Support/FontWriter.h>
#include <Support/LayoutWriter.h>

#include <Helix/UI/Text/Shaping/Face.h>

// Fonts built in code for the shaping tests (no font files): a Latin and Arabic face with layout tables, a CJK face with
// only a character map, and a Latin face with a kern table and no GPOS.
namespace helix::ui::testing
{
    using namespace font::testing;

    // Glyph ids of the Latin and Arabic face.
    namespace Glyphs
    {
        inline constexpr u32 Space = 1, F = 2, I = 3, Fi = 4, A = 5, V = 6, Acute = 7;
        inline constexpr u32 Beh = 8, BehInit = 9, BehMedi = 10, BehFina = 11, Alef = 12, AlefFina = 13, Fatha = 14;
    }

    // A GSUB or GPOS table: the scripts 'latn' and 'arab' with a default language holding every feature.
    struct FeatureSpec
    {
        const char* tag;
        Array<u16> lookups;
    };

    inline Array<u8> LayoutWith(const Array<FeatureSpec>& features, const Array<LookupSpec>& lookups)
    {
        Blob lang;
        lang.U16(0).U16(0xFFFF).U16(u32(features.Size()));
        for (usize i = 0; i < features.Size(); i++)
            lang.U16(u32(i));
        Blob latn, arab;
        latn.Offset16(lang).U16(0);
        arab.Offset16(lang).U16(0);
        Blob scripts;
        scripts.U16(2).Tag("arab").Offset16(arab).Tag("latn").Offset16(latn);
        Blob featureList;
        featureList.U16(u32(features.Size()));
        for (const FeatureSpec& f : features)
        {
            Blob table;
            table.U16(0).U16(u32(f.lookups.Size()));
            for (u16 l : f.lookups)
                table.U16(l);
            featureList.Tag(f.tag).Offset16(table);
        }
        Blob list;
        list.U16(u32(lookups.Size()));
        for (const LookupSpec& l : lookups)
        {
            Blob lookup;
            lookup.U16(l.type).U16(l.flags).U16(u32(l.subtables.Size()));
            for (const Array<u8>& s : l.subtables)
                lookup.Offset16(s);
            list.Offset16(lookup);
        }
        Blob table;
        table.U16(1).U16(0).Offset16(scripts).Offset16(featureList).Offset16(list);
        return table.Flatten();
    }

    inline Array<u8> LatinArabicFont()
    {
        using namespace Glyphs;
        FontSpec s;
        s.glyphs.Resize(15);
        const u16 advances[] = {500, 250, 300, 280, 560, 600, 600, 0, 400, 380, 360, 420, 200, 220, 0};
        for (u32 g = 0; g < 15; g++)
            s.glyphs[g].advance = advances[g];
        s.cmap = {{32, Space}, {'f', F}, {'i', I}, {'A', A}, {'V', V}, {0x301, Acute}, {0x628, Beh}, {0x627, Alef}, {0x64E, Fatha}};
        s.names = {{String("1"), String("Shaping Test")}, {String("2"), String("Regular")}};

        // GSUB: 0 liga f i, 1 init, 2 medi, 3 fina, 4 V to I (only through 5), 5 calt: V between two As (chained context
        // by coverages).
        Array<LookupSpec> gsub;
        gsub.Resize(6);
        Blob lig, set, st;
        lig.U16(Fi).U16(2).U16(I);
        set.U16(1).Offset16(lig);
        st.U16(1).Offset16(Coverage1({F})).U16(1).Offset16(set);
        gsub[0] = {4, 0, 0, {st.Flatten()}};
        auto single = [](const Array<u16>& from, const Array<u16>& to) {
            Blob b;
            b.U16(2).Offset16(Coverage1(from)).U16(u32(to.Size()));
            for (u16 g : to)
                b.U16(g);
            return b.Flatten();
        };
        gsub[1] = {1, 0, 0, {single({Beh}, {BehInit})}};
        gsub[2] = {1, 0, 0, {single({Beh}, {BehMedi})}};
        gsub[3] = {1, 0, 0, {single({Beh, Alef}, {BehFina, AlefFina})}};
        gsub[4] = {1, 0, 0, {single({V}, {I})}};
        Blob chain;
        chain.U16(3).U16(1).Offset16(Coverage1({A})).U16(1).Offset16(Coverage1({V})).U16(1).Offset16(Coverage1({A})).U16(1).U16(0).U16(4);
        gsub[5] = {6, 0, 0, {chain.Flatten()}};
        s.AddTable("GSUB", LayoutWith({{"liga", {0}}, {"init", {1}}, {"medi", {2}}, {"fina", {3}}, {"calt", {5}}}, gsub));

        // GPOS: 0 kern A V by -80, 1 marks on bases at (300, 700) from their anchor (50, 0), 2 cursive joins of the beh
        // forms (exits at (0, 50), entries at their advance, right to left).
        Array<LookupSpec> gpos;
        gpos.Resize(3);
        Blob pairs, pair;
        pairs.U16(1).U16(V).I16(-80);
        pair.U16(1).Offset16(Coverage1({A})).U16(4).U16(0).U16(1).Offset16(pairs);
        gpos[0] = {2, 0, 0, {pair.Flatten()}};
        Blob markArray;
        markArray.U16(2).U16(0).Offset16(Anchor1(50, 0)).U16(0).Offset16(Anchor1(50, 0));
        Blob baseArray;
        baseArray.U16(5);
        for (int k = 0; k < 5; k++)
            baseArray.Offset16(Anchor1(300, 700));
        Blob markBase;
        markBase.U16(1).Offset16(Coverage1({Acute, Fatha})).Offset16(Coverage1({A, Beh, BehInit, BehMedi, BehFina})).U16(1).Offset16(markArray).Offset16(baseArray);
        gpos[1] = {4, 0, 0, {markBase.Flatten()}};
        Blob cursive;
        cursive.U16(1).Offset16(Coverage1({BehInit, BehMedi, BehFina})).U16(3);
        cursive.U16(0).Offset16(Anchor1(0, 50));
        cursive.Offset16(Anchor1(360, 0)).Offset16(Anchor1(0, 50));
        cursive.Offset16(Anchor1(420, 0)).U16(0);
        gpos[2] = {3, 1, 0, {cursive.Flatten()}};
        s.AddTable("GPOS", LayoutWith({{"kern", {0}}, {"mark", {1}}, {"curs", {2}}}, gpos));

        // GDEF 1.0: the ligature and the marks.
        Blob gdef;
        gdef.U16(1).U16(0).Offset16(ClassDef2({{Fi, Fi, 2}, {Acute, Acute, 3}, {Fatha, Fatha, 3}})).U16(0).U16(0).U16(0);
        s.AddTable("GDEF", gdef.Flatten());
        return BuildFont(s);
    }

    // Glyphs 1 to 3: two ideographs and the ideographic full stop, a full em each.
    inline Array<u8> CjkFont()
    {
        FontSpec s;
        s.glyphs.Resize(5);
        for (u32 g = 1; g < 5; g++)
            s.glyphs[g].advance = 1000;
        s.cmap = {{0x6F22, 1}, {0x5B57, 2}, {0x3002, 3}, {32, 4}};
        s.names = {{String("1"), String("Shaping CJK")}, {String("2"), String("Regular")}};
        return BuildFont(s);
    }

    // A and V with a kern table pair of -50 and no GPOS.
    inline Array<u8> KernTableFont()
    {
        FontSpec s;
        s.glyphs.Resize(3);
        s.glyphs[1].advance = 600;
        s.glyphs[2].advance = 600;
        s.cmap = {{'A', 1}, {'V', 2}};
        s.kerning = {{1, 2, -50}};
        return BuildFont(s);
    }

    inline Face Open(const Array<u8>& bytes)
    {
        Face f;
        String error;
        f.Open(Span<const u8>(bytes.Data(), bytes.Size()), 0, error);
        return f;
    }
}
