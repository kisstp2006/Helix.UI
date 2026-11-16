#include <Core/Support/ShapingFonts.h>

#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

#include <cmath>

using namespace helix;
using namespace helix::ui;
using namespace helix::ui::testing;

namespace
{
    // a to z (500 units, a square each), digits, space (250), the ellipsis (600) and alef, bet (500): at size 10 a letter
    // is 5 pixels, the ascent 8, the descent 2 and the line 10.9.
    std::shared_ptr<const Face> LayoutFace()
    {
        FontSpec s;
        s.glyphs.Resize(1);
        auto add = [&](u32 code, u16 advance, bool ink) {
            GlyphSpec g;
            g.advance = advance;
            if (ink)
                g.contours.Push(Square(50, 0, 450, 500));
            s.glyphs.Push(g);
            s.cmap.Push({code, u32(s.glyphs.Size() - 1)});
        };
        for (u32 c = 'a'; c <= 'z'; c++)
            add(c, 500, true);
        for (u32 c = '0'; c <= '9'; c++)
            add(c, 500, true);
        add(' ', 250, false);
        add(0x2026, 600, true);
        add(0x5D0, 500, true);
        add(0x5D1, 500, true);
        s.names = {{String("1"), String("Layout Test")}, {String("2"), String("Regular")}};
        Array<u8> bytes = BuildFont(s);
        auto face = std::make_shared<Face>();
        String error;
        face->Open(Span<const u8>(bytes.Data(), bytes.Size()), 0, error);
        return face;
    }

    struct Fonts
    {
        TextFont latin{LayoutFace(), GlyphRenderMode::Sdf, 32, 4, 256};
        TextFont cjk{std::make_shared<Face>(Open(CjkFont())), GlyphRenderMode::Sdf, 32, 4, 256};
    };

    bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.01f; }

    TextGenerationSettings Settings(Fonts& f, f32 width = -1, f32 height = -1)
    {
        TextGenerationSettings s;
        s.font = &f.latin;
        s.fontSize = 10;
        s.width = width;
        s.height = height;
        return s;
    }
}

HX_TEST(TextLayout, OneLineAdvancesAndQuads)
{
    Fonts f;
    TextGenerator gen;
    TextInfo info;
    gen.Generate("ab cd", Settings(f), info);
    HX_REQUIRE(info.characterCount == 5 && info.lineCount == 1);
    HX_CHECK(Near(info.width, 22.5f) && Near(info.lineInfo[0].baseline, 8) && Near(info.height, 10));
    HX_CHECK(Near(info.characterInfo[3].origin, 12.5f) && Near(info.characterInfo[3].xAdvance, 17.5f));
    HX_CHECK(info.quads.Size() == 4 && info.characterInfo[2].isVisible == false && info.characterInfo[0].isVisible);
    // The quad of 'a': 400 units of ink from 50 at 10 pixels per em, 500 units tall above the baseline (with padding).
    const TextQuad& q = info.quads[0];
    HX_CHECK(q.corners[0].x < 0.5f && q.corners[1].x > 4.5f && q.corners[0].y < 3 && q.corners[3].y > 8 && q.font == &f.latin);
    HX_CHECK(info.wordInfo.Size() == 2 && info.wordInfo[1].firstCharacterIndex == 3 && info.lineInfo[0].wordCount == 2);
}

HX_TEST(TextLayout, WrappingAlignmentAndLongWords)
{
    Fonts f;
    TextGenerator gen;
    TextInfo info;
    gen.Generate("ab cd", Settings(f, 15), info);
    HX_REQUIRE(info.lineCount == 2);
    HX_CHECK(info.lineInfo[0].firstCharacterIndex == 0 && info.lineInfo[1].firstCharacterIndex == 3 && Near(info.lineInfo[0].width, 10));
    HX_CHECK(Near(info.lineInfo[1].baseline, 8 + 10.9f) && info.characterInfo[3].lineNumber == 1 && Near(info.characterInfo[3].origin, 0));
    // A word longer than the line breaks inside.
    gen.Generate("abcdef", Settings(f, 12), info);
    HX_CHECK(info.lineCount == 3 && info.lineInfo[1].firstCharacterIndex == 2);
    // Center and right in a 30 pixel box; justified spreads the spaces of all lines but the last.
    TextGenerationSettings s = Settings(f, 30);
    s.alignment = TextAlignment::Center;
    gen.Generate("ab", s, info);
    HX_CHECK(Near(info.characterInfo[0].origin, 10));
    s.alignment = TextAlignment::Right;
    gen.Generate("ab", s, info);
    HX_CHECK(Near(info.characterInfo[0].origin, 20));
    s.alignment = TextAlignment::Justified;
    s.width = 25;
    gen.Generate("ab cd ef gh", s, info);
    HX_REQUIRE(info.lineCount == 2);
    // "ab cd" spread to 25 (its one space 2.5 wider); "ef gh" is the last line.
    HX_CHECK(Near(info.characterInfo[3].origin, 15) && Near(info.characterInfo[6].origin, 0));
    // Paragraphs at line feeds; an empty one still takes a line.
    gen.Generate("a\n\nb", Settings(f), info);
    HX_CHECK(info.lineCount == 3 && info.characterInfo[3].lineNumber == 2);
}

