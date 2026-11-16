#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/Shaping/Shaper.h>

#include <cstdio>

using namespace helix;
using namespace helix::ui;

// Shaping with fonts of the machine when present (nothing of them is in the repository): Arabic joining and the lam alef
// ligature, Latin kerning and CJK through a fallback chain. A machine without them skips each font.
namespace
{
    bool OpenFace(const char* path, u32 index, Face& face)
    {
        Array<u8> bytes;
        String error;
        if (!platform::ReadFile(StringView(path), bytes))
        {
            std::printf("    %s: not on this machine\n", path);
            return false;
        }
        return face.Open(Span<const u8>(bytes.Data(), bytes.Size()), index, error);
    }

    void ShapeText(const Face& face, std::initializer_list<char32_t> chars, Array<ShapedGlyph>& out)
    {
        Array<char32_t> text;
        for (char32_t c : chars)
            text.Push(c);
        Bidi bidi;
        bidi.Resolve(Span<const char32_t>(text.Data(), text.Size()), BidiDirection::Auto);
        const Face* chain[] = {&face};
        Array<ShapingRun> runs;
        Itemize(Span<const char32_t>(text.Data(), text.Size()), 0, u32(text.Size()), bidi, Span<const Face* const>(chain, 1), runs);
        out.Clear();
        Shaper shaper;
        Array<ShapedGlyph> part;
        for (const ShapingRun& r : runs)
        {
            shaper.Shape(face, Span<const char32_t>(text.Data(), text.Size()), r, part);
            out.Append(part.Data(), part.Size());
        }
    }
}

HX_TEST(SystemShaping, ArabicJoiningAndLamAlef)
{
    for (const char* path : {"C:/Windows/Fonts/arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})
    {
        Face face;
        if (!OpenFace(path, 0, face))
            continue;
        std::printf("    %s\n", path);
        // Beh isolated, then beh yeh teh: the initial and medial forms differ from the nominal glyphs (a font may keep the
        // nominal glyph as a final form).
        Array<ShapedGlyph> isolated, word;
        ShapeText(face, {0x628}, isolated);
        ShapeText(face, {0x628, 0x64A, 0x62A}, word);
        HX_REQUIRE(isolated.Size() == 1 && word.Size() == 3);
        HX_CHECK(word[0].glyph != 0 && word[0].glyph != isolated[0].glyph && word[1].glyph != face.Glyph(0x64A) && word[2].glyph != 0);
        // Lam alef: one glyph, the cluster of the lam.
        Array<ShapedGlyph> lamAlef;
        ShapeText(face, {0x644, 0x627}, lamAlef);
        HX_CHECK(lamAlef.Size() == 1 && lamAlef[0].cluster == 0 && lamAlef[0].glyph != face.Glyph(0x644));
    }
}

HX_TEST(SystemShaping, LatinKerningAndCjkFallback)
{
    Face latin, cjk;
    if (!OpenFace("C:/Windows/Fonts/arial.ttf", 0, latin) && !OpenFace("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, latin))
        return;
    // AV kerns closer than A and V apart.
    Array<ShapedGlyph> av, a;
    ShapeText(latin, {'A', 'V'}, av);
    ShapeText(latin, {'A'}, a);
    HX_CHECK(av.Size() == 2 && av[0].xAdvance < a[0].xAdvance);
    if (!OpenFace("C:/Windows/Fonts/msyh.ttc", 0, cjk) && !OpenFace("C:/Windows/Fonts/YuGothR.ttc", 0, cjk))
        return;
    const char32_t text[] = {'A', 'B', 0x6F22, 0x5B57, 0x3002};
    Bidi bidi;
    bidi.Resolve(Span<const char32_t>(text, 5), BidiDirection::Auto);
    const Face* chain[] = {&latin, &cjk};
    Array<ShapingRun> runs;
    Itemize(Span<const char32_t>(text, 5), 0, 5, bidi, Span<const Face* const>(chain, 2), runs);
    HX_REQUIRE(runs.Size() == 2);
    HX_CHECK(runs[0].face == 0 && runs[1].face == 1 && runs[1].start == 2);
    Array<ShapedGlyph> glyphs;
    Shaper shaper;
    shaper.Shape(cjk, Span<const char32_t>(text, 5), runs[1], glyphs);
    HX_CHECK(glyphs.Size() == 3 && glyphs[0].glyph != 0 && glyphs[2].glyph != 0 && glyphs[0].xAdvance == i32(cjk.UnitsPerEm()));
}
