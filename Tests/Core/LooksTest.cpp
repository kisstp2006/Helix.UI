#include <Core/Support/ShapingFonts.h>

#include <Helix/Testing/Test.h>
#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/Markup/Hxml.h>
#include <Helix/UI/Rendering/UIMesh.h>
#include <Helix/UI/Styles/StyleEngine.h>
#include <Helix/UI/Styles/Transforms.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

#include <cmath>
#include <cstdio>

using namespace helix;
using namespace helix::ui;
using namespace helix::ui::testing;

// The looks of HSS beyond boxes: transforms (drawn, clipped and picked through), backgrounds placed, sized and
// repeated, text shadows and outlines, the ellipsis's place, cursors, custom properties and imported sheets.
namespace
{
    bool Near(f32 a, f32 b, f32 e = 0.01f) { return std::fabs(a - b) < e; }

    // Markup laid out in 200 x 200 pixels.
    struct Page
    {
        ElementTree tree;
        LayoutEngine layout{tree};
        StyleEngine styles{tree, layout};
        EventSystem events{tree, layout, styles};
        ElementHandle root;

        explicit Page(StringView markup)
        {
            root = tree.Create(Intern("Element"));
            tree.SetPicking(root, PickingMode::Ignore);
            events.SetRoot(root);
            HxmlDocument doc;
            Array<HxmlError> errors;
            HxmlDocument::Parse(markup, doc, errors);
            Instantiate(doc, tree, styles, root, nullptr, errors);
            for (const HxmlError& e : errors)
                std::printf("    %u:%u %s\n", e.line, e.column, e.message.CStr());
            Update();
        }

        void Update(f32 dt = 0)
        {
            styles.Update(root, dt);
            layout.Update(root, 200, 200);
        }

        ElementHandle Named(StringView name) const
        {
            ElementHandle found;
            tree.ForEachInSubtree(root, [&](ElementHandle e, u32) {
                if (tree.Name(e) == Intern(name))
                    found = e;
            });
            return found;
        }

        const ComputedStyle& Style(StringView name) const { return styles.Computed(Named(name)); }
    };

    std::shared_ptr<const StyleSheetData> Sheet(StringView text)
    {
        auto sheet = std::make_shared<StyleSheetData>(StyleSheetData::Parse(text));
        for (const StyleError& e : sheet->Errors())
            std::printf("    %u:%u %s\n", e.line, e.column, e.message.CStr());
        return sheet;
    }
}

