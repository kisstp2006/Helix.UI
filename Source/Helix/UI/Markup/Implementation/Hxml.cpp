#include <Helix/UI/Markup/Hxml.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Core/Strings/Format.h>

#include <cstdlib>

namespace helix::ui
{
    namespace
    {
        HashMap<Atom, ElementType>& Types()
        {
            static HashMap<Atom, ElementType>* types = [] {
                auto* t = new HashMap<Atom, ElementType>();
                for (StringView name : {StringView("Element"), StringView("TemplateContainer")})
                    t->Set(Intern(name), ElementType{Intern(name), nullptr, nullptr});
                // A label: its text from the text attribute or the element's content.
                t->Set(Intern("Label"), ElementType{Intern("Label"), [](ElementTree& tree, ElementHandle e, StringView name, StringView value) {
                           if (name != StringView("text"))
                               return false;
                           tree.SetText(e, value);
                           return true;
                       }, nullptr});
                return t;
            }();
            return *types;
        }

        bool ParseBool(StringView v, bool& out)
        {
            if (v == StringView("true") || v == StringView("True"))
                out = true;
            else if (v == StringView("false") || v == StringView("False"))
                out = false;
            else
                return false;
            return true;
        }

        // The attributes every element takes beside name, class and style.
        struct CommonAttribute
        {
            StringView name;
            bool (*apply)(ElementTree& tree, ElementHandle e, StringView value);
        };

        const CommonAttribute kCommonAttributes[] = {
            {"focusable", [](ElementTree& t, ElementHandle e, StringView v) {
                 bool b = false;
                 return ParseBool(v, b) ? (t.SetFocusable(e, b), true) : false;
             }},
            {"enabled", [](ElementTree& t, ElementHandle e, StringView v) {
                 bool b = false;
                 return ParseBool(v, b) ? (t.SetEnabled(e, b), true) : false;
             }},
            {"tabindex", [](ElementTree& t, ElementHandle e, StringView v) {
                 String text(v);
                 char* end = nullptr;
                 long n = std::strtol(text.CStr(), &end, 10);
                 if (text.Empty() || end != text.CStr() + text.Size())
                     return false;
                 t.SetTabIndex(e, i32(n));
                 return true;
             }},
            {"picking-mode", [](ElementTree& t, ElementHandle e, StringView v) {
                 if (v == StringView("Position") || v == StringView("Ignore"))
                 {
                     t.SetPicking(e, v == StringView("Ignore") ? PickingMode::Ignore : PickingMode::Position);
                     return true;
                 }
                 return false;
             }},
        };

        // The common attributes of the layers above (their names interned, so the views stay valid).
        struct RegisteredAttribute
        {
            Atom name = kNoAtom;
            AttributeHandler apply = nullptr;
        };

        Array<RegisteredAttribute>& RegisteredAttributes()
        {
            static Array<RegisteredAttribute>* list = new Array<RegisteredAttribute>();
            return *list;
        }

        const CommonAttribute* FindCommon(StringView name)
        {
            for (const CommonAttribute& a : kCommonAttributes)
                if (a.name == name)
                    return &a;
            return nullptr;
        }

        AttributeHandler FindRegistered(StringView name)
        {
            for (const RegisteredAttribute& a : RegisteredAttributes())
                if (AtomName(a.name) == name)
                    return a.apply;
            return nullptr;
        }

