#include <Helix/UI/Events/EventSystem.h>

#include <Helix/UI/Layout/Layout.h>
#include <Helix/UI/Styles/StyleEngine.h>

#include <cmath>
#include <utility>

namespace helix::ui
{
    namespace
    {
        // The keys that navigate, and how.
        enum class NavigationKind : u8
        {
            Move,
            Submit,
            Cancel
        };

        struct NavigationKey
        {
            u16 keyCode;
            NavigationKind kind;
            NavigationDirection direction;
        };

        // Tab (Shift+Tab: back), the arrows, Return and the keypad's Enter, Escape (the script API's key codes).
        const NavigationKey kNavigationKeys[] = {
            {9, NavigationKind::Move, NavigationDirection::Next},       {273, NavigationKind::Move, NavigationDirection::Up},
            {274, NavigationKind::Move, NavigationDirection::Down},     {275, NavigationKind::Move, NavigationDirection::Right},
            {276, NavigationKind::Move, NavigationDirection::Left},     {13, NavigationKind::Submit, NavigationDirection::None},
            {271, NavigationKind::Submit, NavigationDirection::None},   {27, NavigationKind::Cancel, NavigationDirection::None},
        };

        constexpr u8 kShift = 1;

        bool Inside(const Rect& r, Vector2 p) { return p.x >= r.x && p.y >= r.y && p.x < r.x + r.width && p.y < r.y + r.height; }

        Rect Intersect(const Rect& a, const Rect& b)
        {
            f32 x0 = a.x > b.x ? a.x : b.x, y0 = a.y > b.y ? a.y : b.y;
            f32 x1 = a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width;
            f32 y1 = a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height;
            return Rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
        }

        const Rect kEverywhere(-1e9f, -1e9f, 2e9f, 2e9f);

        bool Contains(const Array<ElementHandle>& list, ElementHandle e)
        {
            for (ElementHandle x : list)
                if (x == e)
                    return true;
            return false;
        }
    }

    EventSystem::EventSystem(ElementTree& tree, LayoutEngine& layout, StyleEngine& styles, EventCallbacks* callbacks)
        : tree_(tree), layout_(layout), styles_(styles), callbacks_(callbacks ? *callbacks : ownCallbacks_)
    {
    }

    EventSystem::Pointer& EventSystem::At(i32 pointerId)
    {
        static Pointer none;
        if (pointerId < 0 || pointerId >= PointerId::max)
        {
            none = Pointer();
            return none;
        }
        return pointers_[pointerId];
    }

    // ------------------------------------------------------------------------------------------------- callbacks

    void EventSystem::RegisterCallback(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase)
    {
        if (tree_.IsAlive(e))
            callbacks_.Register(e, type, callback, user, phase);
    }

    void EventSystem::UnregisterCallback(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase)
    {
        callbacks_.Unregister(e, type, callback, user, phase);
    }

    // -------------------------------------------------------------------------------------------------- dispatch

    void EventSystem::SendEvent(const UIEvent& e) { Send(e, nullptr); }

    void EventSystem::Send(const UIEvent& e, DefaultAction after)
    {
        queue_.Push(Queued{e, after});
        if (dispatching_)
            return;
        dispatching_ = true;
        // Events sent while one is dispatched join the queue behind it.
        for (usize i = 0; i < queue_.Size(); i++)
        {
            Queued q = queue_[i];
            Dispatch(q.event);
            if (q.after)
                q.after(*this, q.event);
        }
        queue_.Clear();
        dispatching_ = false;
    }

    void EventSystem::Dispatch(UIEvent& e)
    {
        if (!tree_.IsAlive(e.target))
            return;
        const EventTypeInfo& info = EventTypeOf(e.type);
        // The path, fixed before any callback runs: the target, then its ancestors.
        Array<ElementHandle> path;
        for (ElementHandle p = e.target; p.IsValid(); p = tree_.Parent(p))
            path.Push(p);
        e.phase = PropagationPhase::TrickleDown;
        for (usize i = path.Size(); i-- > 0 && !e.propagationStopped;)
            if (i == 0 || info.tricklesDown)
                Invoke(path[i], e, true);
        e.phase = PropagationPhase::BubbleUp;
        for (usize i = 0; i < path.Size() && !e.propagationStopped; i++)
        {
            if (i > 0 && !info.bubbles)
                break;
            Invoke(path[i], e, false);
        }
        e.phase = PropagationPhase::None;
        e.currentTarget = {};
    }

