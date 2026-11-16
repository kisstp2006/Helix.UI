#include <Helix/UI/Markup/Hxml.h>
#include <Helix/Testing/Test.h>

#include <cmath>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

HX_TEST(Xml, ElementsAttributesTextAndReferences)
{
    XmlError error;
    auto root = ParseXml(R"(<?xml version="1.0"?>
<!-- a comment -->
<root a="1" b='two &amp; &lt;3&gt;'>
  <child name="x"/>
  text &#65;&#x42;
  <child><!-- inside --></child>
</root>)",
                         error);
    HX_REQUIRE(root != nullptr);
    HX_CHECK(root->name == StringView("root") && root->attributes.Size() == 2 && *root->Attribute("b") == StringView("two & <3>"));
    HX_CHECK(root->children.Size() == 2 && *root->children[0]->Attribute("name") == StringView("x") && root->text == StringView("text AB"));
    HX_CHECK(root->children[0]->line == 4 && root->children[0]->column == 3);

    auto broken = ParseXml("<a>\n  <b>\n</a>", error);
    HX_CHECK(broken == nullptr && error.line == 3 && error.message.View().Contains("</a> closes <b>"));
    HX_CHECK(ParseXml("<a x=1/>", error) == nullptr && error.message.View().Contains("quoted"));
    HX_CHECK(ParseXml("<a x='1' x='2'/>", error) == nullptr && error.message.View().Contains("twice"));
}

namespace
{
    struct Files : HxmlResolver
    {
        HashMap<StringView, HxmlDocument> documents;
        HashMap<StringView, std::shared_ptr<const StyleSheet>> sheets;

        const HxmlDocument* Template(StringView src) override { return documents.Find(src); }
        std::shared_ptr<const StyleSheet> Style(StringView src) override
        {
            auto* s = sheets.Find(src);
            return s ? *s : nullptr;
        }
    };

    struct Fixture
    {
        ElementTree tree;
        LayoutEngine layout{tree};
        StyleEngine styles{tree, layout};
        ElementHandle root = tree.Create(Intern("Element"));

        ElementHandle Find(StringView name)
        {
            ElementHandle found;
            tree.ForEachInSubtree(root, [&](ElementHandle e, u32) {
                if (tree.Name(e) == FindAtom(name))
                    found = e;
            });
            return found;
        }
    };

    // A type with an attribute of its own: count="n" adds n children.
    bool RepeatAttributes(ElementTree& tree, ElementHandle e, StringView name, StringView value)
    {
        if (name != StringView("count"))
            return false;
        for (char c = '0'; c < value[0]; c++)
            tree.Append(e, tree.Create(Intern("Element")));
        return true;
    }
}

HX_TEST(Hxml, ElementsWithNamesClassesStylesAndSheets)
{
    Files files;
    files.sheets.Set("page.hss", std::make_shared<StyleSheet>(StyleSheet::Parse(".row { flex-direction: row; } #title { height: 30px; }")));
    HxmlDocument doc;
    Array<HxmlError> errors;
    HX_REQUIRE(HxmlDocument::Parse(R"(<HXML xmlns="Helix.UI">
  <Style src="page.hss"/>
  <Element name="title" class="big  bold"/>
  <Element class="row" style="width: 100px; opacity: 0.5">
    <Element name="cell"/>
  </Element>
</HXML>)",
                                   doc, errors));
    Fixture f;
    Instantiate(doc, f.tree, f.styles, f.root, &files, errors);
    for (const HxmlError& e : errors)
        std::printf("    %u:%u %s\n", e.line, e.column, e.message.CStr());
    HX_CHECK(errors.Empty());
    ElementHandle title = f.Find("title");
    HX_REQUIRE(title.IsValid());
    HX_CHECK(f.tree.Classes(title).Size() == 2 && f.tree.HasClass(title, Intern("bold")) && f.tree.ChildCount(f.root) == 2);
    f.styles.Update(f.root, 0);
    ElementHandle row = f.tree.ChildAt(f.root, 1);
    HX_CHECK(f.styles.Computed(title).layout.height == Length::Px(30));
    HX_CHECK(f.styles.Computed(row).layout.direction == FlexDirection::Row && f.styles.Computed(row).layout.width == Length::Px(100) &&
             std::fabs(f.styles.Computed(row).opacity - 0.5f) < 1e-4f);
    HX_CHECK(f.tree.Parent(f.Find("cell")) == row);
}

