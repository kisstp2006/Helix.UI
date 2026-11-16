#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/Layout/Layout.h>
#include <Helix/UI/Markup/Hxml.h>
#include <Helix/Testing/Test.h>

#include <cstdio>
#include <initializer_list>

using namespace helix;
using namespace helix::ui;

namespace
{
    // A page of markup laid out in 200 x 200 pixels with its event system, and a log of the events its elements got.
    struct Page
    {
        ElementTree tree;
        LayoutEngine layout{tree};
        StyleEngine styles{tree, layout};
        EventSystem events{tree, layout, styles};
        ElementHandle root;
        Array<String> log;

        explicit Page(StringView markup)
        {
            root = tree.Create(Intern("Element"));
            tree.SetPicking(root, PickingMode::Ignore);
            events.SetRoot(root);
            HxmlDocument doc;
            Array<HxmlError> errors;
            HxmlDocument::Parse(markup, doc, errors);
            Instantiate(doc, tree, styles, root, nullptr, errors);
            for (const HxmlError& e : errors)
                std::printf("    %u:%u %s\n", e.line, e.column, e.message.CStr());
            Update();
        }

        void Update()
        {
            styles.Update(root, 0);
            layout.Update(root, 200, 200);
        }

        ElementHandle Named(StringView name) const
        {
            ElementHandle found;
            tree.ForEachInSubtree(root, [&](ElementHandle e, u32) {
                if (tree.Name(e) == Intern(name))
                    found = e;
            });
            return found;
        }

        static void Record(void* user, EventSystem& events, UIEvent& e)
        {
            Page& p = *static_cast<Page*>(user);
            StringView type = AtomName(EventTypeOf(e.type).name);
            // The type without "Event", where it is, the phase (v trickle down, ^ bubble up).
            type = type.Substr(0, type.Size() - 5);
            String line(type);
            line += ":";
            line += AtomName(events.tree().Name(e.currentTarget));
            line += e.phase == PropagationPhase::TrickleDown ? "v" : "^";
            p.log.Push(line);
        }

        // Every element named gets a recording callback for each type, in a phase or both.
        void Listen(std::initializer_list<EventTypeId> types, bool trickle = false, bool bubble = true)
        {
            tree.ForEachInSubtree(root, [&](ElementHandle e, u32) {
                if (tree.Name(e) == kNoAtom)
                    return;
                for (EventTypeId t : types)
                {
                    if (trickle)
                        events.RegisterCallback(e, t, &Record, this, TrickleDown::TrickleDown);
                    if (bubble)
                        events.RegisterCallback(e, t, &Record, this);
                }
            });
        }

        String Log() const
        {
            String all;
            for (const String& s : log)
            {
                if (!all.Empty())
                    all += " ";
                all += s;
            }
            return all;
        }

        bool LogIs(StringView expected)
        {
            String all = Log();
            bool ok = all == expected;
            if (!ok)
                std::printf("    log:      %s\n    expected: %.*s\n", all.CStr(), int(expected.Size()), expected.Data());
            log.Clear();
            return ok;
        }
    };

    // a holds b holds c; d beside them.
    constexpr StringView kNested = R"(<HXML>
  <Element name="a" style="position: absolute; left: 10px; top: 10px; width: 100px; height: 100px;">
    <Element name="b" style="position: absolute; left: 10px; top: 10px; width: 60px; height: 60px;">
      <Element name="c" style="position: absolute; left: 10px; top: 10px; width: 20px; height: 20px;"/>
    </Element>
  </Element>
  <Element name="d" style="position: absolute; left: 150px; top: 10px; width: 40px; height: 40px;"/>
</HXML>)";

    void StopAtB(void* user, EventSystem& events, UIEvent& e)
    {
        Page::Record(user, events, e);
        e.StopPropagation();
    }

    void StopImmediatelyAtB(void* user, EventSystem& events, UIEvent& e)
    {
        Page::Record(user, events, e);
        e.StopImmediatePropagation();
    }
}

