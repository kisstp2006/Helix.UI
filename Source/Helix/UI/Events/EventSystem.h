#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Math/Geometry.h>
#include <Helix/UI/Events/UIEvent.h>

// The events of a panel: the host's input (pointers, keys, typed and composed text) becomes the reference's events,
// dispatched along their path (trickle down from the root to the target, bubble up back) to the callbacks of the
// elements; events sent while one is dispatched wait for it. Pointers pick the top-most element under them; their
// moves send Over, Out, Enter and Leave after them; a press and a release on one element is a Click; the mouse gets
// its Mouse events after its Pointer events. Keys go to the focused element (or the root); Tab, the arrows, Enter and
// Escape send navigation events after them, whose default (unless stopped) moves the focus. A press focuses the
// focusable element under the pointer or its focusable ancestor, else nothing. The panel's elements keep the pseudo
// states :hover (under the mouse, with their ancestors) and :focus. Elements that are disabled (or in a disabled
// ancestor) get no callbacks.
namespace helix::ui
{
    class LayoutEngine;
    class StyleEngine;
    class EventSystem;

    using EventCallback = void (*)(void* user, EventSystem& events, UIEvent& e);

    enum class TrickleDown : u8
    {
        NoTrickleDown,
        TrickleDown
    };

    class EventSystem
    {
    public:
        EventSystem(ElementTree& tree, LayoutEngine& layout, StyleEngine& styles);
        // The root keys go to when nothing has the focus (its parent's events stop at it).
        void SetRoot(ElementHandle root) { root_ = root; }
        ElementTree& tree() { return tree_; }
        const LayoutEngine& layout() const { return layout_; }

        // A callback of an element for a type, in the bubble up phase (the target's own run there) or the trickle down
        // phase; the same callback, user and phase again is not added twice.
        void RegisterCallback(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase = TrickleDown::NoTrickleDown);
        void UnregisterCallback(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase = TrickleDown::NoTrickleDown);
        // Sends an event to its target (after the one being dispatched, if any).
        void SendEvent(const UIEvent& e);

        // The host's input, in the panel's pixels and seconds. `covered`: a panel above has the point (no element of
        // this one is under it).
        void PointerDown(i32 pointerId, PointerKind kind, i32 button, Vector2 position, u8 modifiers, f64 time, bool covered = false);
        void PointerMove(i32 pointerId, PointerKind kind, Vector2 position, u8 modifiers, f64 time, bool covered = false);
        void PointerUp(i32 pointerId, PointerKind kind, i32 button, Vector2 position, u8 modifiers, f64 time, bool covered = false);
        void PointerCancel(i32 pointerId, f64 time);
        // The wheel's turn in ticks (y up, as the platform's).
        void Wheel(Vector2 position, Vector2 ticks, u8 modifiers, f64 time, bool covered = false);
        void KeyDown(u16 keyCode, u8 modifiers, f64 time);
        void KeyUp(u16 keyCode, u8 modifiers, f64 time);
        void Character(u32 codepoint, u8 modifiers, f64 time);
        // The text an input method composes (empty when nothing is) and its cursor (codepoints); a change sends a
        // CompositionEvent to the focused element (or the root).
        void SetComposition(Span<const u32> text, i32 cursor, f64 time);
        Span<const u32> Composition() const { return Span<const u32>(composition_.Data(), composition_.Size()); }
        i32 CompositionCursor() const { return compositionCursor_; }

        // The top-most element under a point (none when nothing pickable is), and all of them top-most first.
        ElementHandle Pick(Vector2 point) const;
        void PickAll(Vector2 point, Array<ElementHandle>& out) const;
        ElementHandle ElementUnderPointer(i32 pointerId) const;
        Vector2 PointerPosition(i32 pointerId) const;

        // Pointer capture: the element gets the pointer's events (but Over, Out, Enter and Leave) until released.
        void CapturePointer(ElementHandle e, i32 pointerId);
        void ReleasePointer(ElementHandle e, i32 pointerId);
        ElementHandle CapturingElement(i32 pointerId) const;
        bool HasPointerCapture(ElementHandle e, i32 pointerId) const { return CapturingElement(pointerId) == e && e.IsValid(); }

        // Focus.
        ElementHandle Focused() const;
        // Whether the element could take the focus: focusable, enabled, displayed and visible.
        bool CanGrabFocus(ElementHandle e) const;
        // Gives it the focus (its first focusable descendant's when it delegates); false when it cannot take it.
        bool Focus(ElementHandle e);
        // Takes the focus from it (when it has it).
        void Blur(ElementHandle e);
        // The focus ring: positive tab indices first in order, then 0 in tree order.
        void FocusRing(Array<ElementHandle>& out) const;
        // The focus to the next or previous element of the ring, or to the nearest one in a direction.
        void MoveFocus(NavigationDirection direction);