    bool EventSystem::Invoke(ElementHandle element, UIEvent& e, bool trickleDown)
    {
        if (!tree_.IsAlive(element) || !callbacks_.Has(element, e.type, trickleDown) || !tree_.EnabledInHierarchy(element))
            return false;
        e.currentTarget = element;
        Rect r = layout_.WorldRect(element);
        e.localPosition = Vector2(e.position.x - r.x, e.position.y - r.y);
        // A copy: callbacks may add and remove callbacks.
        Span<const EventCallbacks::Callback> of = callbacks_.Of(element);
        Array<EventCallbacks::Callback> list;
        for (const EventCallbacks::Callback& c : of)
            list.Push(c);
        for (const EventCallbacks::Callback& c : list)
        {
            if (c.generation != element.generation || c.type != e.type || c.trickleDown != trickleDown)
                continue;
            c.function(c.user, *this, e);
            if (e.immediatePropagationStopped)
                break;
        }
        return true;
    }

    // --------------------------------------------------------------------------------------------------- picking

    bool EventSystem::Displayed(ElementHandle e) const
    {
        for (ElementHandle x = e; x.IsValid(); x = tree_.Parent(x))
            if (styles_.Computed(x).layout.display == Display::None)
                return false;
        return tree_.IsAlive(e);
    }

    ElementHandle EventSystem::PickIn(ElementHandle e, Vector2 point, const Rect& clip, Array<ElementHandle>* all) const
    {
        if (!tree_.IsAlive(e))
            return {};
        const ComputedStyle& s = styles_.Computed(e);
        if (s.layout.display == Display::None)
            return {};
        Rect r = layout_.WorldRect(e);
        Rect inner = clip;
        if (s.overflow == Overflow::Hidden)
        {
            const LayoutBox& b = layout_.Box(e);
            inner = Intersect(clip, Rect(r.x + b.border[kLeft], r.y + b.border[kTop], r.width - b.border[kLeft] - b.border[kRight],
                                         r.height - b.border[kTop] - b.border[kBottom]));
        }
        // The children drawn last are on top.
        if (Inside(inner, point))
            for (ElementHandle c = tree_.LastChild(e); c.IsValid(); c = tree_.PreviousSibling(c))
            {
                ElementHandle hit = PickIn(c, point, inner, all);
                if (hit.IsValid())
                    return hit;
            }
        if (tree_.Picking(e) == PickingMode::Position && s.visibility == Visibility::Visible && Inside(clip, point) && Inside(r, point))
        {
            if (!all)
                return e;
            all->Push(e);
        }
        return {};
    }

    ElementHandle EventSystem::Pick(Vector2 point) const { return PickIn(root_, point, kEverywhere, nullptr); }

    void EventSystem::PickAll(Vector2 point, Array<ElementHandle>& out) const
    {
        out.Clear();
        PickIn(root_, point, kEverywhere, &out);
    }

    ElementHandle EventSystem::ElementUnderPointer(i32 pointerId) const
    {
        if (pointerId < 0 || pointerId >= PointerId::max || !tree_.IsAlive(pointers_[pointerId].under))
            return {};
        return pointers_[pointerId].under;
    }

    Vector2 EventSystem::PointerPosition(i32 pointerId) const
    {
        return pointerId >= 0 && pointerId < PointerId::max ? pointers_[pointerId].position : Vector2();
    }

    // -------------------------------------------------------------------------------------------------- pointers