HX_TEST(Events, TrickleDownFromTheRootThenBubbleUpBack)
{
    Page p(kNested);
    const EventTypes& t = Events();
    p.Listen({t.pointerDown}, true, true);
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0);
    HX_CHECK(p.LogIs("PointerDown:av PointerDown:bv PointerDown:cv PointerDown:c^ PointerDown:b^ PointerDown:a^"));
}

HX_TEST(Events, StopPropagationLetsTheCurrentElementFinish)
{
    Page p(kNested);
    const EventTypes& t = Events();
    ElementHandle b = p.Named("b");
    p.events.RegisterCallback(b, t.pointerUp, &StopAtB, &p);
    p.Listen({t.pointerUp});
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0);
    // b's other callback still runs, a's does not.
    HX_CHECK(p.LogIs("PointerUp:c^ PointerUp:b^ PointerUp:b^"));

    Page q(kNested);
    q.events.RegisterCallback(q.Named("b"), t.pointerUp, &StopImmediatelyAtB, &q);
    q.Listen({t.pointerUp});
    q.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0);
    HX_CHECK(q.LogIs("PointerUp:c^ PointerUp:b^"));
}

HX_TEST(Events, AMoveIsFollowedByOverAndEnterThenTheMouseOnes)
{
    Page p(kNested);
    const EventTypes& t = Events();
    p.Listen({t.pointerMove, t.pointerOver, t.pointerOut, t.pointerEnter, t.pointerLeave, t.mouseMove, t.mouseOver, t.mouseOut, t.mouseEnter,
              t.mouseLeave});
    // From nowhere into c: no move target before; over bubbles, enter goes to each element entered from the top.
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(35, 35), 0, 0);
    HX_CHECK(p.LogIs("PointerMove:c^ PointerMove:b^ PointerMove:a^ MouseMove:c^ MouseMove:b^ MouseMove:a^ PointerOver:c^ PointerOver:b^ "
                     "PointerOver:a^ PointerEnter:a^ PointerEnter:b^ PointerEnter:c^ MouseOver:c^ MouseOver:b^ MouseOver:a^ MouseEnter:a^ "
                     "MouseEnter:b^ MouseEnter:c^"));
    // From c to b: out of c, leave c only, over b (b and a were entered already).
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(25, 25), 0, 1);
    HX_CHECK(p.LogIs("PointerMove:b^ PointerMove:a^ MouseMove:b^ MouseMove:a^ PointerOut:c^ PointerOut:b^ PointerOut:a^ PointerLeave:c^ "
                     "PointerOver:b^ PointerOver:a^ MouseOut:c^ MouseOut:b^ MouseOut:a^ MouseLeave:c^ MouseOver:b^ MouseOver:a^"));
    // From b to d: leave b then a.
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(160, 20), 0, 2);
    HX_CHECK(p.LogIs("PointerMove:d^ MouseMove:d^ PointerOut:b^ PointerOut:a^ PointerLeave:b^ PointerLeave:a^ PointerOver:d^ PointerEnter:d^ "
                     "MouseOut:b^ MouseOut:a^ MouseLeave:b^ MouseLeave:a^ MouseOver:d^ MouseEnter:d^"));
}

HX_TEST(Events, TheMouseHoversTheElementUnderItAndItsAncestors)
{
    Page p(kNested);
    ElementHandle a = p.Named("a"), b = p.Named("b"), c = p.Named("c"), d = p.Named("d");
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(35, 35), 0, 0);
    auto hover = [&](ElementHandle e) { return Any(p.tree.States(e) & PseudoStates::Hover); };
    HX_CHECK(hover(a) && hover(b) && hover(c) && !hover(d));
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(160, 20), 0, 1);
    HX_CHECK(!hover(a) && !hover(b) && !hover(c) && hover(d));
    // A touch does not hover.
    p.events.PointerMove(PointerId::touchBase, PointerKind::Touch, Vector2(35, 35), 0, 2);
    HX_CHECK(!hover(c) && hover(d));
}