HX_TEST(TextLayout, BidiLinesAndFallbackFonts)
{
    Fonts f;
    TextGenerator gen;
    TextInfo info;
    // "ab " then alef bet: the Hebrew word reads right to left, bet left of alef.
    gen.Generate("ab \xD7\x90\xD7\x91", Settings(f), info);
    HX_REQUIRE(info.characterCount == 5);
    HX_CHECK(info.characterInfo[4].origin < info.characterInfo[3].origin && Near(info.characterInfo[4].origin, 12.5f));
    // A right to left paragraph keeps its first letter at the right.
    TextGenerationSettings s = Settings(f);
    s.direction = BidiDirection::RightToLeft;
    gen.Generate("ab", s, info);
    HX_CHECK(info.characterInfo[0].origin < info.characterInfo[1].origin);
    gen.Generate("\xD7\x90\xD7\x91", s, info);
    HX_CHECK(info.characterInfo[0].origin > info.characterInfo[1].origin);
    // An ideograph the Latin font lacks comes from the fallback.
    TextFont* fallbacks[] = {&f.cjk};
    s = Settings(f);
    s.fallbacks = Span<TextFont* const>(fallbacks, 1);
    gen.Generate("a\xE6\xBC\xA2", s, info);
    HX_REQUIRE(info.characterCount == 2);
    HX_CHECK(info.characterInfo[1].font == &f.cjk && info.characterInfo[1].glyph == 1 && Near(info.characterInfo[1].origin, 5) &&
             Near(info.characterInfo[1].xAdvance, 15));
}

HX_TEST(TextLayout, RichTextStylesAndDecorations)
{
    Fonts f;
    TextGenerator gen;
    TextInfo info;
    gen.Generate("<b>a</b><size=20>b</size><color=red>c</color>", Settings(f), info);
    HX_REQUIRE(info.characterCount == 3);
    HX_CHECK(Near(info.characterInfo[1].origin, 5.7f) && Near(info.characterInfo[2].origin, 15.7f));
    HX_CHECK(info.quads[0].bold && !info.quads[1].bold && info.quads[2].color == Color(1, 0, 0, 1));
    // The 20 pixel b makes the line taller.
    HX_CHECK(Near(info.lineInfo[0].baseline, 16));
    gen.Generate("<u>ab</u> <mark=#00ff0040>c</mark><s>d</s>", Settings(f), info);
    HX_REQUIRE(info.rects.Size() == 3);
    bool behind = false, under = false;
    for (const TextRect& r : info.rects)
    {
        behind = behind || (r.behind && Near(r.rect.x, 12.5f) && r.color.g == 1);
        under = under || (!r.behind && Near(r.rect.x, 0) && Near(r.rect.width, 10));
    }
    HX_CHECK(behind && under);
    gen.Generate("x<link=\"a\">yz</link>", Settings(f), info);
    HX_CHECK(info.linkInfo.Size() == 1 && info.linkInfo[0].id == "a" && info.linkInfo[0].firstCharacterIndex == 1 && info.linkInfo[0].characterCount == 2);
    // Upper case and character spacing.
    TextGenerationSettings s = Settings(f);
    s.characterSpacing = 10;
    gen.Generate("<uppercase>a</uppercase>b", s, info);
    HX_CHECK(info.characterInfo[0].character == 'a' && Near(info.characterInfo[1].origin, 6));
}

HX_TEST(TextLayout, OverflowAutoSizeAndPages)
{
    Fonts f;
    TextGenerator gen;
    TextInfo info;
    TextGenerationSettings s = Settings(f, 20, 12);
    s.overflow = TextOverflowMode::Ellipsis;
    gen.Generate("ab cd ef", s, info);
    HX_CHECK(info.overflowed && info.lineCount == 3 && !info.characterInfo[6].isVisible);
    // The last quad is the ellipsis after the first line.
    HX_CHECK(info.quads.Back().corners[0].x > 9);
    s.overflow = TextOverflowMode::Truncate;
    gen.Generate("ab cd ef", s, info);
    HX_CHECK(info.overflowed && info.quads.Size() == 2);
    // Pages: one line each in an 11 pixel box.
    s = Settings(f, 100, 11);
    s.overflow = TextOverflowMode::Page;
    s.page = 1;
    gen.Generate("a\nb\nc", s, info);
    HX_CHECK(info.pageCount == 3 && info.pageInfo[1].firstCharacterIndex == 2 && info.quads.Size() == 1 && Near(info.lineInfo[1].baseline, 8));
    // Auto size: the largest size that keeps the line in 10 pixels.
    s = Settings(f, 10, 100);
    s.wrapping = TextWrappingMode::NoWrap;
    s.autoSize = true;
    s.minFontSize = 1;
    s.maxFontSize = 72;
    gen.Generate("abcd", s, info);
    HX_CHECK(info.fontSize > 4.5f && info.fontSize <= 5.01f && !info.overflowed);
    // The preferred size.
    Vector2 p = gen.PreferredSize("ab cd", Settings(f));
    HX_CHECK(Near(p.x, 22.5f) && Near(p.y, 10));
    Vector2 narrow = gen.PreferredSize("ab cd", Settings(f), 15);
    HX_CHECK(Near(narrow.x, 10) && Near(narrow.y, 8 + 10.9f + 2));
}
