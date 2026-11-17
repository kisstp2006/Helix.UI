#pragma once

#include <Helix/UI/Markup/Xml.h>
#include <Helix/UI/Styles/StyleEngine.h>

// HXML: element trees in markup. The root is <HXML>; <Style src="..."/> attaches a style sheet (to the document's
// elements at the root, to its parent element inside one); <Template name="..." src="..."/> names another document,
// which <Instance template="..."> builds in place, with <AttributeOverrides element-name="..." attribute="value"/>
// changing attributes of its named elements; the children of an <Instance> go to the template's element marked
// content-container. Other elements are element types from a registry by name; every element takes name, class
// (names separated by spaces), style (an inline style), focusable and enabled (true or false), tabindex (a number) and
// picking-mode (Position or Ignore), and its type reads the rest. A type of a script class makes its object over the
// element (with the markup's other attributes) before the element's children are built, and its content container
// takes them.
namespace helix::ui
{
    struct HxmlError
    {
        u32 line = 0, column = 0;
        String message;
    };

    class Painter;
    class EventSystem;

    struct HxmlAttribute
    {
        Atom name = kNoAtom;
        String value;
    };

    // Reads one attribute of its type; false when the type does not know it.
    using AttributeHandler = bool (*)(ElementTree& tree, ElementHandle element, StringView name, StringView value);
    // Paints an element's own content (vector paths) in its content box (pixels from the box's top left).
    using PaintHandler = void (*)(Painter& painter, const ElementTree& tree, ElementHandle element, f32 width, f32 height);
    // Gives an element its type's behavior in a panel (its callbacks on the panel's events), once it is built.
    using AttachHandler = void (*)(EventSystem& events, ElementHandle element);
    // Makes an element of a type of a script class: its class's object (which makes its element), given the markup's
    // attributes other than the common ones; the names its class does not know go to unknown. An invalid handle when
    // it cannot (a plain element of the type is made instead).
    using CreateHandler = ElementHandle (*)(ElementTree& tree, Atom type, Span<const HxmlAttribute> attributes, Array<Atom>& unknown);
    // Where the markup's children of an element of the type go (its object's content container).
    using ContentHandler = ElementHandle (*)(ElementTree& tree, ElementHandle element);

    struct ElementType
    {
        Atom name = kNoAtom;
        AttributeHandler attributes = nullptr;
        PaintHandler paint = nullptr;
        AttachHandler attach = nullptr;
        CreateHandler create = nullptr;
        ContentHandler content = nullptr;
    };

    // Registers (or replaces) an element type; Element, TemplateContainer and Label are there from the start.
    void RegisterElementType(StringView name, AttributeHandler attributes = nullptr, PaintHandler paint = nullptr,
                             AttachHandler attach = nullptr);
    void RegisterElementType(const ElementType& type);
    void UnregisterElementType(Atom name);
    const ElementType* FindElementType(Atom name);

    // Registers (or replaces) an attribute every element takes, read by the layer that keeps it (such as tooltip);
    // the handler returns false for a value it cannot read.
    void RegisterCommonAttribute(StringView name, AttributeHandler apply);

    // What the markup says of an element's data binding, for the scripts to take: a binding of <Bindings> (its type,
    // such as DataBinding, and its attributes), or the element's data-source-path and data-source-type (the type
    // "#data-source").
    struct HxmlBinding
    {
        ElementHandle element;
        Atom type = kNoAtom;
        Array<HxmlAttribute> attributes = Array<HxmlAttribute>();
    };
    Array<HxmlBinding>& PendingMarkupBindings();

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
        virtual std::shared_ptr<const StyleSheetData> Style(StringView src) = 0;
        // An <Instance> of the template at src built its elements under container.
        virtual void Instanced(ElementHandle container, StringView name, StringView src)
        {
            (void)container, (void)name, (void)src;
        }
    };

    // Builds a document's elements under parent. What cannot be built (unknown types and attributes, missing
    // templates and sheets) is reported and left out; the rest is built.
    void Instantiate(const HxmlDocument& document, ElementTree& tree, StyleEngine& styles, ElementHandle parent, HxmlResolver* resolver,
                     Array<HxmlError>& errors);
}
