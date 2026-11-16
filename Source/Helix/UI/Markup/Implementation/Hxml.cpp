#include <Helix/UI/Markup/Hxml.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Core/Strings/Format.h>

namespace helix::ui
{
    namespace
    {
        HashMap<Atom, ElementType>& Types()
        {
            static HashMap<Atom, ElementType>* types = [] {
                auto* t = new HashMap<Atom, ElementType>();
                for (StringView name : {StringView("Element"), StringView("TemplateContainer")})
                    t->Set(Intern(name), ElementType{Intern(name), nullptr});
                return t;
            }();
            return *types;
        }

        void Convert(const XmlNode& x, HxmlNode& out)
        {
            out.type = Intern(x.LocalName());
            out.line = x.line, out.column = x.column;
            out.text = x.text;
            for (const XmlAttribute& a : x.attributes)
            {
                // xmlns and other prefixed attributes belong to the XML, not to the elements.
                StringView n = a.name.View();
                if (n == StringView("xmlns") || n.StartsWith("xmlns:"))
                    continue;
                out.attributes.Push({Intern(n), a.value});
            }
            for (const auto& c : x.children)
                Convert(*c, out.children.Emplace());
        }

        // An attribute override of an instance: an element's name, the attribute, its value.
        struct Override
        {
            Atom element;
            Atom attribute;
            String value;
        };

        struct Builder
        {
            ElementTree& tree;
            StyleEngine& styles;
            HxmlResolver* resolver;
            Array<HxmlError>& errors;
            // The documents being built (a template that holds itself stops).
            Array<const HxmlDocument*> stack;

            void Error(const HxmlNode& at, StringView message)
            {
                HxmlError e;
                e.line = at.line, e.column = at.column;
                e.message = message;
                errors.Push(Move(e));
            }

            void Document(const HxmlDocument& d, ElementHandle parent, Span<const Override> overrides)
            {
                for (const HxmlDocument* open : stack)
                    if (open == &d)
                        return;
                stack.Push(&d);
                // The document's templates by name.
                HashMap<Atom, String> templates;
                for (const HxmlNode& n : d.Nodes())
                    if (n.type == FindAtom("Template"))
                    {
                        const String* name = n.Attribute("name");
                        const String* src = n.Attribute("src");
                        if (!name || !src)
                            Error(n, "<Template> needs name and src");
                        else
                            templates.Set(Intern(name->View()), *src);
                    }
                for (const HxmlNode& n : d.Nodes())
                    Node(n, parent, templates, overrides);
                stack.Pop();
            }

            void Node(const HxmlNode& n, ElementHandle parent, const HashMap<Atom, String>& templates, Span<const Override> overrides)
            {
                static const Atom style = Intern("Style"), templ = Intern("Template"), instance = Intern("Instance");
                if (n.type == templ)
                    return;
                if (n.type == style)
                {
                    const String* src = n.Attribute("src");
                    if (!src)
                    {
                        Error(n, "<Style> needs src");
                        return;
                    }
                    auto sheet = resolver ? resolver->Style(src->View()) : nullptr;
                    if (!sheet)
                        Error(n, Format("the style sheet '{}' was not found", src->View()));
                    else
                        styles.AddStyleSheet(parent, Move(sheet));
                    return;
                }
                if (n.type == instance)
                {
                    const String* name = n.Attribute("template");
                    const String* src = name ? templates.Find(Intern(name->View())) : nullptr;
                    const HxmlDocument* d = src && resolver ? resolver->Template(src->View()) : nullptr;
                    if (!d)
                    {
                        Error(n, Format("the template '{}' was not found", name ? name->View() : StringView()));
                        return;
                    }
                    ElementHandle container = tree.Create(Intern("TemplateContainer"));
                    tree.Append(parent, container);
                    Array<Override> mine;
                    for (const Override& o : overrides)
                        mine.Push(o);
                    for (const HxmlNode& c : n.children)
                    {
                        if (c.type != FindAtom("AttributeOverrides"))
                            continue;
                        const String* target = c.Attribute("element-name");
                        if (!target)
                        {
                            Error(c, "<AttributeOverrides> needs element-name");
                            continue;
                        }
                        for (const HxmlAttribute& a : c.attributes)
                            if (AtomName(a.name) != StringView("element-name"))
                                mine.Push({Intern(target->View()), a.name, a.value});
                    }
                    for (const HxmlAttribute& a : n.attributes)
                        if (AtomName(a.name) != StringView("template"))
                            Attribute(n, container, a.name, a.value.View());
                    Document(*d, container, Span<const Override>(mine.Data(), mine.Size()));
                    return;
                }
                const ElementType* type = FindElementType(n.type);
                if (!type)
                {
                    Error(n, Format("unknown element type <{}>", AtomName(n.type)));
                    return;
                }
                ElementHandle e = tree.Create(type->name);
                tree.Append(parent, e);
                // The element's name comes first: overrides are found by it.
                const String* name = n.Attribute("name");
                Atom nameAtom = name ? Intern(name->View()) : kNoAtom;
                for (const HxmlAttribute& a : n.attributes)
                {
                    StringView value = a.value.View();
                    for (const Override& o : overrides)
                        if (o.element == nameAtom && nameAtom != kNoAtom && o.attribute == a.name)
                            value = o.value.View();
                    Attribute(n, e, a.name, value);
                }
                for (const Override& o : overrides)
                    if (o.element == nameAtom && nameAtom != kNoAtom && !n.Attribute(AtomName(o.attribute)))
                        Attribute(n, e, o.attribute, o.value.View());
                for (const HxmlNode& c : n.children)
                    Node(c, e, templates, overrides);
            }