        // Text input: an element taking text asks for the input method while it has the focus, with its caret in its
        // box (asked again as the caret moves). TextInput gives the caret in the panel while the element has the focus.
        void SetTextInput(ElementHandle e, const Rect& caret);
        bool TextInput(Rect& caret) const;

        // Drag and drop: a drag of the pointer starts carrying data of a kind; while it moves, elements entered and
        // left get DragEnter and DragLeave and the one under it DragUpdated, whose callbacks set the visual mode; on
        // release DragPerform goes to that element when it accepts (a mode other than None and Rejected), then
        // DragExited, and the drag ends.
        void StartDrag(i32 pointerId, Atom kind, void* data);
        bool Dragging() const { return drag_.active; }
        Atom DragKind() const { return drag_.kind; }
        void* DragData() const { return drag_.data; }
        DragVisualMode VisualMode() const { return drag_.mode; }
        void SetVisualMode(DragVisualMode mode) { drag_.mode = mode; }

        // Presses closer in time and place than these count as one click of more.
        f64 clickInterval = 0.5;
        f32 clickDistance = 4;
        // The wheel's lines per tick.
        f32 scrollDeltaPerTick = 3;

        // Drops what refers to elements that went (after the tree changed).
        void Forget();

    private:
        struct Callback
        {
            u32 generation = 0;
            EventTypeId type = kNoEventType;
            bool trickleDown = false;
            EventCallback function = nullptr;
            void* user = nullptr;
        };

        using DefaultAction = void (*)(EventSystem& events, const UIEvent& e);

        struct Queued
        {
            UIEvent event;
            DefaultAction after = nullptr;
        };

        struct Pointer
        {
            bool known = false, primary = true;
            PointerKind kind = PointerKind::Mouse;
            Vector2 position;
            u32 pressed = 0;
            ElementHandle under, capture;
            ElementHandle pressedOn[5];
            f64 lastPressTime = -1e9;
            Vector2 lastPressPosition;
            i32 lastButton = -1, clickCount = 0;
        };

        struct Drag
        {
            bool active = false;
            i32 pointerId = PointerId::invalid;
            Atom kind = kNoAtom;
            void* data = nullptr;
            DragVisualMode mode = DragVisualMode::None;
            ElementHandle over;
        };

        void Send(const UIEvent& e, DefaultAction after);
        void Dispatch(UIEvent& e);
        bool Invoke(ElementHandle element, UIEvent& e, bool trickleDown);
        UIEvent PointerEvent(EventTypeId type, i32 pointerId, ElementHandle target, i32 button, u8 modifiers, f64 time, Vector2 delta) const;
        // The pointer's event to a target (its capture or the element under it), with the mouse's after it.
        void SendPointer(EventTypeId pointerType, EventTypeId mouseType, i32 pointerId, ElementHandle target, i32 button, u8 modifiers, f64 time,
                         Vector2 delta);
        // The element under a pointer changed: Out, Leave, Over and Enter (the mouse's after), :hover, the drag's.
        void SetUnder(i32 pointerId, ElementHandle under, u8 modifiers, f64 time);
        void SetHover(ElementHandle under);
        void SwitchFocus(ElementHandle to, f64 time);
        void UpdateDrag(u8 modifiers, f64 time);
        void EndDrag(i32 pointerId, bool dropped, u8 modifiers, f64 time);
        ElementHandle PickIn(ElementHandle e, Vector2 point, const Rect& clip, Array<ElementHandle>* all) const;
        bool Displayed(ElementHandle e) const;
        Pointer& At(i32 pointerId);

        static void CommitFocus(EventSystem& events, const UIEvent& e);
        static void Navigate(EventSystem& events, const UIEvent& e);
        static void FinishDrag(EventSystem& events, const UIEvent& e);

        ElementTree& tree_;
        LayoutEngine& layout_;
        StyleEngine& styles_;
        ElementHandle root_;
        Array<Array<Callback>> callbacks_;
        Array<Queued> queue_;
        bool dispatching_ = false;
        Pointer pointers_[PointerId::max];
        Array<ElementHandle> hovered_;
        ElementHandle focused_;
        // The change the queued focus events lead to.
        ElementHandle focusTo_, focusFrom_;
        f64 focusTime_ = 0;
        Array<u32> composition_;
        i32 compositionCursor_ = 0;
        ElementHandle textElement_;
        Rect caret_;
        Drag drag_;
        f64 lastTime_ = 0;
    };
}
