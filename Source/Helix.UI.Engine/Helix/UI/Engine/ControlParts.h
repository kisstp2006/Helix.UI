#pragma once

#include <Helix/UI/Engine/Control.h>

// What the controls share in building their parts and listening to their events (not for scripts).
namespace helix::ui
{
    ElementTree& ControlTree();
    // A part of a control: an element of a type (no object) with classes (separated by spaces), under parent.
    ElementHandle MakePart(ElementHandle parent, StringView classes, StringView type = "Element");
    void AddClasses(ElementHandle e, StringView classes);
    void SetPartStyle(ElementHandle e, StringView property, StringView value);
    void SetPartClass(ElementHandle e, StringView name, bool on);
    void SetPartState(ElementHandle e, PseudoStates state, bool on);
    void ShowPart(ElementHandle e, bool shown);
    // The point (the panel's) is over the element or one of its descendants.
    bool PointerInside(EventSystem& events, ElementHandle e, Vector2 position);
    // A callback of the control (its object the user) on one of its elements.
    void Listen(Element& self, ElementHandle on, EventTypeId type, EventCallback callback, TrickleDown phase = TrickleDown::NoTrickleDown);
    // The element's world rectangle in its panel (empty outside of one).
    Rect PartRect(const Element& owner, ElementHandle e);
}