    UIEvent EventSystem::PointerEvent(EventTypeId type, i32 pointerId, ElementHandle target, i32 button, u8 modifiers, f64 time, Vector2 delta) const
    {
        UIEvent e;
        e.type = type;
        e.target = target;
        e.timestamp = time;
        e.modifiers = modifiers;
        e.pointerId = pointerId;
        if (pointerId >= 0 && pointerId < PointerId::max)
        {
            const Pointer& p = pointers_[pointerId];
            e.pointerKind = p.kind;
            e.isPrimary = p.primary;
            e.pressedButtons = p.pressed;
            e.clickCount = p.clickCount;
            e.position = p.position;
        }
        e.button = button;
        e.deltaPosition = delta;
        return e;
    }

    void EventSystem::SendPointer(EventTypeId pointerType, EventTypeId mouseType, i32 pointerId, ElementHandle target, i32 button, u8 modifiers,
                                  f64 time, Vector2 delta)
    {
        if (!tree_.IsAlive(target))
            return;
        Send(PointerEvent(pointerType, pointerId, target, button, modifiers, time, delta), nullptr);
        if (At(pointerId).kind == PointerKind::Mouse && mouseType != kNoEventType)
            Send(PointerEvent(mouseType, pointerId, target, button, modifiers, time, delta), nullptr);
    }

    void EventSystem::SetUnder(i32 pointerId, ElementHandle under, u8 modifiers, f64 time)
    {
        Pointer& p = At(pointerId);
        ElementHandle previous = tree_.IsAlive(p.under) ? p.under : ElementHandle{};
        if (!tree_.IsAlive(under))
            under = {};
        if (previous == under)
            return;
        p.under = under;
        // The elements left (from the old one up) and entered (down to the new one), below their common ancestor.
        Array<ElementHandle> chain, left, entered;
        for (ElementHandle x = under; x.IsValid(); x = tree_.Parent(x))
            chain.Push(x);
        ElementHandle common;
        for (ElementHandle x = previous; x.IsValid(); x = tree_.Parent(x))
        {
            if (Contains(chain, x))
            {
                common = x;
                break;
            }
            left.Push(x);
        }
        for (ElementHandle x : chain)
        {
            if (x == common)
                break;
            entered.Insert(0, x);
        }
        const EventTypes& t = Events();
        auto send = [&](EventTypeId type, ElementHandle target, ElementHandle related) {
            UIEvent e = PointerEvent(type, pointerId, target, -1, modifiers, time, Vector2());
            e.relatedTarget = related;
            Send(e, nullptr);
        };
        // The pointer's events, then the mouse's.
        struct Kinds
        {
            EventTypeId out, leave, over, enter;
        };
        const Kinds kinds[2] = {{t.pointerOut, t.pointerLeave, t.pointerOver, t.pointerEnter}, {t.mouseOut, t.mouseLeave, t.mouseOver, t.mouseEnter}};
        u32 passes = p.kind == PointerKind::Mouse ? 2 : 1;
        for (u32 k = 0; k < passes; k++)
        {
            if (previous.IsValid())
                send(kinds[k].out, previous, under);
            for (ElementHandle x : left)
                send(kinds[k].leave, x, under);
            if (under.IsValid())
                send(kinds[k].over, under, previous);
            for (ElementHandle x : entered)
                send(kinds[k].enter, x, previous);
        }
        if (pointerId == PointerId::mouse)
            SetHover(under);
    }

    void EventSystem::SetHover(ElementHandle under)
    {
        Array<ElementHandle> chain;
        for (ElementHandle x = under; x.IsValid(); x = tree_.Parent(x))
            chain.Push(x);
        for (ElementHandle x : hovered_)
            if (!Contains(chain, x))
                tree_.SetStates(x, tree_.States(x) & ~PseudoStates::Hover);
        for (ElementHandle x : chain)
            tree_.SetStates(x, tree_.States(x) | PseudoStates::Hover);
        hovered_ = std::move(chain);
    }

    void EventSystem::PointerMove(i32 pointerId, PointerKind kind, Vector2 position, u8 modifiers, f64 time, bool covered)
    {
        lastTime_ = time;
        Pointer& p = At(pointerId);
        Vector2 delta = p.known ? Vector2(position.x - p.position.x, position.y - p.position.y) : Vector2();
        p.known = true;
        p.kind = kind;
        p.position = position;
        ElementHandle under = covered ? ElementHandle{} : Pick(position);
        const EventTypes& t = Events();
        // The move first, then what it entered and left.
        SendPointer(t.pointerMove, t.mouseMove, pointerId, tree_.IsAlive(p.capture) ? p.capture : under, -1, modifiers, time, delta);
        SetUnder(pointerId, under, modifiers, time);
        if (drag_.active && drag_.pointerId == pointerId)
            UpdateDrag(modifiers, time);
    }