HX_TEST(Events, AClickIsAPressAndAReleaseOnOneElement)
{
    Page p(kNested);
    const EventTypes& t = Events();
    i32 clicks = 0, lastCount = 0;
    struct Counter
    {
        i32* clicks;
        i32* count;
    } counter{&clicks, &lastCount};
    p.events.RegisterCallback(p.Named("c"), t.click, [](void* u, EventSystem&, UIEvent& e) {
        auto* c = static_cast<Counter*>(u);
        (*c->clicks)++;
        *c->count = e.clickCount;
    }, &counter);
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0);
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(36, 36), 0, 0.05);
    HX_CHECK(clicks == 1 && lastCount == 1);
    // Again soon after: a double click.
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0.2);
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0.25);
    HX_CHECK(clicks == 2 && lastCount == 2);
    // Released elsewhere: no click.
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 2);
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(160, 20), 0, 2.1);
    HX_CHECK(clicks == 2);
    // The right button does not click.
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 1, Vector2(35, 35), 0, 4);
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 1, Vector2(35, 35), 0, 4.1);
    HX_CHECK(clicks == 2);
}

HX_TEST(Events, PickingFindsTheTopMostPickableElement)
{
    Page p(R"(<HXML>
  <Element name="under" style="position: absolute; left: 0px; top: 0px; width: 100px; height: 100px;"/>
  <Element name="over" style="position: absolute; left: 50px; top: 50px; width: 100px; height: 100px;"/>
  <Element name="ghost" picking-mode="Ignore" style="position: absolute; left: 0px; top: 150px; width: 50px; height: 50px;">
    <Element name="inside" style="position: absolute; left: 10px; top: 10px; width: 20px; height: 20px;"/>
  </Element>
  <Element name="clip" style="position: absolute; left: 100px; top: 150px; width: 40px; height: 40px; overflow: hidden;">
    <Element name="wide" style="position: absolute; left: 0px; top: 0px; width: 90px; height: 20px;"/>
  </Element>
  <Element name="hidden" style="position: absolute; left: 160px; top: 0px; width: 40px; height: 40px; visibility: hidden;">
    <Element name="shown" style="position: absolute; left: 0px; top: 0px; width: 10px; height: 10px; visibility: visible;"/>
  </Element>
  <Element name="none" style="position: absolute; left: 160px; top: 50px; width: 40px; height: 40px; display: none;"/>
</HXML>)");
    auto at = [&](f32 x, f32 y) { return AtomName(p.tree.Name(p.events.Pick(Vector2(x, y)))); };
    HX_CHECK(at(20, 20) == "under" && at(75, 75) == "over");
    // Ignored: its child picks, the rest of it is empty.
    HX_CHECK(at(20, 170) == "inside" && at(5, 155).Empty());
    // Clipped to its parent's box.
    HX_CHECK(at(110, 155) == "wide" && at(150, 155).Empty());
    HX_CHECK(at(165, 5) == "shown" && at(190, 30).Empty());
    HX_CHECK(at(170, 60).Empty());
    Array<ElementHandle> all;
    p.events.PickAll(Vector2(75, 75), all);
    HX_CHECK(all.Size() == 2 && all[0] == p.Named("over") && all[1] == p.Named("under"));
}