HX_TEST(Hxml, TemplatesInstancesAndOverrides)
{
    Files files;
    Array<HxmlError> errors;
    HxmlDocument card;
    HX_REQUIRE(HxmlDocument::Parse(R"(<HXML><Element name="title" class="a"/><Element name="body"/></HXML>)", card, errors));
    files.documents.Set("card.hxml", Move(card));
    // A template that holds itself is built once.
    HxmlDocument loop;
    HX_REQUIRE(HxmlDocument::Parse(R"(<HXML><Template name="Me" src="loop.hxml"/><Element name="inner"/><Instance template="Me"/></HXML>)", loop, errors));
    files.documents.Set("loop.hxml", Move(loop));
    HxmlDocument page;
    HX_REQUIRE(HxmlDocument::Parse(R"(<HXML>
  <Template name="Card" src="card.hxml"/>
  <Template name="Loop" src="loop.hxml"/>
  <Instance template="Card" name="first" class="card">
    <AttributeOverrides element-name="title" class="b"/>
  </Instance>
  <Instance template="Card"/>
  <Instance template="Loop"/>
</HXML>)",
                                   page, errors));
    Fixture f;
    Instantiate(page, f.tree, f.styles, f.root, &files, errors);
    HX_CHECK(errors.Empty());
    HX_REQUIRE(f.tree.ChildCount(f.root) == 3);
    ElementHandle first = f.tree.ChildAt(f.root, 0);
    HX_CHECK(f.tree.Type(first) == Intern("TemplateContainer") && f.tree.Name(first) == Intern("first") && f.tree.HasClass(first, Intern("card")));
    ElementHandle title = f.tree.ChildAt(first, 0);
    HX_CHECK(f.tree.ChildCount(first) == 2 && f.tree.HasClass(title, Intern("b")) && !f.tree.HasClass(title, Intern("a")));
    // The second instance is not overridden.
    HX_CHECK(f.tree.HasClass(f.tree.ChildAt(f.tree.ChildAt(f.root, 1), 0), Intern("a")));
    ElementHandle looped = f.tree.ChildAt(f.root, 2);
    HX_CHECK(f.tree.ChildCount(looped) == 2 && f.tree.ChildCount(f.tree.ChildAt(looped, 1)) == 0);
}

HX_TEST(Hxml, TypesFromTheRegistryAndReportedMistakes)
{
    RegisterElementType("Repeat", &RepeatAttributes);
    Files files;
    Array<HxmlError> errors;
    HxmlDocument doc;
    HX_REQUIRE(HxmlDocument::Parse(R"(<HXML>
  <Repeat count="3"/>
  <Nonsense/>
  <Element colour="red" style="width: big"/>
  <Instance template="Missing"/>
  <Style src="none.hss"/>
</HXML>)",
                                   doc, errors));
    Fixture f;
    Instantiate(doc, f.tree, f.styles, f.root, &files, errors);
    HX_REQUIRE(errors.Size() == 5);
    HX_CHECK(errors[0].line == 3 && errors[0].message.View().Contains("<Nonsense>"));
    HX_CHECK(errors[1].line == 4 && errors[1].message.View().Contains("colour"));
    HX_CHECK(errors[2].line == 4 && errors[2].message.View().Contains("big"));
    HX_CHECK(errors[3].message.View().Contains("Missing") && errors[4].message.View().Contains("none.hss"));
    // The rest was built: Repeat with its 3 children, the Element.
    HX_CHECK(f.tree.ChildCount(f.root) == 2 && f.tree.ChildCount(f.tree.ChildAt(f.root, 0)) == 3);

    HxmlDocument wrong;
    Array<HxmlError> rootErrors;
    HX_CHECK(!HxmlDocument::Parse("<Element/>", wrong, rootErrors) && rootErrors.Size() == 1 && rootErrors[0].message.View().Contains("<HXML>"));
}
