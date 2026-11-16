#include <Core/Support/ShapingFonts.h>

#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/Shaping/Shaper.h>

using namespace helix;
using namespace helix::ui;
using namespace helix::ui::testing;

namespace
{
    // One run over the whole text, the script and level from the first character.
    Array<ShapedGlyph> ShapeAll(const Face& face, std::initializer_list<char32_t> chars, bool rtl = false)
    {
        Array<char32_t> text;
        for (char32_t c : chars)
            text.Push(c);
        ShapingRun run;
        run.end = u32(text.Size());
        run.script = PropertiesOf(text[0]).script;
        run.level = rtl ? 1 : 0;
        Array<ShapedGlyph> out;
        Shaper shaper;
        shaper.Shape(face, Span<const char32_t>(text.Data(), text.Size()), run, out);
        return out;
    }
}

HX_TEST(Shaper, LatinLigaturesKerningAndMarks)
{
    Face face = Open(LatinArabicFont());
    HX_REQUIRE(face.IsOpen() && face.Family() == "Shaping Test");
    Array<ShapedGlyph> g = ShapeAll(face, {'f', 'i'});
    HX_CHECK(g.Size() == 1 && g[0].glyph == Glyphs::Fi && g[0].cluster == 0 && g[0].xAdvance == 560 && g[0].glyphClass == 2);
    g = ShapeAll(face, {'A', 'V', ' ', 'f'});
    HX_CHECK(g.Size() == 4 && g[0].xAdvance == 520 && g[1].xAdvance == 600 && g[2].glyph == Glyphs::Space && g[3].glyph == Glyphs::F);
    // The acute on A: its anchor (50, 0) on A's (300, 700), from the pen after A.
    g = ShapeAll(face, {'A', 0x301});
    HX_CHECK(g.Size() == 2 && g[1].glyph == Glyphs::Acute && g[1].cluster == 1 && g[1].xAdvance == 0 && g[1].xOffset == 250 - 600 && g[1].yOffset == 700);
    // Switched off, liga makes no ligature.
    Array<char32_t> text = {'f', 'i'};
    ShapingRun run;
    run.end = 2;
    run.script = FindScript("Latn");
    FontFeature off{OpenTypeTag("liga"), false};
    Shaper shaper;
    shaper.Shape(face, Span<const char32_t>(text.Data(), 2), run, g, 0, Span<const FontFeature>(&off, 1));
    HX_CHECK(g.Size() == 2 && g[0].glyph == Glyphs::F && g[1].glyph == Glyphs::I);
}

HX_TEST(Shaper, ArabicJoiningFormsAndMarks)
{
    Face face = Open(LatinArabicFont());
    // Beh beh alef: initial, medial, final.
    Array<ShapedGlyph> g = ShapeAll(face, {0x628, 0x628, 0x627}, true);
    HX_CHECK(g.Size() == 3 && g[0].glyph == Glyphs::BehInit && g[1].glyph == Glyphs::BehMedi && g[2].glyph == Glyphs::AlefFina);
    // Alef does not join to the next letter: beh after it starts again (isolated: the font has no isol lookup).
    g = ShapeAll(face, {0x627, 0x628}, true);
    HX_CHECK(g.Size() == 2 && g[0].glyph == Glyphs::Alef && g[1].glyph == Glyphs::Beh);
    // A transparent fatha between the behs keeps them joined; it sits on the first beh (right to left: from its own pen).
    g = ShapeAll(face, {0x628, 0x64E, 0x628}, true);
    HX_CHECK(g.Size() == 3 && g[0].glyph == Glyphs::BehInit && g[1].glyph == Glyphs::Fatha && g[2].glyph == Glyphs::BehFina);
    HX_CHECK(g[1].xAdvance == 0 && g[1].xOffset == 250 && g[1].yOffset == 700);
}

HX_TEST(Shaper, ChainedContextAndCursiveAttachment)
{
    Face face = Open(LatinArabicFont());
    // V between two As becomes I; elsewhere it stays.
    Array<ShapedGlyph> g = ShapeAll(face, {'A', 'V', 'A', 'V'});
    HX_CHECK(g.Size() == 4 && g[1].glyph == Glyphs::I && g[3].glyph == Glyphs::V);
    // Three behs joined: each advance ends at the next one's entry; right to left, the earlier glyphs hang on the later
    // ones (the exit 50 above the entry).
    g = ShapeAll(face, {0x628, 0x628, 0x628}, true);
    HX_REQUIRE(g.Size() == 3);
    HX_CHECK(g[0].glyph == Glyphs::BehInit && g[1].glyph == Glyphs::BehMedi && g[2].glyph == Glyphs::BehFina);
    HX_CHECK(g[0].xAdvance == 380 && g[1].xAdvance == 360 && g[2].xAdvance == 420);
    HX_CHECK(g[0].yOffset == -100 && g[1].yOffset == -50 && g[2].yOffset == 0);
}

HX_TEST(Shaper, KernTableWithoutGpos)
{
    Face face = Open(KernTableFont());
    Array<ShapedGlyph> g = ShapeAll(face, {'A', 'V'});
    HX_CHECK(g.Size() == 2 && g[0].xAdvance == 550 && g[1].xAdvance == 600);
}

HX_TEST(Shaper, ItemizeByScriptLevelAndFallback)
{
    Face latin = Open(LatinArabicFont());
    Face cjk = Open(CjkFont());
    const Face* chain[] = {&latin, &cjk};
    // "A " then two ideographs and a full stop the Latin face lacks: the space stays with A.
    const char32_t text[] = {'A', ' ', 0x6F22, 0x5B57, 0x3002};
    Bidi bidi;
    bidi.Resolve(Span<const char32_t>(text, 5), BidiDirection::Auto);
    Array<ShapingRun> runs;
    Itemize(Span<const char32_t>(text, 5), 0, 5, bidi, Span<const Face* const>(chain, 2), runs);
    HX_REQUIRE(runs.Size() == 2);
    HX_CHECK(runs[0].start == 0 && runs[0].end == 2 && runs[0].face == 0 && runs[0].script == FindScript("Latn"));
    HX_CHECK(runs[1].start == 2 && runs[1].end == 5 && runs[1].face == 1 && runs[1].script == FindScript("Hani"));
    HX_CHECK(OpenTypeScript(FindScript("Hani")) == OpenTypeTag("hani") && OpenTypeScript(FindScript("Hira")) == OpenTypeTag("kana") &&
             OpenTypeScript(kScriptCommon) == OpenTypeTag("DFLT"));
    // Arabic in a Latin paragraph: a run of its own at level 1.
    const char32_t mixed[] = {'A', ' ', 0x628, 0x628};
    bidi.Resolve(Span<const char32_t>(mixed, 4), BidiDirection::Auto);
    Itemize(Span<const char32_t>(mixed, 4), 0, 4, bidi, Span<const Face* const>(chain, 2), runs);
    HX_REQUIRE(runs.Size() == 2);
    HX_CHECK(runs[0].level == 0 && runs[1].level == 1 && runs[1].RightToLeft() && runs[1].script == FindScript("Arab"));
    // A character no face has goes to the first face (its .notdef).
    const char32_t missing[] = {0x10400};
    bidi.Resolve(Span<const char32_t>(missing, 1), BidiDirection::Auto);
    Itemize(Span<const char32_t>(missing, 1), 0, 1, bidi, Span<const Face* const>(chain, 2), runs);
    HX_CHECK(runs.Size() == 1 && runs[0].face == 0);
}
