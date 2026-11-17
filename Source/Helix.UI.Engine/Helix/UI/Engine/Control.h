#pragma once

#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Markup/Hxml.h>

#include <type_traits>

// The controls' registry: each control class is a markup type of its name whose elements are made by its object (the
// object builds its parts and registers its behavior on the world's callbacks, so it works in any panel), with a table
// of the attributes its markup takes (the base classes' after its own). The C# side hears of what a control did
// (clicked, a value changed) through one notification by the element's handle.
namespace helix::ui
{
    // A markup attribute of a control: its name and how its text sets the object (false: not a value it takes).
    struct ControlAttribute
    {
        const char* name;
        bool (*set)(Element& self, StringView value);
    };

    // What a control tells its C# object.
    enum class ControlNotify : i32
    {
        Clicked = 1,
        ValueChanged = 2,
        Selected = 3,
    };
    void NotifyControl(ElementHandle e, ControlNotify what);

    // The values of markup attributes.
    bool ParseBoolText(StringView text, bool& out);
    bool ParseIntText(StringView text, i64& out);
    bool ParseUIntText(StringView text, u64& out);
    bool ParseFloatText(StringView text, f64& out);
    // An enum's value by its name (or its number) among names in their values' order.
    bool ParseEnumText(StringView text, Span<const StringView> names, i32& out);

    // The controls' timed behavior (repeat buttons, scroll inertia), once a frame (the views call it).
    void TickControls(f64 time);
    // A control's per-frame work while it needs it (removed by returning false).
    using ControlTick = bool (*)(Element& self, f64 time);
    void StartControlTick(Element& self, ControlTick tick);

    namespace detail
    {
        template <class T>
        bool ControlAttributes(ElementTree& tree, ElementHandle e, StringView name, StringView value)
        {
            ElementWorld& world = ElementWorld::Main();
            if (&tree != &world.tree())
                return false;
            Element* o = Element::Of(world, e);
            if (!o)
                return false;
            for (const ControlAttribute& a : T::MarkupAttributes())
                if (name == StringView(a.name))
                    return a.set(*o, value);
            if constexpr (!std::is_same_v<typename T::ControlBase, Element>)
                return ControlAttributes<typename T::ControlBase>(tree, e, name, value);
            else
                return false;
        }

        template <class T>
        ElementHandle CreateControl(ElementTree& tree, Atom, Span<const HxmlAttribute> attributes, Array<Atom>& unknown)
        {
            if (&tree != &ElementWorld::Main().tree())
                return {};
            T* o = new T();
            for (const HxmlAttribute& a : attributes)
                if (!ControlAttributes<T>(tree, o->handle(), AtomName(a.name), a.value.View()))
                    unknown.Push(a.name);
            return o->handle();
        }

        ElementHandle ControlContent(ElementTree& tree, ElementHandle e);
    }

    // Registers a control class as the markup type of its name.
    template <class T>
    void RegisterControl(StringView name, PaintHandler paint = nullptr)
    {
        ElementType type;
        type.name = Intern(name);
        type.attributes = &detail::ControlAttributes<T>;
        type.paint = paint;
        type.create = &detail::CreateControl<T>;
        type.content = &detail::ControlContent;
        RegisterElementType(type);
        Element::RegisterObjectClass(name, [](ElementWorld& world, ElementHandle e) -> Element* { return new T(world, e); });
    }
}