    void EventSystem::PointerDown(i32 pointerId, PointerKind kind, i32 button, Vector2 position, u8 modifiers, f64 time, bool covered)
    {
        lastTime_ = time;
        Pointer& p = At(pointerId);
        if (kind != PointerKind::Mouse && !p.known)
        {
            // The first touch while none is down is the primary one.
            bool other = false;
            for (i32 i = PointerId::touchBase; i < PointerId::max; i++)
                other = other || (i != pointerId && pointers_[i].known && pointers_[i].pressed);
            p.primary = !other;
        }
        else if (kind == PointerKind::Mouse)
            p.primary = true;
        p.known = true;
        p.kind = kind;
        p.position = position;
        ElementHandle under = covered ? ElementHandle{} : Pick(position);
        SetUnder(pointerId, under, modifiers, time);
        bool again = button == p.lastButton && time - p.lastPressTime <= clickInterval &&
                     std::sqrt((position.x - p.lastPressPosition.x) * (position.x - p.lastPressPosition.x) +
                               (position.y - p.lastPressPosition.y) * (position.y - p.lastPressPosition.y)) <= clickDistance;
        p.clickCount = again ? p.clickCount + 1 : 1;
        p.lastButton = button;
        p.lastPressTime = time;
        p.lastPressPosition = position;
        if (button >= 0 && button < 5)
            p.pressed |= 1u << button;
        ElementHandle target = tree_.IsAlive(p.capture) ? p.capture : under;
        if (button >= 0 && button < 5)
            p.pressedOn[button] = target;
        const EventTypes& t = Events();
        SendPointer(t.pointerDown, t.mouseDown, pointerId, target, button, modifiers, time, Vector2());
        // A press of the primary pointer moves the focus to what it pressed (after its events).
        if (p.primary)
        {
            ElementHandle f = under;
            while (f.IsValid() && !CanGrabFocus(f))
                f = tree_.Parent(f);
            if (f.IsValid())
            {
                if (Focused() != f && !tree_.IsAncestor(f, Focused()))
                    Focus(f);
            }
            else
                SwitchFocus({}, time);
        }
    }

    void EventSystem::PointerUp(i32 pointerId, PointerKind kind, i32 button, Vector2 position, u8 modifiers, f64 time, bool covered)
    {
        lastTime_ = time;
        Pointer& p = At(pointerId);
        p.known = true;
        p.kind = kind;
        p.position = position;
        ElementHandle under = covered ? ElementHandle{} : Pick(position);
        SetUnder(pointerId, under, modifiers, time);
        if (button >= 0 && button < 5)
            p.pressed &= ~(1u << button);
        ElementHandle target = tree_.IsAlive(p.capture) ? p.capture : under;
        const EventTypes& t = Events();
        SendPointer(t.pointerUp, t.mouseUp, pointerId, target, button, modifiers, time, Vector2());
        if (drag_.active && drag_.pointerId == pointerId)
            EndDrag(pointerId, true, modifiers, time);
        else if (button == 0 && target.IsValid() && p.pressedOn[0] == target)
            Send(PointerEvent(t.click, pointerId, target, button, modifiers, time, Vector2()), nullptr);
        if (button >= 0 && button < 5)
            p.pressedOn[button] = {};
        // A touch ends with its release: it leaves what it was over.
        if (kind != PointerKind::Mouse && !p.pressed)
        {
            SetUnder(pointerId, {}, modifiers, time);
            p.known = false;
        }
    }