HX_TEST(Looks, TransformsTakeLengthsAnglesAndKeywordsAndTransitionValueByValue)
{
    Page p(R"(<HXML>
        <Element name="a" style="width: 40px; height: 20px; translate: 10px 50%; rotate: 0.25turn; scale: 2; transform-origin: left bottom;" />
        <Element name="b" style="translate: 5px; rotate: -90deg; scale: 1 3; transform-origin: 25% 10px 4px;
                                 transition: translate 1s linear, rotate 1s linear;" />
        <Element name="c" style="translate: none; scale: none; rotate: 100grad;" />
    </HXML>)");
    const ComputedStyle& a = p.Style("a");
    HX_CHECK(a.translate[0] == Length::Px(10) && a.translate[1] == Length::Percent(50) && Near(a.rotate, 90) && a.scale[0] == 2 && a.scale[1] == 2);
    HX_CHECK(a.transformOrigin[0] == Length::Percent(0) && a.transformOrigin[1] == Length::Percent(100));
    const ComputedStyle& b = p.Style("b");
    HX_CHECK(b.translate[0] == Length::Px(5) && b.translate[1] == Length::Px(0) && Near(b.rotate, -90) && b.scale[0] == 1 && b.scale[1] == 3);
    HX_CHECK(b.transformOrigin[0] == Length::Percent(25) && b.transformOrigin[1] == Length::Px(10));
    const ComputedStyle& c = p.Style("c");
    HX_CHECK(c.translate[0] == Length::Px(0) && c.scale[0] == 1 && c.scale[1] == 1 && Near(c.rotate, 90));
    // The world map: a's origin is its bottom left (0, 20); 10 px and half its height across, turned a quarter, doubled.
    Rect r = p.layout.WorldRect(p.Named("a"));
    Affine m = LocalTransform(a, r);
    Vector2 topRight = m.Apply(Vector2(r.x + 40, r.y));
    // (40, -20) from the origin turns to (40, 80) (clockwise on the screen, y down), moved by (10, 10).
    HX_CHECK(Near(topRight.x, r.x + 0 + 40 + 10, 0.05f) && Near(topRight.y, r.y + 20 + 80 + 10, 0.05f));
    // Bad values are errors; the rest still applies.
    StyleSheetData bad = StyleSheetData::Parse("A { rotate: 3px; scale: red; translate: 1px 2px 3px 4px; transform-origin: middle; width: 3px; }");
    HX_CHECK(bad.Errors().Size() == 4 && bad.Rules().Size() == 1 && bad.Rules()[0].declarations.Size() == 1);

    // A transition moves both values of translate and the angle.
    ElementHandle e = p.Named("b");
    p.styles.SetInlineProperty(e, FindProperty("translate"), "25px 40px");
    p.styles.SetInlineProperty(e, FindProperty("rotate"), "90deg");
    p.Update(0.5f);
    HX_CHECK(Near(p.styles.Computed(e).translate[0].value, 15) && Near(p.styles.Computed(e).translate[1].value, 20) && Near(p.styles.Computed(e).rotate, 0));
    p.Update(0.5f);
    HX_CHECK(Near(p.styles.Computed(e).translate[0].value, 25) && Near(p.styles.Computed(e).rotate, 90) && !p.styles.Animating());
}

