#pragma once

#include <Helix/UI/Markup/Xml.h>
#include <Helix/UI/Styles/StyleEngine.h>

// HXML: element trees in markup. The root is <HXML>; <Style src="..."/> attaches a style sheet (to the document's
// elements at the root, to its parent element inside one); <Template name="..." src="..."/> names another document,
// which <Instance template="..."> builds in place, with <AttributeOverrides element-name="..." attribute="value"/>
// changing attributes of its named elements. Other elements are element types from a registry by name; every element
// takes name, class (names separated by spaces) and style (an inline style), and its type reads the rest.
namespace helix::ui
{
    struct HxmlError
    {
        u32 line = 0, column = 0;
        String message;
    };

    class Painter;

    // Reads one attribute of its type; false when the type does not know it.
    using AttributeHandler = bool (*)(ElementTree& tree, ElementHandle element, StringView name, StringView value);
    // Paints an element's own content (vector paths) in its content box (pixels from the box's top left).
    using PaintHandler = void (*)(Painter& painter, const ElementTree& tree, ElementHandle element, f32 width, f32 height);

    struct ElementType
    {
        Atom name = kNoAtom;
        AttributeHandler attributes = nullptr;
        PaintHandler paint = nullptr;
    };

    // Registers (or replaces) an element type; Element, TemplateContainer and Label are there from the start.
    void RegisterElementType(StringView name, AttributeHandler attributes = nullptr, PaintHandler paint = nullptr);
    const ElementType* FindElementType(Atom name);

    struct HxmlAttribute
    {
        Atom name = kNoAtom;
        String value;
    };

    struct HxmlNode
    {
        // Element types, or the markup's own Style, Template, Instance and AttributeOverrides.
        Atom type = kNoAtom;
        Array<HxmlAttribute> attributes;
        Array<HxmlNode> children;
        String text;
        u32 line = 0, column = 0;

        const String* Attribute(StringView name) const;
    };

    class HxmlDocument
    {
    public:
        // Parses a document; false with the reason when it is no HXML at all.
        static bool Parse(StringView text, HxmlDocument& out, Array<HxmlError>& errors);

        Span<const HxmlNode> Nodes() const { return Span<const HxmlNode>(nodes_.Data(), nodes_.Size()); }

    private:
        Array<HxmlNode> nodes_;
    };

    // What markup refers to by path: other documents and style sheets (null when missing).
    class HxmlResolver
    {
    public:
        virtual ~HxmlResolver() = default;
        virtual const HxmlDocument* Template(StringView src) = 0;
        virtual std::shared_ptr<const StyleSheet> Style(StringView src) = 0;
    };

    // Builds a document's elements under parent. What cannot be built (unknown types and attributes, missing
    // templates and sheets) is reported and left out; the rest is built.
    void Instantiate(const HxmlDocument& document, ElementTree& tree, StyleEngine& styles, ElementHandle parent, HxmlResolver* resolver,
                     Array<HxmlError>& errors);
}