    void EventSystem::PointerCancel(i32 pointerId, f64 time)
    {
        lastTime_ = time;
        Pointer& p = At(pointerId);
        if (!p.known)
            return;
        ElementHandle target = tree_.IsAlive(p.capture) ? p.capture : ElementUnderPointer(pointerId);
        SendPointer(Events().pointerCancel, kNoEventType, pointerId, target, -1, 0, time, Vector2());
        if (drag_.active && drag_.pointerId == pointerId)
            EndDrag(pointerId, false, 0, time);
        if (tree_.IsAlive(p.capture))
            ReleasePointer(p.capture, pointerId);
        p.pressed = 0;
        for (ElementHandle& e : p.pressedOn)
            e = {};
        SetUnder(pointerId, {}, 0, time);
        p.known = false;
    }

    void EventSystem::Wheel(Vector2 position, Vector2 ticks, u8 modifiers, f64 time, bool covered)
    {
        lastTime_ = time;
        Pointer& p = At(PointerId::mouse);
        p.known = true;
        p.kind = PointerKind::Mouse;
        p.position = position;
        ElementHandle under = covered ? ElementHandle{} : Pick(position);
        SetUnder(PointerId::mouse, under, modifiers, time);
        ElementHandle target = tree_.IsAlive(p.capture) ? p.capture : under;
        if (!target.IsValid())
            return;
        UIEvent e = PointerEvent(Events().wheel, PointerId::mouse, target, -1, modifiers, time, Vector2());
        // Lines, y down.
        e.delta = Vector2(ticks.x * scrollDeltaPerTick, -ticks.y * scrollDeltaPerTick);
        Send(e, nullptr);
    }

    // --------------------------------------------------------------------------------------------------- capture

    void EventSystem::CapturePointer(ElementHandle e, i32 pointerId)
    {
        Pointer& p = At(pointerId);
        if (!tree_.IsAlive(e) || p.capture == e)
            return;
        const EventTypes& t = Events();
        bool mouse = pointerId == PointerId::mouse;
        ElementHandle old = tree_.IsAlive(p.capture) ? p.capture : ElementHandle{};
        p.capture = e;
        auto send = [&](EventTypeId type, ElementHandle target, ElementHandle related) {
            UIEvent ev = PointerEvent(type, pointerId, target, -1, 0, lastTime_, Vector2());
            ev.relatedTarget = related;
            Send(ev, nullptr);
        };
        if (old.IsValid())
        {
            send(t.pointerCaptureOut, old, e);
            if (mouse)
                send(t.mouseCaptureOut, old, e);
        }
        send(t.pointerCapture, e, old);
        if (mouse)
            send(t.mouseCapture, e, old);
    }

    void EventSystem::ReleasePointer(ElementHandle e, i32 pointerId)
    {
        Pointer& p = At(pointerId);
        if (!e.IsValid() || p.capture != e)
            return;
        p.capture = {};
        if (!tree_.IsAlive(e))
            return;
        const EventTypes& t = Events();
        Send(PointerEvent(t.pointerCaptureOut, pointerId, e, -1, 0, lastTime_, Vector2()), nullptr);
        if (pointerId == PointerId::mouse)
            Send(PointerEvent(t.mouseCaptureOut, pointerId, e, -1, 0, lastTime_, Vector2()), nullptr);
    }

    ElementHandle EventSystem::CapturingElement(i32 pointerId) const
    {
        if (pointerId < 0 || pointerId >= PointerId::max || !tree_.IsAlive(pointers_[pointerId].capture))
            return {};
        return pointers_[pointerId].capture;
    }

    // ------------------------------------------------------------------------------------------------------ keys