HX_TEST(Looks, BackgroundsArePlacedSizedAndRepeated)
{
    Page p(R"(<HXML>
        <Element name="pos" style="background-position: right 10px bottom 20%; background-size: 30px auto; background-repeat: repeat-x;" />
        <Element name="two" style="background-position: top right; background-size: cover; background-repeat: space round;" />
        <Element name="mode" style="background-position-x: left 4px; -helix-background-scale-mode: scale-to-fit;" />
        <Element name="none" />
    </HXML>)");
    const ComputedStyle& pos = p.Style("pos");
    HX_CHECK(pos.backgroundPositionKeyword[0] == BackgroundPositionKeyword::Right && pos.backgroundPositionOffset[0] == Length::Px(10));
    HX_CHECK(pos.backgroundPositionKeyword[1] == BackgroundPositionKeyword::Bottom && pos.backgroundPositionOffset[1] == Length::Percent(20));
    HX_CHECK(pos.backgroundSizeType == BackgroundSizeType::Length && pos.backgroundSize[0] == Length::Px(30) && pos.backgroundSize[1].IsAuto());
    HX_CHECK(pos.backgroundRepeat[0] == BackgroundRepeat::Repeat && pos.backgroundRepeat[1] == BackgroundRepeat::NoRepeat);
    const ComputedStyle& two = p.Style("two");
    HX_CHECK(two.backgroundPositionKeyword[0] == BackgroundPositionKeyword::Right && two.backgroundPositionKeyword[1] == BackgroundPositionKeyword::Top);
    HX_CHECK(two.backgroundSizeType == BackgroundSizeType::Cover && two.backgroundRepeat[0] == BackgroundRepeat::Space &&
             two.backgroundRepeat[1] == BackgroundRepeat::Round);
    // The scale mode sets the longhands after it (the position's x again to the middle).
    const ComputedStyle& mode = p.Style("mode");
    HX_CHECK(mode.backgroundSizeType == BackgroundSizeType::Contain && mode.backgroundPositionKeyword[0] == BackgroundPositionKeyword::Center);
    // The initial background fills the box.
    const ComputedStyle& none = p.Style("none");
    HX_CHECK(none.backgroundSize[0] == Length::Percent(100) && none.backgroundRepeat[0] == BackgroundRepeat::NoRepeat);

    // The mesh: a 20 x 10 texture.
    int texture = 0;
    UIImage image;
    image.texture = &texture;
    image.textureWidth = 20;
    image.textureHeight = 10;
    const f32 none4[4] = {0, 0, 0, 0};
    auto quads = [](const UIMesh& m) { return m.Vertices().Size() / 4; };
    // Its own size at the bottom right, 5 pixels in.
    UIMesh m;
    image.size[0] = image.size[1] = Length::Auto();
    image.edge[0] = image.edge[1] = UIEdge::End;
    image.offset[0] = image.offset[1] = Length::Px(5);
    m.Image(Rect(0, 0, 100, 50), image, Vector4(), none4, 1);
    HX_REQUIRE(quads(m) == 1);
    HX_CHECK(Near(m.Vertices()[0].position.x, 75) && Near(m.Vertices()[0].position.y, 35) && Near(m.Vertices()[2].position.x, 95));
    // Repeated across from the middle: whole tiles through the box, the ends cut (uv cut with them).
    m.Clear();
    image.edge[0] = UIEdge::Center, image.offset[0] = Length::Px(0);
    image.repeat[0] = UIRepeat::Repeat;
    m.Image(Rect(0, 0, 50, 50), image, Vector4(), none4, 1);
    // Tiles from 15: [-5, 15) cut to [0, 15), [15, 35), [35, 55) cut to [35, 50).
    HX_REQUIRE(quads(m) == 3);
    HX_CHECK(Near(m.Vertices()[0].position.x, 0) && Near(m.Vertices()[0].uvLocal.x, 0.25f) && Near(m.Vertices()[4].position.x, 15));
    HX_CHECK(Near(m.Vertices()[8].position.x, 35) && Near(m.Vertices()[9].position.x, 50) && Near(m.Vertices()[9].uvLocal.x, 0.75f));
    // Round: as many as fit best, stretched (50 / 20 is 2.5: three tiles of 16.7).
    m.Clear();
    image.repeat[0] = UIRepeat::Round;
    m.Image(Rect(0, 0, 50, 50), image, Vector4(), none4, 1);
    HX_CHECK(quads(m) == 3 && Near(m.Vertices()[1].position.x, 50.0f / 3, 0.02f));
    // Space: two whole tiles at the ends.
    m.Clear();
    image.repeat[0] = UIRepeat::Space;
    m.Image(Rect(0, 0, 50, 50), image, Vector4(), none4, 1);
    HX_CHECK(quads(m) == 2 && Near(m.Vertices()[0].position.x, 0) && Near(m.Vertices()[5].position.x, 50));
}

HX_TEST(Looks, TheMeshMapsGeometryAndKeepsTheClipInItsElementsSpace)
{
    UIMesh m;
    // A quarter turn about (50, 50), scaled twice.
    Affine turn = Affine::Translation(Vector2(50, 50)) * Affine::Rotation(90) * Affine::Scaling(Vector2(2, 2)) * Affine::Translation(Vector2(-50, -50));
    m.SetTransform(turn);
    UIShape box;
    box.rect = Rect(50, 50, 10, 4);
    box.radii = Vector4(1, 1, 1, 1);
    box.fill = Color(1, 1, 1, 1);
    m.Shape(box, 1);
    HX_REQUIRE(m.Vertices().Size() == 4);
    // The top right corner (60, 50) goes to (50, 70); the box's distances are pixels, twice its own.
    HX_CHECK(Near(m.Vertices()[1].position.x, 50) && Near(m.Vertices()[1].position.y, 70));
    HX_CHECK(Near(m.Vertices()[1].size.x, 20) && Near(m.Vertices()[1].uvLocal.z, 20) && Near(m.Vertices()[1].radii.x, 2));

    // A clip of the turned element: a vertex's place in it is in the element's space (in pixels).
    m.Clear();
    UIClip clip = UIClip().Within(Rect(50, 50, 10, 4), Vector4(), turn);
    HX_CHECK(Near(clip.rect.x, 50) && Near(clip.rect.width, 10));
    m.SetClip(clip);
    m.SetTransform(turn);
    m.Rectangle(Rect(55, 51, 1, 1), Color(1, 1, 1, 1));
    const UIVertex& v = m.Vertices()[0];
    // (55, 51) is 5 and 1 into the clip, twice that in pixels; the clip 20 x 8 pixels.
    HX_CHECK(Near(v.clipRect.x, 10) && Near(v.clipRect.y, 2) && Near(v.clipRect.z, 20) && Near(v.clipRect.w, 8));
    // An inner clip of an element that does not turn against it takes the outer one exactly.
    UIClip inner = clip.Within(Rect(52, 48, 20, 4), Vector4(), turn);
    HX_CHECK(Near(inner.rect.x, 52) && Near(inner.rect.y, 50) && Near(inner.rect.width, 8) && Near(inner.rect.height, 2));
}