            void Attribute(const HxmlNode& n, ElementHandle e, Atom attribute, StringView value)
            {
                StringView name = AtomName(attribute);
                if (name == StringView("name"))
                    tree.SetName(e, Intern(value));
                else if (name == StringView("class"))
                {
                    usize at = 0;
                    while (at < value.Size())
                    {
                        while (at < value.Size() && value[at] == ' ')
                            at++;
                        usize end = value.Find(' ', at);
                        if (end == StringView::npos)
                            end = value.Size();
                        if (end > at)
                            tree.AddClass(e, Intern(value.Substr(at, end - at)));
                        at = end;
                    }
                }
                else if (name == StringView("style"))
                {
                    Array<StyleError> found;
                    styles.SetInlineStyle(e, value, &found);
                    for (const StyleError& s : found)
                        Error(n, Format("style: {}", s.message.View()));
                }
                else
                {
                    const ElementType* type = FindElementType(tree.Type(e));
                    if (!type || !type->attributes || !type->attributes(tree, e, name, value))
                        Error(n, Format("<{}> has no attribute '{}'", AtomName(tree.Type(e)), name));
                }
            }
        };
    }

    void RegisterElementType(StringView name, AttributeHandler attributes)
    {
        Atom a = Intern(name);
        Types().Set(a, ElementType{a, attributes});
    }

    const ElementType* FindElementType(Atom name) { return Types().Find(name); }

    const String* HxmlNode::Attribute(StringView n) const
    {
        Atom a = FindAtom(n);
        if (a == kNoAtom)
            return nullptr;
        for (const HxmlAttribute& at : attributes)
            if (at.name == a)
                return &at.value;
        return nullptr;
    }

    bool HxmlDocument::Parse(StringView text, HxmlDocument& out, Array<HxmlError>& errors)
    {
        out.nodes_.Clear();
        XmlError xmlError;
        auto root = ParseXml(text, xmlError);
        if (!root)
        {
            errors.Push({xmlError.line, xmlError.column, xmlError.message});
            return false;
        }
        if (root->LocalName() != StringView("HXML"))
        {
            errors.Push({root->line, root->column, Format("the root must be <HXML>, not <{}>", root->name.View())});
            return false;
        }
        for (const auto& c : root->children)
            Convert(*c, out.nodes_.Emplace());
        return true;
    }

    void Instantiate(const HxmlDocument& document, ElementTree& tree, StyleEngine& styles, ElementHandle parent, HxmlResolver* resolver,
                     Array<HxmlError>& errors)
    {
        Builder b{tree, styles, resolver, errors, Array<const HxmlDocument*>()};
        b.Document(document, parent, {});
    }
}
