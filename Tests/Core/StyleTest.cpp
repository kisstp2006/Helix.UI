#include <Helix/UI/Styles/StyleEngine.h>
#include <Helix/Testing/Test.h>

#include <cmath>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

namespace
{
    bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.002f; }
    bool SameColor(Color a, Color b) { return Near(a.r, b.r) && Near(a.g, b.g) && Near(a.b, b.b) && Near(a.a, b.a); }

    struct Fixture
    {
        ElementTree tree;
        LayoutEngine layout{tree};
        StyleEngine styles{tree, layout};
        ElementHandle root;

        explicit Fixture(StringView sheet)
        {
            root = tree.Create(Intern("Element"));
            auto parsed = std::make_shared<StyleSheet>(StyleSheet::Parse(sheet));
            for (const StyleError& e : parsed->Errors())
                std::printf("    %u:%u %s\n", e.line, e.column, e.message.CStr());
            styles.AddStyleSheet(root, parsed);
        }

        ElementHandle Add(ElementHandle parent, StringView type, StringView name = {}, StringView classes = {})
        {
            ElementHandle e = tree.Create(Intern(type));
            tree.Append(parent, e);
            if (!name.Empty())
                tree.SetName(e, Intern(name));
            usize at = 0;
            while (at < classes.Size())
            {
                usize space = classes.Find(' ', at);
                if (space == StringView::npos)
                    space = classes.Size();
                tree.AddClass(e, Intern(classes.Substr(at, space - at)));
                at = space + 1;
            }
            return e;
        }
    };
}

HX_TEST(Hss, ParsesRulesSelectorsAndShorthands)
{
    StyleSheet sheet = StyleSheet::Parse(R"(
        /* a comment */
        Button, .big > #ok:hover {
            width: 120px; height: 50%;
            margin: 1px 2px 3px;
            border-color: #f00 rgb(0, 255, 0) blue transparent;
            flex: 2 0 10px;
        }
        Label .note { color: #11223344; }
    )");
    HX_CHECK(sheet.Errors().Empty());
    HX_REQUIRE(sheet.Rules().Size() == 2);
    const StyleRule& r = sheet.Rules()[0];
    HX_REQUIRE(r.selectors.Size() == 2);
    HX_CHECK(r.selectors[0].parts.Size() == 1 && r.selectors[0].parts[0].type == Intern("Button") && r.selectors[0].specificity == 1);
    const Selector& second = r.selectors[1];
    HX_REQUIRE(second.parts.Size() == 2);
    HX_CHECK(second.parts[0].name == Intern("ok") && Any(second.parts[0].required & PseudoStates::Hover) && second.parts[0].left == Combinator::Child);
    HX_CHECK(second.parts[1].classes.Size() == 1 && second.parts[1].classes[0] == Intern("big"));
    // One name, one class and one pseudo state.
    HX_CHECK(second.specificity == (1u << 16 | 2u << 8));
    // margin: 1 2 3 is top 1, right 2, bottom 3, left 2; border-color four sides; flex three longhands.
    ComputedStyle s;
    for (const Declaration& d : r.declarations)
        Property(d.property).set(s, Span<const StyleValue>(d.values.Data(), d.values.Size()));
    HX_CHECK(s.layout.width == Length::Px(120) && s.layout.height == Length::Percent(50));
    HX_CHECK(s.layout.margin[kTop] == Length::Px(1) && s.layout.margin[kRight] == Length::Px(2) && s.layout.margin[kBottom] == Length::Px(3) &&
             s.layout.margin[kLeft] == Length::Px(2));
    HX_CHECK(SameColor(s.borderColor[kTop], Color(1, 0, 0, 1)) && SameColor(s.borderColor[kRight], Color(0, 1, 0, 1)) &&
             SameColor(s.borderColor[kBottom], Color(0, 0, 1, 1)) && SameColor(s.borderColor[kLeft], Color(0, 0, 0, 0)));
    HX_CHECK(s.layout.grow == 2 && s.layout.shrink == 0 && s.layout.basis == Length::Px(10));
    const Selector& descendant = sheet.Rules()[1].selectors[0];
    HX_CHECK(descendant.parts.Size() == 2 && descendant.parts[0].left == Combinator::Descendant && descendant.ancestorAtoms.Size() == 1);
}