HX_TEST(Looks, PickingAndLocalPositionsGoThroughTransforms)
{
    Page p(R"(<HXML>
        <Element name="bar" style="position: absolute; left: 100px; top: 100px; width: 80px; height: 10px; rotate: 90deg;">
            <Element name="tip" style="position: absolute; left: 70px; top: 0; width: 10px; height: 10px;" />
        </Element>
        <Element name="clip" style="position: absolute; left: 0; top: 0; width: 40px; height: 40px; overflow: hidden; scale: 0.5; transform-origin: 0 0;">
            <Element name="inside" style="width: 60px; height: 60px;" />
        </Element>
    </HXML>)");
    auto at = [&](f32 x, f32 y) { return AtomName(p.tree.Name(p.events.Pick(Vector2(x, y)))); };
    // The bar turns about its middle (140, 105): it now stands from y 65 to 145 at x 135 to 145, its tip at the bottom.
    HX_CHECK(at(140, 70) == StringView("bar") && at(140, 140) == StringView("tip") && at(170, 105) == StringView());
    // The clip is drawn at half size from its top left: its 40 pixels cover 20; what is outside of it is not picked.
    HX_CHECK(at(15, 15) == StringView("inside") && at(25, 25) == StringView());
    // The local position of an event in the turned element.
    Array<String> seen;
    struct Log
    {
        static void Down(void* user, EventSystem&, UIEvent& e)
        {
            auto& out = *static_cast<Array<String>*>(user);
            char text[64];
            std::snprintf(text, sizeof(text), "%.0f,%.0f", e.localPosition.x, e.localPosition.y);
            out.Push(String(text));
        }
    };
    p.events.RegisterCallback(p.Named("bar"), Events().pointerDown, &Log::Down, &seen);
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(140, 70), 0, 0, false);
    HX_CHECK(seen.Size() == 1 && seen[0] == StringView("5,5"));
}