HX_TEST(Events, DisabledElementsGetNoCallbacksButTheirAncestorsDo)
{
    Page p(R"(<HXML>
  <Element name="a" style="position: absolute; left: 0px; top: 0px; width: 100px; height: 100px;">
    <Element name="b" enabled="false" style="position: absolute; left: 0px; top: 0px; width: 50px; height: 50px;">
      <Element name="c" style="position: absolute; left: 0px; top: 0px; width: 20px; height: 20px;"/>
    </Element>
  </Element>
</HXML>)");
    const EventTypes& t = Events();
    p.Listen({t.pointerDown});
    HX_CHECK(Any(p.tree.States(p.Named("b")) & PseudoStates::Disabled) && !p.tree.EnabledInHierarchy(p.Named("c")));
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(5, 5), 0, 0);
    HX_CHECK(p.LogIs("PointerDown:a^"));
}

HX_TEST(Events, ACapturedPointerGoesToItsCaptureWhereverItIs)
{
    Page p(kNested);
    const EventTypes& t = Events();
    ElementHandle b = p.Named("b");
    p.Listen({t.pointerMove, t.pointerCapture, t.pointerCaptureOut, t.mouseCapture, t.mouseCaptureOut});
    p.events.CapturePointer(b, PointerId::mouse);
    HX_CHECK(p.LogIs("PointerCapture:b^ PointerCapture:a^ MouseCapture:b^ MouseCapture:a^"));
    HX_CHECK(p.events.HasPointerCapture(b, PointerId::mouse));
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(160, 20), 0, 0);
    HX_CHECK(p.LogIs("PointerMove:b^ PointerMove:a^"));
    p.events.ReleasePointer(b, PointerId::mouse);
    HX_CHECK(p.LogIs("PointerCaptureOut:b^ PointerCaptureOut:a^ MouseCaptureOut:b^ MouseCaptureOut:a^"));
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(161, 20), 0, 1);
    HX_CHECK(p.LogIs("PointerMove:d^"));
}

namespace
{
    constexpr StringView kFocusable = R"(<HXML>
  <Element name="first" focusable="true" style="position: absolute; left: 0px; top: 0px; width: 40px; height: 40px;"/>
  <Element name="second" focusable="true" tabindex="2" style="position: absolute; left: 50px; top: 0px; width: 40px; height: 40px;"/>
  <Element name="third" focusable="true" tabindex="1" style="position: absolute; left: 0px; top: 50px; width: 40px; height: 40px;">
    <Element name="inner" style="position: absolute; left: 5px; top: 5px; width: 10px; height: 10px;"/>
  </Element>
  <Element name="out" focusable="true" tabindex="-1" style="position: absolute; left: 50px; top: 50px; width: 40px; height: 40px;"/>
  <Element name="plain" style="position: absolute; left: 100px; top: 100px; width: 40px; height: 40px;"/>
</HXML>)";
}

HX_TEST(Events, APressFocusesAndTheFocusEventsComeInTheirOrder)
{
    Page p(kFocusable);
    const EventTypes& t = Events();
    p.Listen({t.focusOut, t.focusIn, t.blur, t.focus});
    // A press on a child goes to its focusable ancestor.
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(10, 60), 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("third"));
    HX_CHECK(p.LogIs("FocusIn:third^ Focus:third^"));
    HX_CHECK(Any(p.tree.States(p.Named("third")) & PseudoStates::Focus));
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(10, 10), 0, 1);
    HX_CHECK(p.LogIs("FocusOut:third^ FocusIn:first^ Blur:third^ Focus:first^"));
    HX_CHECK(!Any(p.tree.States(p.Named("third")) & PseudoStates::Focus) && Any(p.tree.States(p.Named("first")) & PseudoStates::Focus));
    // Nothing focusable under the press: the focus goes.
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(120, 120), 0, 2);
    HX_CHECK(!p.events.Focused().IsValid());
    HX_CHECK(p.LogIs("FocusOut:first^ Blur:first^"));
}

