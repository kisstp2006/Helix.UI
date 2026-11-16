#include <Helix/UI/Events/UIEvent.h>

#include <Helix/Core/Containers/Array.h>

namespace helix::ui
{
    namespace
    {
        Array<EventTypeInfo>& Registry()
        {
            static Array<EventTypeInfo>* types = new Array<EventTypeInfo>();
            return *types;
        }

        struct Builtin
        {
            EventTypeId EventTypes::* field;
            StringView name;
            bool tricklesDown, bubbles;
            EventFamily family;
        };

        // The built-in types: their names and propagation as the reference's.
        const Builtin kBuiltins[] = {
            {&EventTypes::pointerDown, "PointerDownEvent", true, true, EventFamily::Pointer},
            {&EventTypes::pointerMove, "PointerMoveEvent", true, true, EventFamily::Pointer},
            {&EventTypes::pointerUp, "PointerUpEvent", true, true, EventFamily::Pointer},
            {&EventTypes::pointerCancel, "PointerCancelEvent", true, true, EventFamily::Pointer},
            {&EventTypes::pointerEnter, "PointerEnterEvent", true, false, EventFamily::Pointer},
            {&EventTypes::pointerLeave, "PointerLeaveEvent", true, false, EventFamily::Pointer},
            {&EventTypes::pointerOver, "PointerOverEvent", true, true, EventFamily::Pointer},
            {&EventTypes::pointerOut, "PointerOutEvent", true, true, EventFamily::Pointer},
            {&EventTypes::click, "ClickEvent", true, true, EventFamily::Pointer},
            {&EventTypes::mouseDown, "MouseDownEvent", true, true, EventFamily::Mouse},
            {&EventTypes::mouseMove, "MouseMoveEvent", true, true, EventFamily::Mouse},
            {&EventTypes::mouseUp, "MouseUpEvent", true, true, EventFamily::Mouse},
            {&EventTypes::mouseEnter, "MouseEnterEvent", true, false, EventFamily::Mouse},
            {&EventTypes::mouseLeave, "MouseLeaveEvent", true, false, EventFamily::Mouse},
            {&EventTypes::mouseOver, "MouseOverEvent", true, true, EventFamily::Mouse},
            {&EventTypes::mouseOut, "MouseOutEvent", true, true, EventFamily::Mouse},
            {&EventTypes::wheel, "WheelEvent", true, true, EventFamily::Mouse},
            {&EventTypes::pointerCapture, "PointerCaptureEvent", true, true, EventFamily::Capture},
            {&EventTypes::pointerCaptureOut, "PointerCaptureOutEvent", true, true, EventFamily::Capture},
            {&EventTypes::mouseCapture, "MouseCaptureEvent", true, true, EventFamily::Capture},
            {&EventTypes::mouseCaptureOut, "MouseCaptureOutEvent", true, true, EventFamily::Capture},
            {&EventTypes::keyDown, "KeyDownEvent", true, true, EventFamily::Keyboard},
            {&EventTypes::keyUp, "KeyUpEvent", true, true, EventFamily::Keyboard},
            {&EventTypes::focusOut, "FocusOutEvent", true, true, EventFamily::Focus},
            {&EventTypes::focusIn, "FocusInEvent", true, true, EventFamily::Focus},
            {&EventTypes::blur, "BlurEvent", true, false, EventFamily::Focus},
            {&EventTypes::focus, "FocusEvent", true, false, EventFamily::Focus},
            {&EventTypes::navigationMove, "NavigationMoveEvent", true, true, EventFamily::Navigation},
            {&EventTypes::navigationSubmit, "NavigationSubmitEvent", true, true, EventFamily::Navigation},
            {&EventTypes::navigationCancel, "NavigationCancelEvent", true, true, EventFamily::Navigation},
            {&EventTypes::dragEnter, "DragEnterEvent", true, false, EventFamily::Drag},
            {&EventTypes::dragLeave, "DragLeaveEvent", true, false, EventFamily::Drag},
            {&EventTypes::dragUpdated, "DragUpdatedEvent", true, true, EventFamily::Drag},
            {&EventTypes::dragPerform, "DragPerformEvent", true, true, EventFamily::Drag},
            {&EventTypes::dragExited, "DragExitedEvent", true, true, EventFamily::Drag},
            {&EventTypes::composition, "CompositionEvent", true, true, EventFamily::Keyboard},
            {&EventTypes::geometryChanged, "GeometryChangedEvent", false, false, EventFamily::Other},
            {&EventTypes::attachToPanel, "AttachToPanelEvent", false, false, EventFamily::Other},
            {&EventTypes::detachFromPanel, "DetachFromPanelEvent", false, false, EventFamily::Other},
            {&EventTypes::customStyleResolved, "CustomStyleResolvedEvent", false, false, EventFamily::Other},
        };
    }

    EventTypeId RegisterEventType(StringView name, bool tricklesDown, bool bubbles, EventFamily family)
    {
        Atom a = Intern(name);
        Array<EventTypeInfo>& r = Registry();
        for (usize i = 0; i < r.Size(); i++)
            if (r[i].name == a)
                return EventTypeId(i);
        r.Push(EventTypeInfo{a, tricklesDown, bubbles, family});
        return EventTypeId(r.Size() - 1);
    }

    const EventTypeInfo& EventTypeOf(EventTypeId type)
    {
        static const EventTypeInfo none;
        const Array<EventTypeInfo>& r = Registry();
        return type < r.Size() ? r[type] : none;
    }

    EventTypeId FindEventType(StringView name)
    {
        Atom a = Intern(name);
        const Array<EventTypeInfo>& r = Registry();
        for (usize i = 0; i < r.Size(); i++)
            if (r[i].name == a)
                return EventTypeId(i);
        return kNoEventType;
    }

    const EventTypes& Events()
    {
        static const EventTypes types = [] {
            EventTypes t{};
            for (const Builtin& b : kBuiltins)
                t.*b.field = RegisterEventType(b.name, b.tricklesDown, b.bubbles, b.family);
            return t;
        }();
        return types;
    }
}