HX_TEST(Looks, TextShadowsOutlinesCursorsAndClipBoxes)
{
    Page p(R"(<HXML>
        <Element name="outer" style="text-shadow: 2px 3px 4px rgba(0, 0, 0, 0.5); -helix-text-outline: 1px #ff0000; cursor: link;">
            <Element name="inner" style="-helix-text-overflow-position: middle; -helix-overflow-clip-box: content-box;" />
        </Element>
        <Element name="image" style="cursor: url(&quot;Images/Hand.png&quot;) 4 6; text-shadow: #00ff00 1px 1px;" />
        <Element name="none" style="text-shadow: none;" />
    </HXML>)");
    const ComputedStyle& o = p.Style("outer");
    HX_CHECK(Near(o.textShadowOffset[0], 2) && Near(o.textShadowOffset[1], 3) && Near(o.textShadowBlur, 4) && Near(o.textShadowColor.a, 0.5f));
    HX_CHECK(Near(o.textOutlineWidth, 1) && Near(o.textOutlineColor.r, 1) && Near(o.textOutlineColor.g, 0));
    HX_CHECK(o.cursorSet && o.cursorShape == CursorShape::Link && o.cursorImage == kNoAtom);
    // Shadows, outlines and cursors are inherited; the overflow places are not.
    const ComputedStyle& i = p.Style("inner");
    HX_CHECK(Near(i.textShadowBlur, 4) && Near(i.textOutlineWidth, 1) && i.cursorSet && i.cursorShape == CursorShape::Link);
    HX_CHECK(i.textOverflowPosition == TextOverflowPosition::Middle && i.overflowClipBox == OverflowClipBox::ContentBox);
    HX_CHECK(o.textOverflowPosition == TextOverflowPosition::End);
    const ComputedStyle& image = p.Style("image");
    HX_CHECK(image.cursorSet && image.cursorImage == Intern("Images/Hand.png") && Near(image.cursorHotspot[0], 4) && Near(image.cursorHotspot[1], 6));
    HX_CHECK(Near(image.textShadowBlur, 0) && Near(image.textShadowColor.g, 1));
    HX_CHECK(Near(p.Style("none").textShadowColor.a, 0));
    StyleSheetData bad = StyleSheetData::Parse("A { cursor: url(\"a.png\") 4; text-shadow: 1px; cursor: banana; }");
    HX_CHECK(bad.Errors().Size() == 3);
}

namespace
{
    std::shared_ptr<const Face> EllipsisFace()
    {
        FontSpec s;
        s.glyphs.Resize(1);
        auto add = [&](u32 code, u16 advance) {
            GlyphSpec g;
            g.advance = advance;
            g.contours.Push(Square(50, 0, 450, 500));
            s.glyphs.Push(g);
            s.cmap.Push({code, u32(s.glyphs.Size() - 1)});
        };
        for (u32 c = 'a'; c <= 'z'; c++)
            add(c, 500);
        add(0x2026, 600);
        s.names = {{String("1"), String("Ellipsis Test")}, {String("2"), String("Regular")}};
        Array<u8> bytes = BuildFont(s);
        auto face = std::make_shared<Face>();
        String error;
        face->Open(Span<const u8>(bytes.Data(), bytes.Size()), 0, error);
        return face;
    }
}

HX_TEST(Looks, TheEllipsisCutsAtTheEndTheStartOrTheMiddle)
{
    // Eight letters of 5 pixels in 20; the ellipsis is 6 pixels, so 14 are left for letters.
    TextFont font(EllipsisFace(), GlyphRenderMode::Sdf, 32, 4, 256);
    TextGenerator gen;
    TextInfo info;
    TextGenerationSettings s;
    s.font = &font;
    s.fontSize = 10;
    s.width = 20;
    s.height = 20;
    s.wrapping = TextWrappingMode::NoWrap;
    s.overflow = TextOverflowMode::Ellipsis;
    auto shown = [&] {
        String out;
        for (const CharacterInfo& c : info.characterInfo)
            if (c.isVisible)
                out.Append(char(c.character));
        return out;
    };
    gen.Generate("abcdefgh", s, info);
    HX_CHECK(shown() == StringView("ab") && info.overflowed);
    s.ellipsisPosition = TextEllipsisPosition::Start;
    gen.Generate("abcdefgh", s, info);
    // The tail after the ellipsis.
    HX_CHECK(shown() == StringView("gh") && Near(info.characterInfo[6].origin, 6) && Near(info.quads.Back().corners[0].x, 0, 1));
    s.ellipsisPosition = TextEllipsisPosition::Middle;
    gen.Generate("abcdefgh", s, info);
    // Half the room for the head (a), the rest for the tail (h), the ellipsis between.
    HX_CHECK(shown() == StringView("ah") && Near(info.characterInfo[7].origin, 11));
}