HX_TEST(Events, TabGoesRoundTheRingByTabIndexThenTreeOrder)
{
    Page p(kFocusable);
    Array<ElementHandle> ring;
    p.events.FocusRing(ring);
    // third (1), second (2), then first (0); out (-1) is not in it.
    HX_CHECK(ring.Size() == 3 && ring[0] == p.Named("third") && ring[1] == p.Named("second") && ring[2] == p.Named("first"));
    const u16 tab = 9;
    p.events.KeyDown(tab, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("third"));
    p.events.KeyDown(tab, 0, 0);
    p.events.KeyDown(tab, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("first"));
    p.events.KeyDown(tab, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("third"));
    // Shift+Tab goes back.
    p.events.KeyDown(tab, 1, 0);
    HX_CHECK(p.events.Focused() == p.Named("first"));
    // Out of the ring, it can still be focused by code.
    HX_CHECK(p.events.Focus(p.Named("out")) && p.events.Focused() == p.Named("out"));
    HX_CHECK(!p.events.Focus(p.Named("plain")));
}

HX_TEST(Events, KeysGoToTheFocusThenNavigationFollows)
{
    Page p(kFocusable);
    const EventTypes& t = Events();
    p.Listen({t.keyDown, t.keyUp, t.navigationMove, t.navigationSubmit, t.navigationCancel});
    // Nothing focused: to the root (which has no name, so nothing is logged).
    p.events.KeyDown(97, 0, 0);
    HX_CHECK(p.LogIs(""));
    p.events.Focus(p.Named("first"));
    p.events.KeyDown(97, 0, 0);
    p.events.Character('a', 0, 0);
    p.events.KeyUp(97, 0, 0);
    HX_CHECK(p.LogIs("KeyDown:first^ KeyDown:first^ KeyUp:first^"));
    p.events.KeyDown(13, 0, 0);
    p.events.KeyDown(27, 0, 0);
    HX_CHECK(p.LogIs("KeyDown:first^ NavigationSubmit:first^ KeyDown:first^ NavigationCancel:first^"));
    // The right arrow moves the focus to the nearest one to the right.
    p.events.KeyDown(275, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("second"));
    p.log.Clear();
    // The down arrow from second: out is below it but not in the ring, third is down and left.
    p.events.KeyDown(274, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("third"));
}

HX_TEST(Events, AStoppedNavigationKeepsTheFocus)
{
    Page p(kFocusable);
    const EventTypes& t = Events();
    p.events.Focus(p.Named("first"));
    p.events.RegisterCallback(p.Named("first"), t.navigationMove, [](void*, EventSystem&, UIEvent& e) { e.StopPropagation(); }, nullptr);
    p.events.KeyDown(275, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("first"));
    p.events.KeyDown(9, 0, 0);
    HX_CHECK(p.events.Focused() == p.Named("first"));
}

HX_TEST(Events, AFocusThatDelegatesGoesToItsFirstFocusableChild)
{
    Page p(R"(<HXML>
  <Element name="group" style="position: absolute; left: 0px; top: 0px; width: 100px; height: 100px;">
    <Element name="label" style="height: 20px;"/>
    <Element name="field" focusable="true" style="height: 20px;"/>
  </Element>
</HXML>)");
    p.tree.SetFocusable(p.Named("group"), true);
    p.tree.SetDelegatesFocus(p.Named("group"), true);
    HX_CHECK(p.events.Focus(p.Named("group")) && p.events.Focused() == p.Named("field"));
}

namespace
{
    struct DropTarget
    {
        DragVisualMode mode;
        i32 performed = 0;
        void* data = nullptr;
    };
}