    void EventSystem::KeyDown(u16 keyCode, u8 modifiers, f64 time)
    {
        lastTime_ = time;
        ElementHandle target = Focused().IsValid() ? Focused() : root_;
        if (!tree_.IsAlive(target))
            return;
        const EventTypes& t = Events();
        UIEvent e;
        e.type = t.keyDown;
        e.target = target;
        e.keyCode = keyCode;
        e.modifiers = modifiers;
        e.timestamp = time;
        Send(e, nullptr);
        // The navigation the key means, after it.
        for (const NavigationKey& k : kNavigationKeys)
            if (k.keyCode == keyCode)
            {
                UIEvent n;
                n.target = target;
                n.modifiers = modifiers;
                n.timestamp = time;
                if (k.kind == NavigationKind::Move)
                {
                    n.type = t.navigationMove;
                    n.direction = k.direction == NavigationDirection::Next && (modifiers & kShift) ? NavigationDirection::Previous : k.direction;
                    n.move = Vector2(k.direction == NavigationDirection::Left ? -1.0f : k.direction == NavigationDirection::Right ? 1.0f : 0.0f,
                                     k.direction == NavigationDirection::Up ? 1.0f : k.direction == NavigationDirection::Down ? -1.0f : 0.0f);
                }
                else
                    n.type = k.kind == NavigationKind::Submit ? t.navigationSubmit : t.navigationCancel;
                Send(n, k.kind == NavigationKind::Move ? &Navigate : nullptr);
                break;
            }
    }

    void EventSystem::KeyUp(u16 keyCode, u8 modifiers, f64 time)
    {
        lastTime_ = time;
        ElementHandle target = Focused().IsValid() ? Focused() : root_;
        if (!tree_.IsAlive(target))
            return;
        UIEvent e;
        e.type = Events().keyUp;
        e.target = target;
        e.keyCode = keyCode;
        e.modifiers = modifiers;
        e.timestamp = time;
        Send(e, nullptr);
    }

    void EventSystem::Character(u32 codepoint, u8 modifiers, f64 time)
    {
        lastTime_ = time;
        ElementHandle target = Focused().IsValid() ? Focused() : root_;
        if (!tree_.IsAlive(target))
            return;
        UIEvent e;
        e.type = Events().keyDown;
        e.target = target;
        e.character = codepoint;
        e.modifiers = modifiers;
        e.timestamp = time;
        Send(e, nullptr);
    }

    void EventSystem::SetComposition(Span<const u32> text, i32 cursor, f64 time)
    {
        lastTime_ = time;
        cursor = cursor < 0 ? 0 : cursor > i32(text.Size()) ? i32(text.Size()) : cursor;
        bool same = cursor == compositionCursor_ && text.Size() == composition_.Size();
        for (usize i = 0; same && i < text.Size(); i++)
            same = text[i] == composition_[i];
        if (same)
            return;
        composition_.Clear();
        for (u32 c : text)
            composition_.Push(c);
        compositionCursor_ = cursor;
        ElementHandle target = Focused().IsValid() ? Focused() : root_;
        if (!tree_.IsAlive(target))
            return;
        UIEvent e;
        e.type = Events().composition;
        e.target = target;
        e.timestamp = time;
        Send(e, nullptr);
    }

    void EventSystem::SetTextInput(ElementHandle e, const Rect& caret)
    {
        textElement_ = e;
        caret_ = caret;
    }

    bool EventSystem::TextInput(Rect& caret) const
    {
        if (!textElement_.IsValid() || Focused() != textElement_)
            return false;
        Rect r = layout_.WorldRect(textElement_);
        caret = Rect(r.x + caret_.x, r.y + caret_.y, caret_.width, caret_.height);
        return true;
    }

    void EventSystem::Navigate(EventSystem& events, const UIEvent& e)
    {
        // Stopped by an element that uses the key itself (a text field's arrows).
        if (!e.propagationStopped)
            events.MoveFocus(e.direction);
    }

    // ----------------------------------------------------------------------------------------------------- focus

    ElementHandle EventSystem::Focused() const { return tree_.IsAlive(focused_) ? focused_ : ElementHandle{}; }

    bool EventSystem::CanGrabFocus(ElementHandle e) const
    {
        return tree_.IsAlive(e) && tree_.Focusable(e) && tree_.EnabledInHierarchy(e) && Displayed(e) &&
               styles_.Computed(e).visibility == Visibility::Visible;
    }

    bool EventSystem::Focus(ElementHandle e)
    {
        if (!tree_.IsAlive(e))
            return false;
        ElementHandle to = e;
        if (tree_.DelegatesFocus(e))
        {
            // The first of its focusable descendants in the ring.
            Array<ElementHandle> ring;
            FocusRing(ring);
            to = {};
            for (ElementHandle x : ring)
                if (x != e && tree_.IsAncestor(e, x))
                {
                    to = x;
                    break;
                }
        }
        if (!CanGrabFocus(to))
            return false;
        SwitchFocus(to, lastTime_);
        return true;
    }