HX_TEST(Looks, CustomPropertiesTellScriptsWhenTheyAreResolved)
{
    Page p(R"(<HXML>
        <Element name="card" class="card" style="--accent: var(--base); --size: 12px;" />
    </HXML>)");
    p.styles.AddStyleSheet(p.root, Sheet(":root { --base: #336699; } .card { --title: \"Hello\"; --flag: true; } .big { --size: 20px; }"));
    p.Update();
    ElementHandle card = p.Named("card");
    String text;
    HX_CHECK(p.styles.CustomProperty(card, "--accent", text) && text == StringView("#336699"));
    HX_CHECK(p.styles.CustomProperty(card, "--title", text) && text == StringView("\"Hello\""));
    // Declared by an ancestor only: not the element's own.
    HX_CHECK(!p.styles.CustomProperty(card, "--base", text) && p.styles.CustomProperty(p.root, "--base", text));
    Array<Atom> names;
    p.styles.CustomPropertyNames(card, names);
    HX_CHECK(names.Size() == 4);
    // Computed again (a class): the element is told, its inline value still wins over the class's.
    p.tree.AddClass(card, Intern("big"));
    p.Update();
    HX_CHECK(p.styles.CustomResolved().Size() == 1 && p.styles.CustomResolved()[0] == card);
    HX_CHECK(p.styles.CustomProperty(card, "--size", text) && text == StringView("12px"));
    // Nothing computed: nobody is told; an element without custom properties is not told either.
    p.Update();
    HX_CHECK(p.styles.CustomResolved().Empty());
    ElementHandle plain = p.tree.Create(Intern("Element"));
    p.tree.Append(p.root, plain);
    p.Update();
    for (ElementHandle e : p.styles.CustomResolved())
        HX_CHECK(e != plain);
}

HX_TEST(Looks, ImportedSheetsComeBeforeTheSheetsOwnRules)
{
    auto base = Sheet("Element { width: 10px; height: 10px; } .a { width: 20px; }");
    auto theme = std::make_shared<StyleSheetData>(StyleSheetData::Parse(R"(
        @import url("base.hss");
        @import "colors.hss";
        :root { --accent: red; }
        .a { height: 30px; }
    )"));
    HX_CHECK(theme->Errors().Empty());
    HX_REQUIRE(theme->Imports().Size() == 2);
    HX_CHECK(theme->Imports()[0] == StringView("base.hss") && theme->Imports()[1] == StringView("colors.hss"));
    Array<std::shared_ptr<const StyleSheetData>> found;
    found.Push(base);
    theme->SetImported(Move(found));
    Page p(R"(<HXML><Element name="x" class="a" /></HXML>)");
    p.styles.AddStyleSheet(p.root, theme);
    p.Update();
    // The import's rules apply, the theme's own win at the same specificity.
    const ComputedStyle& x = p.Style("x");
    HX_CHECK(x.layout.width == Length::Px(20) && x.layout.height == Length::Px(30));
    // @import after a rule, and other at-rules, are errors.
    StyleSheetData late = StyleSheetData::Parse("A { width: 1px; } @import \"x.hss\"; @media screen { A { width: 2px; } } B { width: 3px; }");
    HX_CHECK(late.Errors().Size() == 2 && late.Imports().Empty() && late.Rules().Size() == 2);
}

HX_TEST(Looks, VariablesInShorthandsAreReplacedBeforeTheyExpand)
{
    Page p(R"(<HXML><Element name="a" class="a" /></HXML>)");
    p.styles.AddStyleSheet(p.root, Sheet(":root { --c: #ff0000; --w: 3px; } .a { border-color: var(--c); border-width: var(--w) 1px; "
                                         "margin: var(--m, 4px) 2px; transition: opacity var(--t, 2s) linear; }"));
    p.Update();
    const ComputedStyle& a = p.Style("a");
    HX_CHECK(Near(a.borderColor[kLeft].r, 1) && Near(a.borderColor[kBottom].r, 1) && Near(a.borderColor[kTop].g, 0));
    HX_CHECK(Near(a.layout.border[kTop], 3) && Near(a.layout.border[kLeft], 1));
    HX_CHECK(a.layout.margin[kTop] == Length::Px(4) && a.layout.margin[kRight] == Length::Px(2));
    HX_CHECK(a.transitionCount == 1 && Near(a.transitions[0].duration, 2));
}