HX_TEST(Events, DragAndDropAsksTheTargetAndDropsWhereItIsAccepted)
{
    Page p(kNested);
    const EventTypes& t = Events();
    DropTarget target{DragVisualMode::Move};
    ElementHandle d = p.Named("d");
    p.events.RegisterCallback(d, t.dragUpdated, [](void* u, EventSystem& events, UIEvent&) {
        events.SetVisualMode(static_cast<DropTarget*>(u)->mode);
    }, &target);
    p.events.RegisterCallback(d, t.dragPerform, [](void* u, EventSystem& events, UIEvent&) {
        static_cast<DropTarget*>(u)->performed++;
        static_cast<DropTarget*>(u)->data = events.DragData();
    }, &target);
    p.Listen({t.dragEnter, t.dragLeave, t.dragExited});
    int payload = 7;
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0);
    p.events.StartDrag(PointerId::mouse, Intern("item"), &payload);
    HX_CHECK(p.events.Dragging() && p.events.DragKind() == Intern("item"));
    p.log.Clear();
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(160, 20), 0, 0.1);
    HX_CHECK(p.LogIs("DragLeave:c^ DragLeave:b^ DragLeave:a^ DragEnter:d^"));
    HX_CHECK(p.events.VisualMode() == DragVisualMode::Move);
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(160, 20), 0, 0.2);
    HX_CHECK(target.performed == 1 && target.data == &payload);
    HX_CHECK(p.LogIs("DragExited:d^") && !p.events.Dragging());

    // Rejected: no drop.
    target.mode = DragVisualMode::Rejected;
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 1);
    p.events.StartDrag(PointerId::mouse, Intern("item"), &payload);
    p.events.PointerMove(PointerId::mouse, PointerKind::Mouse, Vector2(160, 20), 0, 1.1);
    p.events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(160, 20), 0, 1.2);
    HX_CHECK(target.performed == 1 && !p.events.Dragging());
}

namespace
{
    struct Chain
    {
        EventTypeId custom;
        Page* page;
    };
}

HX_TEST(Events, EventsSentWhileDispatchingWaitForTheCurrentOne)
{
    Page p(kNested);
    const EventTypes& t = Events();
    EventTypeId custom = RegisterEventType("TestChainEvent", false, true, EventFamily::Other);
    HX_CHECK(RegisterEventType("TestChainEvent", true, true, EventFamily::Other) == custom && FindEventType("TestChainEvent") == custom);
    Chain chain{custom, &p};
    // c sends the custom event to d while its pointer down is still on its way up.
    p.events.RegisterCallback(p.Named("c"), t.pointerDown, [](void* u, EventSystem& events, UIEvent&) {
        auto* c = static_cast<Chain*>(u);
        UIEvent e;
        e.type = c->custom;
        e.target = c->page->Named("d");
        events.SendEvent(e);
    }, &chain);
    p.Listen({t.pointerDown, custom});
    p.events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(35, 35), 0, 0);
    HX_CHECK(p.LogIs("PointerDown:c^ PointerDown:b^ PointerDown:a^ TestChain:d^"));
}

HX_TEST(Events, TheCompositionGoesToTheFocusAndTextInputLastsWhileItHasIt)
{
    Page p(kFocusable);
    const EventTypes& t = Events();
    ElementHandle first = p.Named("first");
    p.Listen({t.composition});
    p.events.Focus(first);
    const u32 text[] = {0x304B, 0x3093};
    p.events.SetComposition(Span<const u32>(text, 2), 5, 0);
    HX_CHECK(p.events.Composition().Size() == 2 && p.events.CompositionCursor() == 2);
    // The same again changes nothing; its end does.
    p.events.SetComposition(Span<const u32>(text, 2), 2, 0);
    p.events.SetComposition(Span<const u32>(), 0, 0);
    HX_CHECK(p.LogIs("Composition:first^ Composition:first^"));
    // The caret in the element's box, given in the panel's (first is at 0, 0; second at 50, 0).
    Rect caret;
    HX_CHECK(!p.events.TextInput(caret));
    ElementHandle second = p.Named("second");
    p.events.Focus(second);
    p.events.SetTextInput(second, Rect(4, 2, 1, 16));
    HX_CHECK(p.events.TextInput(caret) && caret.x == 54 && caret.y == 2 && caret.height == 16);
    // Off when the focus goes.
    p.events.Focus(first);
    HX_CHECK(!p.events.TextInput(caret));
}
