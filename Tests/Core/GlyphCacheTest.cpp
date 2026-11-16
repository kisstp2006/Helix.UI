#include <Support/FontWriter.h>

#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/Fonts/GlyphCache.h>
#include <Helix/UI/Text/Fonts/SystemFonts.h>

#include <cstdio>

using namespace helix;
using namespace helix::ui;
using namespace helix::font::testing;

namespace
{
    // Glyph 1 a 500 unit square, glyph 2 empty (a space); 1000 units per em, ascender 800, descender -200, gap 90.
    std::shared_ptr<const Face> SquareFace()
    {
        FontSpec s;
        s.glyphs.Resize(3);
        s.glyphs[1].contours.Push(Square(100, 0, 600, 500));
        s.glyphs[1].advance = 700;
        s.glyphs[2].advance = 250;
        s.cmap = {{'A', 1}, {' ', 2}};
        s.names = {{String("1"), String("Square Test")}, {String("2"), String("Bold")}};
        Array<u8> bytes = BuildFont(s);
        auto face = std::make_shared<Face>();
        String error;
        face->Open(Span<const u8>(bytes.Data(), bytes.Size()), 0, error);
        return face;
    }
}

HX_TEST(GlyphCache, FaceInfoFromTheMetrics)
{
    auto face = SquareFace();
    FaceInfo info = FaceInfoOf(*face, 20);
    HX_CHECK(info.familyName == "Square Test" && info.styleName == "Bold" && info.unitsPerEM == 1000);
    HX_CHECK(info.ascentLine == 16 && info.descentLine == -4 && info.lineHeight > 21.79f && info.lineHeight < 21.81f);
    HX_CHECK(info.tabWidth == 5);
}

HX_TEST(GlyphCache, DistanceFieldsAndCoverageInAtlasPages)
{
    auto face = SquareFace();
    GlyphCache sdf(face, GlyphRenderMode::Sdf, 64, 8, 256);
    const GlyphEntry* a = sdf.Get(1);
    HX_REQUIRE(a != nullptr && !a->empty && !a->color);
    // 500 units at 64 pixels per em are 32 pixels (6.4 to 38.4: the pixels 6 to 38), with 8 of padding around.
    HX_CHECK(a->width == 49 && a->height == 49 && a->left == 6 - 8 && a->top == -33 - 8);
    HX_CHECK(sdf.Atlas().PageCount() == 1 && sdf.Atlas().Channels() == 1 && sdf.Version() == 1);
    // The middle of the square is inside (above the edge value 128), a corner of the padding outside.
    const u8* page = sdf.Atlas().PagePixels(a->page);
    HX_CHECK(page[(a->y + 24) * 256 + a->x + 24] > 200 && page[a->y * 256 + a->x] < 60);
    // The same glyph again: the same place, nothing new.
    HX_CHECK(sdf.Get(1) == a && sdf.Version() == 1);
    // A space has no image.
    const GlyphEntry* space = sdf.Get(2);
    HX_CHECK(space != nullptr && space->empty);
    GlyphCache msdf(face, GlyphRenderMode::Msdf, 32, 4, 128);
    const GlyphEntry* m = msdf.Get(1);
    HX_CHECK(m != nullptr && !m->empty && msdf.Atlas().Channels() == 4);
    GlyphCache smooth(face, GlyphRenderMode::Smooth, 32, 1, 128);
    const GlyphEntry* c = smooth.Get(1);
    HX_CHECK(c != nullptr && c->width == 17 && c->height == 17 && c->left == 3 && c->top == -17);
}

HX_TEST(GlyphCache, SystemFontsByFamily)
{
    // The Web has no fonts of the machine.
    if (SystemFonts::Folders().Empty())
    {
        std::printf("    no font folders on this platform\n");
        return;
    }
    const SystemFontFace* arial = SystemFonts::Find("arial");
    if (!arial)
    {
        std::printf("    no Arial on this machine (%u faces)\n", u32(SystemFonts::Faces().Size()));
        return;
    }
    HX_CHECK(arial->family == "Arial" && arial->style == "Regular");
    const SystemFontFace* bold = SystemFonts::Find("Arial", "Bold");
    HX_CHECK(bold != nullptr && bold->style == "Bold" && bold->path != arial->path);
    std::printf("    %u faces, Arial at %s\n", u32(SystemFonts::Faces().Size()), arial->path.Data());
    HX_CHECK(SystemFonts::Find("No Such Family Here") == nullptr);
}
