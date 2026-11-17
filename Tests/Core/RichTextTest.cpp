#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/RichText/RichText.h>

using namespace helix;
using namespace helix::ui;

namespace
{
    String Plain(const ParsedText& t)
    {
        String s;
        for (const RichTextElement& e : t.elements)
            if (e.kind == TextElementKind::Character && e.code < 0x80)
                s += StringView(reinterpret_cast<const char*>(&e.code), 1);
        return s;
    }

    const TextStyle& StyleAt(const ParsedText& t, u32 i) { return t.styles[t.elements[i].style]; }

    struct Sheets final : TextStyleSheetLookup
    {
        bool Find(StringView name, String& opening, String& closing) const override
        {
            if (name != "H1")
                return false;
            opening = String("<size=2em><b>");
            closing = String("</b></size>");
            return true;
        }
    };
}

HX_TEST(RichText, FlagsColorsAndNesting)
{
    ParsedText t;
    ParseRichText("a<b>b<i>c</b>d</i>e", true, TextStyle(), t);
    HX_CHECK(Plain(t) == "abcde");
    HX_CHECK(StyleAt(t, 0).flags == 0 && StyleAt(t, 1).flags == TextFlags::Bold && StyleAt(t, 2).flags == (TextFlags::Bold | TextFlags::Italic));
    HX_CHECK(StyleAt(t, 3).flags == TextFlags::Italic && StyleAt(t, 4).flags == 0);
    // Colors by name and hex; the closing tag brings back the outer one.
    ParseRichText("<color=red>r<color=#00ff0080>g</color>r</color>w", true, TextStyle(), t);
    HX_CHECK(StyleAt(t, 0).color == Color(1, 0, 0, 1) && StyleAt(t, 1).color.g == 1 && StyleAt(t, 1).color.a > 0.5f && StyleAt(t, 1).color.a < 0.51f);
    HX_CHECK(StyleAt(t, 2).color == Color(1, 0, 0, 1) && !StyleAt(t, 3).hasColor);
    Color c;
    HX_CHECK(ParseTextColor("#F80", c) && c == Color(1, 0x88 / 255.0f, 0, 1) && !ParseTextColor("#12345", c) && !ParseTextColor("nope", c));
    // Equal states share a style.
    ParseRichText("<b>x</b>y<b>z</b>", true, TextStyle(), t);
    HX_CHECK(t.elements[0].style == t.elements[2].style && t.styles.Size() == 2);
}

HX_TEST(RichText, LengthsSizesAndStandaloneTags)
{
    ParsedText t;
    ParseRichText("<size=150%>a</size><size=+4>b</size><size=2em>c<cspace=0.1em>d</cspace></size>", true, TextStyle(), t);
    HX_CHECK(StyleAt(t, 0).size.unit == TextLength::Unit::Percent && StyleAt(t, 0).size.value == 150);
    HX_CHECK(StyleAt(t, 1).size.unit == TextLength::Unit::Relative && StyleAt(t, 1).size.Resolve(10, 10, 20) == 24);
    HX_CHECK(StyleAt(t, 2).size.Resolve(10, 0, 0) == 20 && StyleAt(t, 3).characterSpacing.Resolve(20, 0, 0) > 1.99f);
    // br, space, pos, page and sprites become elements; the plain text holds U+FFFC for them.
    ParseRichText("a<br>b<space=5px>c<pos=50%>d<page><sprite=3><sprite name=\"smile\">", true, TextStyle(), t);
    HX_REQUIRE(t.elements.Size() == 10);
    HX_CHECK(t.elements[1].code == '\n' && t.elements[3].kind == TextElementKind::Space && t.elements[3].length.value == 5);
    HX_CHECK(t.elements[5].kind == TextElementKind::Position && t.elements[5].length.unit == TextLength::Unit::Percent);
    HX_CHECK(t.elements[7].kind == TextElementKind::PageBreak && t.text[7] == 0xC && t.text[3] == 0xFFFC);
    HX_CHECK(t.elements[8].kind == TextElementKind::Sprite && t.elements[8].spriteIndex == 3 && t.elements[9].spriteName == "smile");
    // align, mark, line-height, margin, voffset, link.
    ParseRichText("<align=center><mark=#ff000040><line-height=120%><margin=10>x</margin><voffset=1em><link=\"id1\">y</link>", true, TextStyle(), t);
    HX_CHECK(StyleAt(t, 0).hasAlign && StyleAt(t, 0).align == TextAlignment::Center && (StyleAt(t, 0).flags & TextFlags::Mark));
    HX_CHECK(StyleAt(t, 0).marginLeft.value == 10 && StyleAt(t, 0).marginRight.value == 10 && StyleAt(t, 1).marginLeft.value == 0);
    HX_CHECK(StyleAt(t, 1).link == 0 && t.links.Size() == 1 && t.links[0].id == "id1" && t.links[0].firstElement == 1 && t.links[0].count == 1);
    HX_CHECK(!t.links[0].href);
    // An anchor: a link whose id is its address.
    ParseRichText("go <a href=\"https://example.org/x\">here</a>!", true, TextStyle(), t);
    HX_CHECK(t.links.Size() == 1 && t.links[0].href && t.links[0].id == "https://example.org/x" && t.links[0].firstElement == 3 && t.links[0].count == 4);
    HX_CHECK(Plain(t) == "go here!");
}

HX_TEST(RichText, UnknownTagsNoparseStylesAndPlainMode)
{
    ParsedText t;
    // An unknown tag and a bad value stay as text.
    ParseRichText("a<unknown>b<size=x>c", true, TextStyle(), t);
    HX_CHECK(Plain(t) == "a<unknown>b<size=x>c");
    ParseRichText("<noparse><b>x</b></noparse>y", true, TextStyle(), t);
    HX_CHECK(Plain(t) == "<b>x</b>y" && StyleAt(t, 3).flags == 0);
    Sheets sheets;
    ParseRichText("<style=\"H1\">T</style>t", true, TextStyle(), t, &sheets);
    HX_CHECK(Plain(t) == "Tt" && (StyleAt(t, 0).flags & TextFlags::Bold) && StyleAt(t, 0).size.unit == TextLength::Unit::Em && StyleAt(t, 1).flags == 0);
    // Rich text off: tags are characters.
    ParseRichText("<b>x</b>", false, TextStyle(), t);
    HX_CHECK(Plain(t) == "<b>x</b>" && t.styles.Size() == 1);
    // UTF-8: one element per code point with its source byte.
    ParseRichText("\xC3\xA9<b>\xE6\xBC\xA2</b>", true, TextStyle(), t);
    HX_CHECK(t.elements.Size() == 2 && t.elements[0].code == 0xE9 && t.elements[1].code == 0x6F22 && t.elements[1].source == 5);
}