HX_TEST(Hss, ErrorsCarryTheirLineAndColumnAndTheRestStillApplies)
{
    StyleSheet sheet = StyleSheet::Parse("Box {\n  width: 10px;\n  colour: red;\n  height: purple;\n}\n.ok { opacity: 0.5; }\n");
    HX_REQUIRE(sheet.Errors().Size() == 2);
    HX_CHECK(sheet.Errors()[0].line == 3 && sheet.Errors()[0].column == 3 && sheet.Errors()[0].message.View().Contains("colour"));
    HX_CHECK(sheet.Errors()[1].line == 4 && sheet.Errors()[1].message.View().Contains("purple"));
    HX_REQUIRE(sheet.Rules().Size() == 2);
    HX_CHECK(sheet.Rules()[0].declarations.Size() == 1 && sheet.Rules()[1].declarations.Size() == 1);
}

HX_TEST(Hss, TheCascadeBySpecificitySheetsAndInlineStyles)
{
    Fixture f(R"(
        Box { width: 10px; opacity: 0.1; }
        .wide { width: 20px; }
        #main { width: 30px; }
        Box.wide { opacity: 0.2; }
        Panel > Box { height: 5px; }
        Panel Box { background-color: red; }
        Box:hover { opacity: 0.9; }
    )");
    ElementHandle panel = f.Add(f.root, "Panel");
    ElementHandle a = f.Add(panel, "Box", {}, "wide");
    ElementHandle b = f.Add(panel, "Box", "main", "wide");
    ElementHandle wrapper = f.Add(panel, "Element");
    ElementHandle c = f.Add(wrapper, "Box");
    f.styles.Update(f.root, 0);
    HX_CHECK(f.styles.Computed(a).layout.width == Length::Px(20) && Near(f.styles.Computed(a).opacity, 0.2f));
    HX_CHECK(f.styles.Computed(b).layout.width == Length::Px(30));
    HX_CHECK(f.styles.Computed(c).layout.width == Length::Px(10) && Near(f.styles.Computed(c).opacity, 0.1f));
    // Child combinator: c is not a child of the panel; descendant: it is under it.
    HX_CHECK(f.styles.Computed(a).layout.height == Length::Px(5) && f.styles.Computed(c).layout.height.IsAuto());
    HX_CHECK(SameColor(f.styles.Computed(c).backgroundColor, Color(1, 0, 0, 1)));

    // A pseudo state; a sheet on the panel wins over the root's at the same specificity; the inline style wins over all.
    f.tree.SetStates(c, PseudoStates::Hover);
    f.styles.AddStyleSheet(panel, std::make_shared<StyleSheet>(StyleSheet::Parse("Box { width: 40px; }")));
    f.styles.SetInlineStyle(b, "width: 50px; opacity: 0.4");
    f.styles.Update(f.root, 0);
    HX_CHECK(Near(f.styles.Computed(c).opacity, 0.9f) && f.styles.Computed(c).layout.width == Length::Px(40));
    HX_CHECK(f.styles.Computed(a).layout.width == Length::Px(20));
    HX_CHECK(f.styles.Computed(b).layout.width == Length::Px(50) && Near(f.styles.Computed(b).opacity, 0.4f));
    // :root is the element without a parent.
    Fixture g(":root { opacity: 0.3; } Box:enabled { opacity: 0.6; } Box:disabled { opacity: 0.7; }");
    ElementHandle on = g.Add(g.root, "Box"), off = g.Add(g.root, "Box");
    g.tree.SetStates(off, PseudoStates::Disabled);
    g.styles.Update(g.root, 0);
    HX_CHECK(Near(g.styles.Computed(g.root).opacity, 0.3f) && Near(g.styles.Computed(on).opacity, 0.6f) && Near(g.styles.Computed(off).opacity, 0.7f));
}

