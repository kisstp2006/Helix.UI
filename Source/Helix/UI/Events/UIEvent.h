#pragma once

#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>
#include <Helix/UI/Elements/ElementTree.h>

// The events of a panel: types from a registry (a name, whether the type trickles down from the root to its target and
// bubbles up back, its family), one event struct for every type with the fields each family uses. Positions are in the
// panel's pixels from the top left; key codes are the script API's KeyCode values; modifiers are Shift 1, Control 2,
// Alt 4 and Command 8.
namespace helix::ui
{
    using EventTypeId = u16;
    inline constexpr EventTypeId kNoEventType = 0xFFFF;

    enum class EventFamily : u8
    {
        Pointer,
        Mouse,
        Keyboard,
        Focus,
        Navigation,
        Capture,
        Drag,
        Other
    };

    struct EventTypeInfo
    {
        Atom name = kNoAtom;
        bool tricklesDown = true;
        bool bubbles = true;
        EventFamily family = EventFamily::Other;
    };

    // Registers an event type (the same name again gives the same id).
    EventTypeId RegisterEventType(StringView name, bool tricklesDown, bool bubbles, EventFamily family);
    const EventTypeInfo& EventTypeOf(EventTypeId type);
    EventTypeId FindEventType(StringView name);

    // The built-in types, registered on first use with the reference's names and propagation.
    struct EventTypes
    {
        EventTypeId pointerDown, pointerMove, pointerUp, pointerCancel, pointerEnter, pointerLeave, pointerOver, pointerOut, click;
        EventTypeId mouseDown, mouseMove, mouseUp, mouseEnter, mouseLeave, mouseOver, mouseOut, wheel;
        EventTypeId pointerCapture, pointerCaptureOut, mouseCapture, mouseCaptureOut;
        EventTypeId keyDown, keyUp;
        EventTypeId focusOut, focusIn, blur, focus;
        EventTypeId navigationMove, navigationSubmit, navigationCancel;
        EventTypeId dragEnter, dragLeave, dragUpdated, dragPerform, dragExited;
        // Helix's: the text an input method composes changed (EventSystem::Composition), to the focused element (the
        // reference's text fields read it from the input each frame).
        EventTypeId composition;
        // The panel's own, to their target only: its box changed (oldRect, newRect), it came into a panel or left one,
        // its custom style properties were resolved.
        EventTypeId geometryChanged, attachToPanel, detachFromPanel, customStyleResolved;
    };
    const EventTypes& Events();

    enum class PropagationPhase : u8
    {
        None,
        TrickleDown,
        BubbleUp
    };

    enum class PointerKind : u8
    {
        Mouse,
        Touch,
        Pen
    };

    // Pointer ids: the mouse, then the touches, then the pens.
    struct PointerId
    {
        static constexpr i32 invalid = -1;
        static constexpr i32 mouse = 0;
        static constexpr i32 touchBase = 1, touchCount = 20;
        static constexpr i32 penBase = 21, penCount = 2;
        static constexpr i32 max = 32;
    };

    enum class NavigationDirection : u8
    {
        None,
        Left,
        Up,
        Right,
        Down,
        Next,
        Previous
    };

    // What a drop would do, set by the drop target while a drag moves over it.
    enum class DragVisualMode : u8
    {
        None,
        Copy,
        Link,
        Move,
        Generic,
        Rejected
    };

    struct UIEvent
    {
        EventTypeId type = kNoEventType;
        // Where it happens, the element whose callbacks run now, and the other element of a change (focus, enter,
        // leave, over, out).
        ElementHandle target, currentTarget, relatedTarget;
        PropagationPhase phase = PropagationPhase::None;
        // Seconds (the host's clock).
        f64 timestamp = 0;
        u8 modifiers = 0;

        // Pointers: the pointer, its kind, whether it is the primary one, the button that changed (-1 for none) and
        // the pressed buttons (1 left, 2 right, 4 middle), the presses in quick succession, the position, the
        // position in the current target's box, the move since the last event.
        i32 pointerId = PointerId::invalid;
        PointerKind pointerKind = PointerKind::Mouse;
        bool isPrimary = true;
        i32 button = -1;
        u32 pressedButtons = 0;
        i32 clickCount = 0;
        Vector2 position, localPosition, deltaPosition;
        // The wheel's scroll (lines; y down).
        Vector2 delta;

        // Keys: the key's code (0 for a typed character) and the character typed (0 for none).
        u16 keyCode = 0;
        u32 character = 0;

        // Navigation.
        NavigationDirection direction = NavigationDirection::None;
        Vector2 move;

        bool propagationStopped = false, immediatePropagationStopped = false;
        // A box's change (GeometryChangedEvent).
        Rect oldRect, newRect;

        // No elements after this one get it (the current one's other callbacks still do).
        void StopPropagation() { propagationStopped = true; }
        // Not even the current element's other callbacks get it.
        void StopImmediatePropagation() { propagationStopped = immediatePropagationStopped = true; }
        bool Is(EventTypeId t) const { return type == t; }
    };
}