    void EventSystem::Blur(ElementHandle e)
    {
        if (e.IsValid() && Focused() == e)
            SwitchFocus({}, lastTime_);
    }

    void EventSystem::SwitchFocus(ElementHandle to, f64 time)
    {
        ElementHandle from = Focused();
        if (from == to)
            return;
        focusFrom_ = from;
        focusTo_ = to;
        focusTime_ = time;
        const EventTypes& t = Events();
        // FocusOut and FocusIn before the change, Blur and Focus after it.
        UIEvent e;
        e.timestamp = time;
        if (from.IsValid())
        {
            e.type = t.focusOut;
            e.target = from;
            e.relatedTarget = to;
            Send(e, to.IsValid() ? nullptr : &CommitFocus);
        }
        if (to.IsValid())
        {
            e.type = t.focusIn;
            e.target = to;
            e.relatedTarget = from;
            Send(e, &CommitFocus);
        }
    }

    void EventSystem::CommitFocus(EventSystem& events, const UIEvent&)
    {
        ElementTree& tree = events.tree_;
        ElementHandle from = events.focusFrom_, to = events.focusTo_;
        if (tree.IsAlive(from))
            tree.SetStates(from, tree.States(from) & ~PseudoStates::Focus);
        events.focused_ = to;
        if (tree.IsAlive(to))
            tree.SetStates(to, tree.States(to) | PseudoStates::Focus);
        const EventTypes& t = Events();
        UIEvent e;
        e.timestamp = events.focusTime_;
        if (tree.IsAlive(from))
        {
            e.type = t.blur;
            e.target = from;
            e.relatedTarget = to;
            events.Send(e, nullptr);
        }
        if (tree.IsAlive(to))
        {
            e.type = t.focus;
            e.target = to;
            e.relatedTarget = from;
            events.Send(e, nullptr);
        }
    }

    void EventSystem::FocusRing(Array<ElementHandle>& out) const
    {
        out.Clear();
        Array<i32> keys;
        tree_.ForEachInSubtree(root_, [&](ElementHandle e, u32) {
            i32 index = tree_.TabIndex(e);
            if (index < 0 || !CanGrabFocus(e))
                return;
            // Positive indices first in their order, then 0 in tree order (a stable insertion).
            i32 key = index > 0 ? index : 0x7FFFFFFF;
            usize at = keys.Size();
            while (at > 0 && keys[at - 1] > key)
                at--;
            keys.Insert(at, key);
            out.Insert(at, e);
        });
    }

    void EventSystem::MoveFocus(NavigationDirection direction)
    {
        Array<ElementHandle> ring;
        FocusRing(ring);
        if (ring.Empty() || direction == NavigationDirection::None)
            return;
        ElementHandle from = Focused();
        usize at = ring.Size();
        for (usize i = 0; i < ring.Size(); i++)
            if (ring[i] == from)
                at = i;
        if (direction == NavigationDirection::Next || direction == NavigationDirection::Previous)
        {
            usize n = ring.Size();
            usize next = at == n ? (direction == NavigationDirection::Next ? 0 : n - 1)
                                 : (direction == NavigationDirection::Next ? (at + 1) % n : (at + n - 1) % n);
            Focus(ring[next]);
            return;
        }
        if (!from.IsValid())
        {
            Focus(ring[0]);
            return;
        }
        // The nearest one that way, the distance across the way counting double.
        Vector2 way(direction == NavigationDirection::Left ? -1.0f : direction == NavigationDirection::Right ? 1.0f : 0.0f,
                    direction == NavigationDirection::Up ? -1.0f : direction == NavigationDirection::Down ? 1.0f : 0.0f);
        Vector2 c = layout_.WorldRect(from).center();
        ElementHandle best;
        f32 bestScore = 1e30f;
        for (ElementHandle x : ring)
        {
            if (x == from)
                continue;
            Vector2 d = layout_.WorldRect(x).center();
            d = Vector2(d.x - c.x, d.y - c.y);
            f32 along = d.x * way.x + d.y * way.y;
            if (along <= 0.5f)
                continue;
            f32 across = std::fabs(d.x * way.y - d.y * way.x);
            f32 score = along + 2 * across;
            if (score < bestScore)
            {
                bestScore = score;
                best = x;
            }
        }
        if (best.IsValid())
            Focus(best);
    }