        // Text content without the white space around it (lines of markup indent it).
        StringView Trimmed(StringView s)
        {
            auto space = [](char c) { return c == 32 || c == 10 || c == 13 || c == 9; };
            while (!s.Empty() && space(s[0]))
                s = s.Substr(1);
            while (!s.Empty() && space(s[s.Size() - 1]))
                s = s.Substr(0, s.Size() - 1);
            return s;
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
            // While a template's document is built: where its element marked content-container goes.
            ElementHandle* contentTarget = nullptr;

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
                    // The template's element marked content-container takes the instance's children.
                    ElementHandle* outer = contentTarget;
                    ElementHandle target;
                    contentTarget = &target;
                    Document(*d, container, Span<const Override>(mine.Data(), mine.Size()));
                    contentTarget = outer;
                    if (tree.IsAlive(target))
                        tree.SetContentContainer(container, target);
                    resolver->Instanced(container, name->View(), src->View());
                    for (const HxmlNode& c : n.children)
                        if (c.type != FindAtom("AttributeOverrides"))
                            Node(c, tree.ContentContainer(container), templates, overrides);
                    return;
                }
                const ElementType* type = FindElementType(n.type);
                if (!type)
                {
                    Error(n, Format("unknown element type <{}>", AtomName(n.type)));
                    return;
                }
                // The element's name comes first: overrides are found by it.
                const String* name = n.Attribute("name");
                Atom nameAtom = name ? Intern(name->View()) : kNoAtom;
                Array<HxmlAttribute> all;
                for (const HxmlAttribute& a : n.attributes)
                {
                    StringView value = a.value.View();
                    for (const Override& o : overrides)
                        if (o.element == nameAtom && nameAtom != kNoAtom && o.attribute == a.name)
                            value = o.value.View();
                    all.Push({a.name, String(value)});
                }
                for (const Override& o : overrides)
                    if (o.element == nameAtom && nameAtom != kNoAtom && !n.Attribute(AtomName(o.attribute)))
                        all.Push({o.attribute, o.value});
                // A type of a script class: its object makes the element, with the attributes the markup does not
                // read itself.
                ElementHandle e;
                if (type->create)
                {
                    Array<HxmlAttribute> own;
                    for (const HxmlAttribute& a : all)
                        if (!IsCommon(AtomName(a.name)))
                            own.Push(a);
                    Array<Atom> unknown;
                    e = type->create(tree, type->name, Span<const HxmlAttribute>(own.Data(), own.Size()), unknown);
                    for (Atom u : unknown)
                        Error(n, Format("<{}> has no attribute '{}'", AtomName(type->name), AtomName(u)));
                }
                if (!tree.IsAlive(e))
                    e = tree.Create(type->name);
                tree.Append(parent, e);
                for (const HxmlAttribute& a : all)
                    if (!type->create || IsCommon(AtomName(a.name)))
                        Attribute(n, e, a.name, a.value.View());
                // Text content (a label's text) when no text attribute gave one.
                if (!n.text.Empty() && tree.Text(e).Empty() && type->attributes && !n.Attribute("text"))
                    type->attributes(tree, e, "text", Trimmed(n.text.View()));
                // The children into its content container (its object's, or the element).
                ElementHandle into = type->content ? type->content(tree, e) : tree.ContentContainer(e);
                if (!tree.IsAlive(into))
                    into = e;
                static const Atom bindings = Intern("Bindings");
                for (const HxmlNode& c : n.children)
                {
                    // The element's bindings: kept for the scripts (the binding classes are theirs).
                    if (c.type == bindings)
                    {
                        for (const HxmlNode& b : c.children)
                            {
                            HxmlBinding binding;
                            binding.element = e;
                            binding.type = b.type;
                            binding.attributes = b.attributes;
                            PendingMarkupBindings().Push(Move(binding));
                        }
                        continue;
                    }
                    Node(c, into, templates, overrides);
                }
                // Its data source path and type, for the scripts' data binding.
                HxmlBinding source;
                source.element = e;
                source.type = Intern("#data-source");
                for (const HxmlAttribute& a : all)
                    if (AtomName(a.name) == StringView("data-source-path") || AtomName(a.name) == StringView("data-source-type") ||
                        AtomName(a.name) == StringView("data-source"))
                        source.attributes.Push(a);
                if (!source.attributes.Empty())
                    PendingMarkupBindings().Push(Move(source));
            }

            // The attributes the markup reads itself (the rest are the element type's).
            static bool IsCommon(StringView name)
            {
                if (name == StringView("data-source-path") || name == StringView("data-source-type") || name == StringView("data-source"))
                    return true;
                return name == StringView("name") || name == StringView("class") || name == StringView("style") ||
                       name == StringView("content-container") || FindCommon(name) != nullptr || FindRegistered(name) != nullptr;
            }

            void Attribute(const HxmlNode& n, ElementHandle e, Atom attribute, StringView value)
            {
                StringView name = AtomName(attribute);
                if (name == StringView("data-source-path") || name == StringView("data-source-type") || name == StringView("data-source"))
                    return;
                if (name == StringView("content-container"))
                {
                    // Marks the template's element that takes an instance's children (outside of a template: none).
                    if (contentTarget)
                        *contentTarget = e;
                }
                else if (name == StringView("name"))
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
                else if (const CommonAttribute* common = FindCommon(name))
                {
                    if (!common->apply(tree, e, value))
                        Error(n, Format("{}: '{}' is not a value of it", name, value));
                }
                else if (AttributeHandler registered = FindRegistered(name))
                {
                    if (!registered(tree, e, name, value))
                        Error(n, Format("{}: '{}' is not a value of it", name, value));
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

    Array<HxmlBinding>& PendingMarkupBindings()
    {
        static Array<HxmlBinding>* pending = new Array<HxmlBinding>();
        return *pending;
    }

    void RegisterElementType(StringView name, AttributeHandler attributes, PaintHandler paint, AttachHandler attach)
    {
        Atom a = Intern(name);
        ElementType t;
        t.name = a, t.attributes = attributes, t.paint = paint, t.attach = attach;
        Types().Set(a, t);
    }

    void RegisterElementType(const ElementType& type) { Types().Set(type.name, type); }

    void UnregisterElementType(Atom name) { Types().Remove(name); }

    const ElementType* FindElementType(Atom name) { return Types().Find(name); }

    void RegisterCommonAttribute(StringView name, AttributeHandler apply)
    {
        Atom atom = Intern(name);
        for (RegisteredAttribute& a : RegisteredAttributes())
            if (a.name == atom)
            {
                a.apply = apply;
                return;
            }
        RegisteredAttributes().Push({atom, apply});
    }

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