HX_TEST(Hss, InheritanceVariablesAndInitialValues)
{
    Fixture f(R"(
        Panel { color: #0000ff; font-size: 20px; --gap: 7px; --tint: #00ff00; }
        Box { margin-left: var(--gap); background-color: var(--missing, red); border-left-color: var(--tint); }
        .reset { color: initial; }
    )");
    ElementHandle panel = f.Add(f.root, "Panel");
    ElementHandle box = f.Add(panel, "Box");
    ElementHandle reset = f.Add(box, "Element", {}, "reset");
    f.styles.Update(f.root, 0);
    const ComputedStyle& s = f.styles.Computed(box);
    HX_CHECK(SameColor(s.color, Color(0, 0, 1, 1)) && s.fontSize == Length::Px(20));
    HX_CHECK(s.layout.margin[kLeft] == Length::Px(7) && SameColor(s.backgroundColor, Color(1, 0, 0, 1)) && SameColor(s.borderColor[kLeft], Color(0, 1, 0, 1)));
    // Not inherited: margins; initial: the property's start value.
    HX_CHECK(f.styles.Computed(reset).layout.margin[kLeft] == Length::Px(0) && SameColor(f.styles.Computed(reset).color, Color(0, 0, 0, 1)));
    HX_CHECK(f.styles.Computed(reset).fontSize == Length::Px(20));
}

HX_TEST(Hss, OnlyDirtyStylesAreComputedAndLayoutInputsFollow)
{
    Fixture f(".big { width: 100px; } Box { height: 10px; }");
    ElementHandle boxes[10];
    for (auto& b : boxes)
        b = f.Add(f.root, "Box");
    f.styles.Update(f.root, 0);
    HX_CHECK(f.styles.LastComputed() == 11);
    f.styles.Update(f.root, 0);
    HX_CHECK(f.styles.LastComputed() == 0);
    f.tree.AddClass(boxes[3], Intern("big"));
    f.styles.Update(f.root, 0);
    HX_CHECK(f.styles.LastComputed() == 1);
    f.layout.Update(f.root, 300, 300);
    HX_CHECK(Near(f.layout.Box(boxes[3]).width, 100) && Near(f.layout.Box(boxes[3]).height, 10) && Near(f.layout.Box(boxes[4]).y, 40));
}

HX_TEST(Hss, TransitionsMoveValuesOverTime)
{
    Fixture f(R"(
        Box { width: 100px; opacity: 1; background-color: #000000; transition: width 1s linear, opacity 0.5s linear 0.5s; }
        Box.wide { width: 200px; opacity: 0; background-color: #ffffff; }
    )");
    ElementHandle box = f.Add(f.root, "Box");
    f.styles.Update(f.root, 0);
    f.tree.AddClass(box, Intern("wide"));
    f.styles.Update(f.root, 0.25f);
    HX_CHECK(Near(f.styles.Computed(box).layout.width.value, 125) && Near(f.styles.Computed(box).opacity, 1));
    // No transition for the background: at once.
    HX_CHECK(SameColor(f.styles.Computed(box).backgroundColor, Color(1, 1, 1, 1)));
    HX_CHECK(f.styles.Animating());
    f.styles.Update(f.root, 0.5f);
    HX_CHECK(Near(f.styles.Computed(box).layout.width.value, 175) && Near(f.styles.Computed(box).opacity, 0.5f));
    f.styles.Update(f.root, 0.5f);
    HX_CHECK(Near(f.styles.Computed(box).layout.width.value, 200) && Near(f.styles.Computed(box).opacity, 0) && !f.styles.Animating());
    // The layout saw the animated width.
    f.layout.Update(f.root, 400, 400);
    HX_CHECK(Near(f.layout.Box(box).width, 200));
    // Easing curves: ends fixed, ease-in slow at first.
    HX_CHECK(Near(Ease(Easing::EaseIn, 0), 0) && Near(Ease(Easing::EaseIn, 1), 1) && Ease(Easing::EaseIn, 0.25f) < 0.25f && Ease(Easing::EaseOut, 0.25f) > 0.25f);
}