    // ------------------------------------------------------------------------------------------- drag and drop

    void EventSystem::StartDrag(i32 pointerId, Atom kind, void* data)
    {
        if (pointerId < 0 || pointerId >= PointerId::max)
            return;
        drag_ = Drag();
        drag_.active = true;
        drag_.pointerId = pointerId;
        drag_.kind = kind;
        drag_.data = data;
        UpdateDrag(0, lastTime_);
    }

    void EventSystem::UpdateDrag(u8 modifiers, f64 time)
    {
        i32 id = drag_.pointerId;
        ElementHandle under = ElementUnderPointer(id);
        ElementHandle previous = tree_.IsAlive(drag_.over) ? drag_.over : ElementHandle{};
        const EventTypes& t = Events();
        if (under != previous)
        {
            drag_.over = under;
            Array<ElementHandle> chain, left, entered;
            for (ElementHandle x = under; x.IsValid(); x = tree_.Parent(x))
                chain.Push(x);
            ElementHandle common;
            for (ElementHandle x = previous; x.IsValid(); x = tree_.Parent(x))
            {
                if (Contains(chain, x))
                {
                    common = x;
                    break;
                }
                left.Push(x);
            }
            for (ElementHandle x : chain)
            {
                if (x == common)
                    break;
                entered.Insert(0, x);
            }
            for (ElementHandle x : left)
            {
                UIEvent e = PointerEvent(t.dragLeave, id, x, -1, modifiers, time, Vector2());
                e.relatedTarget = under;
                Send(e, nullptr);
            }
            for (ElementHandle x : entered)
            {
                UIEvent e = PointerEvent(t.dragEnter, id, x, -1, modifiers, time, Vector2());
                e.relatedTarget = previous;
                Send(e, nullptr);
            }
        }
        // The element under it says what a drop would do.
        drag_.mode = DragVisualMode::None;
        if (under.IsValid())
            Send(PointerEvent(t.dragUpdated, id, under, -1, modifiers, time, Vector2()), nullptr);
    }

    void EventSystem::EndDrag(i32 pointerId, bool dropped, u8 modifiers, f64 time)
    {
        const EventTypes& t = Events();
        ElementHandle over = tree_.IsAlive(drag_.over) ? drag_.over : ElementHandle{};
        if (dropped && over.IsValid() && drag_.mode != DragVisualMode::None && drag_.mode != DragVisualMode::Rejected)
            Send(PointerEvent(t.dragPerform, pointerId, over, 0, modifiers, time, Vector2()), nullptr);
        ElementHandle target = over.IsValid() ? over : root_;
        if (tree_.IsAlive(target))
            Send(PointerEvent(t.dragExited, pointerId, target, 0, modifiers, time, Vector2()), &FinishDrag);
        else
            drag_ = Drag();
    }

    void EventSystem::FinishDrag(EventSystem& events, const UIEvent&) { events.drag_ = Drag(); }

    // ----------------------------------------------------------------------------------------------------- state

    void EventSystem::Forget()
    {
        for (Pointer& p : pointers_)
        {
            if (!tree_.IsAlive(p.under))
                p.under = {};
            if (!tree_.IsAlive(p.capture))
                p.capture = {};
            for (ElementHandle& e : p.pressedOn)
                if (!tree_.IsAlive(e))
                    e = {};
        }
        for (usize i = 0; i < hovered_.Size();)
            if (!tree_.IsAlive(hovered_[i]))
                hovered_.RemoveAt(i);
            else
                i++;
        if (!tree_.IsAlive(focused_))
            focused_ = {};
        if (!tree_.IsAlive(drag_.over))
            drag_.over = {};
    }
}
