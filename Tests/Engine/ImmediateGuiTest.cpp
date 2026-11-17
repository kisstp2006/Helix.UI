#include <Engine/Support/CppScripts.h>
#include <Engine/Support/TestAssets.h>
#include <Engine/Support/TestPipeline.h>
#include <Support/Devices.h>
#include <Support/Golden.h>

#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Input/SystemClipboard.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Engine/Controls.h>
#include <Helix/UI/Engine/Gui.h>
#include <Helix/UI/Engine/UIView.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace helix;
using namespace helix::engine;
using namespace helix::ui;
using namespace helix::ui::gui;
using helix::ui::testing::AssetFolder;
using helix::ui::Button;
using helix::ui::testing::CppNotes;
using helix::ui::testing::GuiHook;
using helix::ui::testing::LoadAt;
using helix::ui::testing::Notes;
using helix::ui::testing::PageFont;
using helix::ui::testing::RegisterModules;
using helix::ui::testing::ReportScreen;
using helix::ui::testing::StartScripts;

namespace
{
    f64 gTime = 10;

    engine::InputEvent Mouse(platform::EventType type, f32 x, f32 y, platform::MouseButton button = platform::MouseButton::Left)
    {
        engine::InputEvent e;
        e.event.type = type;
        e.event.mouse.x = x;
        e.event.mouse.y = y;
        e.event.mouse.button = button;
        e.time = gTime;
        return e;
    }

    engine::InputEvent Key(platform::KeyCode key, u8 modifiers = 0, bool down = true)
    {
        engine::InputEvent e;
        e.event.type = down ? platform::EventType::KeyDown : platform::EventType::KeyUp;
        e.event.key.key = key;
        e.event.modifiers = modifiers;
        e.time = gTime;
        return e;
    }

    engine::InputEvent Char(u32 c)
    {
        engine::InputEvent e;
        e.event.type = platform::EventType::Text;
        e.event.text.codepoint = c;
        e.time = gTime;
        return e;
    }

    engine::InputEvent Wheel(f32 dy)
    {
        engine::InputEvent e;
        e.event.type = platform::EventType::MouseWheel;
        e.event.wheel.dy = dy;
        e.time = gTime;
        return e;
    }

    // A frame: its input through the scripts, then their Repaint.
    void Frame(std::initializer_list<engine::InputEvent> events = {})
    {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), {});
        system::SendQueuedEvents();
        system::Repaint();
        gTime += 1;
    }

    void Click(f32 x, f32 y)
    {
        Frame({Mouse(platform::EventType::MouseMove, x, y), Mouse(platform::EventType::MouseDown, x, y), Mouse(platform::EventType::MouseUp, x, y)});
    }

    void Type(StringView text)
    {
        for (char c : text)
            Frame({Char(u32(u8(c)))});
    }

    // A C++ script whose OnGui is the test's.
    struct GuiScene
    {
        // Its objects go with the folder's teardown (every object).
        AssetFolder folder;
        Array<GameObject*> hosts;

        explicit GuiScene(void (*hook)(StringView), std::initializer_list<const char*> names = {"Gui"})
        {
            ReportScreen(400, 300);
            system::ResetForTests();
            engine::InputFeed::Reset();
            GuiHook() = hook;
            for (const char* n : names)
            {
                GameObject* g = GameObject::Create(n);
                g->AddComponent(scripting::TypeOf<helix::ui::testing::CppGuiHook>());
                hosts.Push(g);
            }
            Frame();
        }

        ~GuiScene()
        {
            GuiHook() = nullptr;
            for (GameObject* g : hosts)
                DestroyImmediate(g);
            system::ResetForTests();
            engine::InputFeed::Reset();
        }
    };

    bool Near(f32 a, f32 b, f32 tolerance = 0.51f) { return std::fabs(a - b) <= tolerance; }

    // The controls' state the hooks keep.
    struct ControlState
    {
        i32 pressed = 0, repeat = 0, disabledPressed = 0, tool = 0, grid = 0;
        bool toggle = false, changed = false;
        f32 slider = 0, scroll = 0;
    } gControls;
}

HX_TEST(ImmediateGui, ButtonsTogglesSlidersAndGridsTakeTheMouse)
{
    HX_REQUIRE(RegisterModules());
    gControls = ControlState();
    GuiScene scene([](StringView) {
        ControlState& c = gControls;
        if (Gui::Button(Rect(10, 10, 80, 20), "Go"))
            c.pressed++;
        if (Gui::RepeatButton(Rect(100, 10, 80, 20), "Hold"))
            c.repeat++;
        Gui::SetChanged(false);
        c.toggle = Gui::Toggle(Rect(10, 40, 80, 20), c.toggle, "T");
        if (Gui::changed())
            c.changed = true;
        c.slider = Gui::HorizontalSlider(Rect(10, 70, 200, 20), c.slider, 0, 100);
        const String tabs[3] = {String("a"), String("b"), String("c")};
        c.tool = Gui::Toolbar(Rect(10, 100, 150, 20), c.tool, Span<const String>(tabs, 3));
        c.grid = Gui::SelectionGrid(Rect(10, 130, 100, 40), c.grid, Span<const String>(tabs, 3), 2);
        c.scroll = Gui::VerticalScrollbar(Rect(300, 10, 12, 200), c.scroll, 20, 0, 100);
        Gui::SetEnabled(false);
        if (Gui::Button(Rect(200, 40, 60, 20), "Off"))
            c.disabledPressed++;
        Gui::SetEnabled(true);
    });
    ControlState& c = gControls;
    // A button is pressed when the mouse goes up over it after going down over it.
    Frame({Mouse(platform::EventType::MouseMove, 50, 20), Mouse(platform::EventType::MouseDown, 50, 20)});
    HX_CHECK(c.pressed == 0 && GuiUtility::hotControl() != 0);
    Frame({Mouse(platform::EventType::MouseUp, 50, 20)});
    HX_CHECK(c.pressed == 1 && GuiUtility::hotControl() == 0);
    // Down over it, up away from it: no press.
    Frame({Mouse(platform::EventType::MouseDown, 50, 20), Mouse(platform::EventType::MouseMove, 150, 250), Mouse(platform::EventType::MouseUp, 150, 250)});
    HX_CHECK(c.pressed == 1);
    // A repeat button is pressed in each Repaint while it is held over.
    Frame({Mouse(platform::EventType::MouseMove, 140, 20), Mouse(platform::EventType::MouseDown, 140, 20)});
    Frame();
    Frame();
    HX_CHECK(c.repeat == 3);
    Frame({Mouse(platform::EventType::MouseUp, 140, 20)});
    Frame();
    HX_CHECK(c.repeat == 3);
    // A toggle flips on its click (GUI.changed with it).
    Click(30, 50);
    HX_CHECK(c.toggle && c.changed);
    Click(30, 50);
    HX_CHECK(!c.toggle);
    // A slider jumps to the click (its track is its padding's inside: 202 pixels, the thumb 12) and follows a drag.
    Frame({Mouse(platform::EventType::MouseMove, 9 + 6 + 0.75f * 190, 80), Mouse(platform::EventType::MouseDown, 9 + 6 + 0.75f * 190, 80)});
    HX_CHECK(Near(c.slider, 75, 0.6f));
    Frame({Mouse(platform::EventType::MouseMove, 9 + 6 + 0.25f * 190, 80)});
    HX_CHECK(Near(c.slider, 25, 0.6f));
    Frame({Mouse(platform::EventType::MouseMove, 500, 80), Mouse(platform::EventType::MouseUp, 500, 80)});
    HX_CHECK(Near(c.slider, 100, 0.01f));
    // A toolbar's cells split its width; a grid's cells its columns and rows (the style's margin between them).
    Click(85, 110);
    HX_CHECK(c.tool == 1);
    Click(70, 140);
    HX_CHECK(c.grid == 1);
    Click(20, 162);
    HX_CHECK(c.grid == 2);
    // A scrollbar's track pages toward the click (its thumb as long as the size's share).
    Click(306, 150);
    HX_CHECK(Near(c.scroll, 20, 0.01f));
    Click(306, 180);
    HX_CHECK(Near(c.scroll, 40, 0.01f));
    Click(306, 15);
    HX_CHECK(Near(c.scroll, 20, 0.01f));
    // A disabled control takes no input.
    Click(230, 50);
    HX_CHECK(c.disabledPressed == 0);
    // Repaint drew into the GUI's mesh.
    HX_CHECK(!system::Mesh().Vertices().Empty());
}

namespace
{
    struct ClipState
    {
        i32 inGroup = 0, clipped = 0, deep = 0, scaled = 0;
        Vector2 local, scroll;
    } gClips;
}

HX_TEST(ImmediateGui, GroupsClipsScrollViewsAndTheMatrixMoveTheCoordinates)
{
    HX_REQUIRE(RegisterModules());
    gClips = ClipState();
    GuiScene scene([](StringView) {
        ClipState& c = gClips;
        Gui::BeginGroup(Rect(100, 100, 100, 60));
        if (Gui::Button(Rect(0, 0, 50, 20), "In"))
            c.inGroup++;
        // Half of it outside the group: only the inside takes the mouse.
        if (Gui::Button(Rect(80, 30, 50, 20), "Cut"))
            c.clipped++;
        if (gui::Event::current()->type() == EventType::MouseMove)
            c.local = gui::Event::current()->mousePosition();
        Gui::EndGroup();
        c.scroll = Gui::BeginScrollView(Rect(10, 10, 80, 60), c.scroll, Rect(0, 0, 60, 200));
        if (Gui::Button(Rect(0, 100, 60, 20), "Deep"))
            c.deep++;
        Gui::EndScrollView();
        Gui::SetMatrix(Matrix4x4::Scale(Vector3(2, 2, 1)));
        if (Gui::Button(Rect(100, 100, 20, 10), "Big"))
            c.scaled++;
        Gui::SetMatrix(Matrix4x4::identity);
    });
    ClipState& c = gClips;
    // A group's controls are in its space; the mouse is too.
    Frame({Mouse(platform::EventType::MouseMove, 130, 110)});
    HX_CHECK(Near(c.local.x, 30) && Near(c.local.y, 10));
    Click(110, 110);
    HX_CHECK(c.inGroup == 1);
    Click(190, 140);
    HX_CHECK(c.clipped == 1);
    Click(210, 140);
    HX_CHECK(c.clipped == 1);
    // A scroll view: what is below its rect takes no click until the wheel scrolls it there (three lines of 20
    // pixels a notch, kept inside the view).
    Click(30, 55);
    HX_CHECK(c.deep == 0);
    Frame({Mouse(platform::EventType::MouseMove, 30, 40), Wheel(-1)});
    Frame();
    HX_CHECK(Near(c.scroll.y, 60) && Near(c.scroll.x, 0));
    Click(30, 55);
    HX_CHECK(c.deep == 1);
    for (i32 i = 0; i < 4; i++)
        Frame({Wheel(-1)});
    HX_CHECK(Near(c.scroll.y, 140));
    // The matrix scales the GUI: a button at 100 in GUI space is at 200 on the screen.
    Click(110, 105);
    HX_CHECK(c.scaled == 0);
    Click(220, 205);
    HX_CHECK(c.scaled == 1);
}

namespace
{
    struct TextState
    {
        String a = String("hello"), b = String("abc"), p, area, focused;
        String focusName;
    } gText;
}

HX_TEST(ImmediateGui, TextFieldsEditByKeysTheClipboardAndTheFocus)
{
    HX_REQUIRE(RegisterModules());
    gText = TextState();
    engine::SystemClipboard::SetText("");
    GuiScene scene([](StringView) {
        TextState& t = gText;
        Gui::SetNextControlName("first");
        t.a = Gui::TextField(Rect(10, 10, 200, 22), t.a.View());
        Gui::SetNextControlName("second");
        t.b = Gui::TextField(Rect(10, 40, 200, 22), t.b.View(), 5);
        t.p = Gui::PasswordField(Rect(10, 70, 200, 22), t.p.View(), u'*');
        t.area = Gui::TextArea(Rect(10, 100, 200, 60), t.area.View());
        if (!t.focusName.Empty())
        {
            Gui::FocusControl(t.focusName.View());
            t.focusName.Clear();
        }
        t.focused = Gui::GetNameOfFocusedControl();
    });
    TextState& t = gText;
    using K = platform::KeyCode;
    // A click puts the cursor where it is (after the text, here); characters go in, keys edit.
    Click(205, 21);
    HX_CHECK(t.focused == StringView("first") && GuiUtility::keyboardControl() != 0);
    Type("xy");
    HX_CHECK(t.a == StringView("helloxy"));
    Frame({Key(K::Backspace)});
    HX_CHECK(t.a == StringView("hellox"));
    Frame({Key(K::LeftArrow), Key(K::LeftArrow), Key(K::Delete)});
    HX_CHECK(t.a == StringView("hellx"));
    Frame({Key(K::A, platform::ModControl)});
    Type("z");
    HX_CHECK(t.a == StringView("z"));
    Frame({Key(K::Home)});
    Type("q");
    HX_CHECK(t.a == StringView("qz"));
    Frame({Key(K::A, platform::ModControl), Key(K::C, platform::ModControl)});
    HX_CHECK(engine::SystemClipboard::Text() == StringView("qz"));
    // Tab moves the focus to the next field, which takes it with its text selected; a field keeps its length.
    Frame({Key(K::Tab)});
    HX_CHECK(t.focused == StringView("second"));
    Type("123456");
    HX_CHECK(t.b == StringView("12345"));
    Frame({Key(K::V, platform::ModControl)});
    HX_CHECK(t.b == StringView("12345"));
    Frame({Key(K::A, platform::ModControl), Key(K::V, platform::ModControl)});
    HX_CHECK(t.b == StringView("qz"));
    // A password field holds the text; it draws its mask.
    Click(100, 81);
    Type("pw");
    HX_CHECK(t.p == StringView("pw") && t.focused.Empty());
    // An area takes line feeds from the key.
    Click(100, 120);
    Type("a");
    Frame({Key(K::Return)});
    Type("b");
    HX_CHECK(t.area == StringView("a\nb"));
    // Focus by name.
    t.focusName = String("first");
    Frame();
    Frame();
    HX_CHECK(t.focused == StringView("first"));
    engine::SystemClipboard::SetText("");
}

namespace
{
    struct WindowState
    {
        Rect a = Rect(50, 50, 150, 100), b = Rect(120, 60, 150, 100);
        i32 pressA = 0, pressB = 0, pressModal = 0;
        bool modal = false;
        u32 token = 0;
    } gWindows;
}

HX_TEST(ImmediateGui, WindowsTakeTheirClicksComeToTheFrontAndDrag)
{
    HX_REQUIRE(RegisterModules());
    gWindows = WindowState();
    gWindows.token = Gui::windowFunctionCalled().Add([](void*, i32 id) {
        WindowState& w = gWindows;
        bool pressed = Gui::Button(Rect(10, 25, 60, 20), "Go");
        if (id == 1)
        {
            w.pressA += pressed ? 1 : 0;
            Gui::DragWindow(Rect(0, 0, 1000, 20));
        }
        else if (id == 2)
            w.pressB += pressed ? 1 : 0;
        else
            w.pressModal += pressed ? 1 : 0;
    });
    GuiScene scene([](StringView) {
        WindowState& w = gWindows;
        GuiContent one("One"), two("Two");
        w.a = Gui::WindowInternal(1, w.a, &one, nullptr, false);
        w.b = Gui::WindowInternal(2, w.b, &two, nullptr, false);
        if (w.modal)
            Gui::WindowInternal(3, Rect(300, 200, 90, 60), &one, nullptr, true);
    });
    WindowState& w = gWindows;
    // Where they overlap the later one is in front: it takes the click.
    Click(140, 90);
    HX_CHECK(w.pressB == 1 && w.pressA == 0);
    // A click in the one behind brings it to the front.
    Click(70, 85);
    HX_CHECK(w.pressA == 1);
    Click(140, 90);
    HX_CHECK(w.pressB == 1 && w.pressA == 1);
    // A drag of its title moves it.
    Frame({Mouse(platform::EventType::MouseMove, 100, 55), Mouse(platform::EventType::MouseDown, 100, 55)});
    Frame({Mouse(platform::EventType::MouseMove, 130, 75)});
    Frame({Mouse(platform::EventType::MouseUp, 130, 75)});
    Frame();
    HX_CHECK(Near(w.a.x, 80) && Near(w.a.y, 70));
    // A modal window takes all the input.
    w.modal = true;
    Frame();
    HX_CHECK(GuiUtility::hasModalWindow());
    Click(250, 110);
    HX_CHECK(w.pressB == 1);
    Click(330, 235);
    HX_CHECK(w.pressModal == 1);
    w.modal = false;
    Frame();
    HX_CHECK(!GuiUtility::hasModalWindow());
    Gui::windowFunctionCalled().Remove(w.token);
}

namespace
{
    struct LayoutState
    {
        Rect one, two, left, right, row, after, field, window = Rect(320, 10, 10, 10);
        u32 token = 0;
        Vector2 scroll;
        Rect deep;
    } gLayout;
}

HX_TEST(ImmediateGui, GuiLayoutStacksStretchesAndSizesByItsOptions)
{
    HX_REQUIRE(RegisterModules());
    gLayout = LayoutState();
    gLayout.token = Gui::windowFunctionCalled().Add([](void*, i32 id) {
        if (id != 5)
            return;
        GuiLayout::Label("Grow");
        GuiLayout::Button("A wide button in the window");
    });
    GuiScene scene([](StringView) {
        LayoutState& l = gLayout;
        bool repaint = gui::Event::current()->type() == EventType::Repaint;
        GuiLayout::BeginArea(Rect(10, 10, 300, 280));
        GuiLayout::Button("One");
        Rect one = GuiLayoutUtility::GetLastRect();
        GuiLayout::Button("Two");
        Rect two = GuiLayoutUtility::GetLastRect();
        GuiLayout::BeginHorizontal();
        GuiLayout::Button("L");
        Rect left = GuiLayoutUtility::GetLastRect();
        GuiLayout::FlexibleSpace();
        GuiLayoutOption* width = GuiLayout::Width(50);
        GuiLayout::Button("R", Span<GuiLayoutOption* const>(&width, 1));
        delete width;
        Rect right = GuiLayoutUtility::GetLastRect();
        GuiLayout::EndHorizontal();
        Rect row = GuiLayoutUtility::GetLastRect();
        GuiLayout::Space(30);
        GuiLayout::Label("After");
        Rect after = GuiLayoutUtility::GetLastRect();
        GuiLayoutOption* height = GuiLayout::Height(60);
        l.scroll = GuiLayout::BeginScrollView(l.scroll, Span<GuiLayoutOption* const>(&height, 1));
        delete height;
        for (i32 i = 0; i < 8; i++)
            GuiLayout::Button("Row");
        Rect deep = GuiLayoutUtility::GetLastRect();
        GuiLayout::EndScrollView();
        GuiLayout::EndArea();
        l.window = GuiLayout::WindowInternal(5, l.window, &GuiContent::Temp("Window"), nullptr);
        if (repaint)
            l.one = one, l.two = two, l.left = left, l.right = right, l.row = row, l.after = after, l.deep = deep;
    });
    Frame();
    LayoutState& l = gLayout;
    // Stacked down the area (in its space), as wide as it less their margins; the margins between them collapse.
    HX_CHECK(Near(l.one.x, 4) && Near(l.one.width, 292) && l.one.height > 4);
    HX_CHECK(Near(l.two.y, l.one.y + l.one.height + 2));
    // A flexible space takes the room left in its row: the buttons keep their sizes, the fixed one at the end.
    HX_CHECK(l.left.width < 60 && Near(l.right.width, 50) && Near(l.right.x + l.right.width, 296));
    HX_CHECK(l.row.y >= l.two.y + l.two.height);
    // A space keeps its pixels between entries.
    HX_CHECK(l.after.y >= l.row.y + l.row.height + 30);
    // The scroll view's content is in its own space, taller than the view.
    HX_CHECK(l.deep.y > 60);
    // A layout window grows to what its function lays out.
    HX_CHECK(l.window.width > 100 && l.window.height > 40);
    Gui::windowFunctionCalled().Remove(l.token);
}

namespace
{
    String gOrder;
    i32 gFrontPressed = 0, gBackPressed = 0;
}

HX_TEST(ImmediateGui, DepthOrdersTheScriptsAndInputStopsWhereItIsUsed)
{
    HX_REQUIRE(RegisterModules());
    gOrder.Clear();
    gFrontPressed = gBackPressed = 0;
    GuiScene scene(
        [](StringView name) {
            bool front = name == StringView("Front");
            Gui::SetDepth(front ? -1 : 1);
            if (gui::Event::current()->type() == EventType::Repaint)
                gOrder += name;
            if (Gui::Button(Rect(10, 10, 80, 20), name))
                (front ? gFrontPressed : gBackPressed)++;
        },
        {"Back", "Front"});
    gOrder.Clear();
    Frame();
    // Repaint draws the deepest first; input goes to the front one, which uses it.
    HX_CHECK(gOrder == StringView("BackFront"));
    Click(50, 20);
    HX_CHECK(gFrontPressed == 1 && gBackPressed == 0);
}

namespace
{
    struct MoreState
    {
        i32 fit = 0, off = 0;
        Rect fitCells[3];
        bool towards = false;
        Vector2 scroll;
        Rect aligned;
        i32 alignedWidth = 0, alignedHeight = 0;
    } gMore;
}

HX_TEST(ImmediateGui, ToolbarsFitTheirContentsCellsTurnOffAndScrollViewsMoveToward)
{
    HX_REQUIRE(RegisterModules());
    gMore = MoreState();
    GuiScene scene([](StringView) {
        MoreState& m = gMore;
        const String texts[3] = {String("a"), String("a much longer one"), String("b")};
        m.fit = Gui::Toolbar(Rect(10, 100, 300, 20), m.fit, Span<const String>(texts, 3), nullptr, Gui::ToolbarButtonSize::FitToContents);
        const bool enabled[3] = {true, false, true};
        GuiContent a("a"), b("b"), c("c");
        GuiContent* contents[3] = {&a, &b, &c};
        m.off = GuiLayout::Toolbar(m.off, Span<GuiContent* const>(contents, 3), Span<const bool>(enabled, 3), nullptr, Gui::ToolbarButtonSize::Fixed);
        Rect last = GuiLayoutUtility::GetLastRect();
        (void)last;
        m.scroll = Gui::BeginScrollView(Rect(10, 200, 100, 50), m.scroll, Rect(0, 0, 80, 400));
        if (m.towards)
            Gui::ScrollTowards(Rect(0, 300, 10, 10), 25);
        Gui::EndScrollView();
        Gui::SetMatrix(Matrix4x4::Scale(Vector3(1.5f, 1.5f, 1)));
        m.aligned = GuiUtility::AlignRectToDevice(Rect(10.2f, 10.2f, 20.1f, 10.1f), m.alignedWidth, m.alignedHeight);
        Gui::SetMatrix(Matrix4x4::identity);
    });
    MoreState& m = gMore;
    // Fit to contents: the long one's cell takes most of the width (a click at two thirds of it hits it).
    Click(10 + 300 * 0.66f, 110);
    HX_CHECK(m.fit == 1);
    // A cell turned off takes no click; the others do (the toolbar stretches across the GUI: three cells of 392).
    Click(4 + 392 * 0.5f, 8);
    HX_CHECK(m.off == 0);
    Click(4 + 392 * 0.85f, 8);
    HX_CHECK(m.off == 2);
    // ScrollTowards moves 25 pixels a pass toward the rect (two passes a frame: Layout and Repaint).
    m.towards = true;
    Frame();
    HX_CHECK(m.scroll.y > 0 && m.scroll.y <= 50.01f);
    for (i32 i = 0; i < 20; i++)
        Frame();
    HX_CHECK(Near(m.scroll.y, 260, 1.0f));
    // A rect through a scale of 1.5 lands on whole pixels: 15.3 to 45.45 becomes 15 to 45.
    HX_CHECK(m.alignedWidth == 30 && m.alignedHeight == 15 && Near(m.aligned.x * 1.5f, 15, 0.01f));
    // The frame's events are queued for PopEvent.
    Frame({Key(platform::KeyCode::A), Key(platform::KeyCode::B)});
    gui::Event popped;
    HX_CHECK(gui::Event::GetEventCount() == 2 && gui::Event::PopEvent(&popped) && popped.keyCode() == platform::KeyCode::A);
    HX_CHECK(gui::Event::PopEvent(&popped) && popped.keyCode() == platform::KeyCode::B && !gui::Event::PopEvent(&popped));
}

HX_TEST(ImmediateGui, TheTextEditorMovesByWordsParagraphsLinesAndFindsLinks)
{
    HX_REQUIRE(RegisterModules());
    GuiScene scene([](StringView) {});
    TextEditor t;
    t.SetText("one two\nthree four");
    t.SetCursorIndex(0);
    t.SetSelectIndex(0);
    t.MoveToStartOfNextWord();
    HX_CHECK(t.cursorIndex() == 4);
    t.MoveParagraphForward();
    HX_CHECK(t.cursorIndex() == 7);
    t.MoveParagraphForward();
    HX_CHECK(t.cursorIndex() == 18);
    t.MoveParagraphBackward();
    HX_CHECK(t.cursorIndex() == 8);
    t.MoveToEndOfPreviousWord();
    HX_CHECK(t.cursorIndex() == 7);
    HX_CHECK(t.FindStartOfNextWord(0) == 4 && t.FindStartOfNextWord(5) == 8);
    // Snapping by words while the mouse drags; the alt cursor moves the selection.
    t.SetCursorIndex(0);
    t.SetSelectIndex(3);
    t.SetAltCursorPosition(8);
    t.MoveSelectionToAltCursor();
    HX_CHECK(t.text() == StringView(" two\nonethree four") && t.SelectedText() == StringView("one"));
    t.DblClickSnap(TextEditor::DblClickSnapping::PARAGRAPHS);
    HX_CHECK(t.doubleClickSnapping() == TextEditor::DblClickSnapping::PARAGRAPHS);
    // Links under a point: <link="..."> and <a href="..."> in the text of a styled field.
    GuiStyle style;
    t.SetStyle(&style);
    t.SetPosition(Rect(0, 0, 400, 40));
    t.SetText("x <link=\"id7\">linked</link> <a href=\"https://example.org\">site</a>");
    String found;
    HX_CHECK(t.HasClickedOnLink(Vector2(-100, -100), found) == false);
    GuiContent before("x linke"), beforeSite("x linked si");
    f32 x = style.CalcSize(&before).x;
    f32 y = style.lineHeight() * 0.5f;
    HX_CHECK(t.HasClickedOnLink(Vector2(x, y), found) && found == StringView("id7"));
    f32 x2 = style.CalcSize(&beforeSite).x;
    HX_CHECK(t.HasClickedOnHREF(Vector2(x2, y), found) && found == StringView("https://example.org"));
    HX_CHECK(!t.HasClickedOnLink(Vector2(x2, y), found));
}

namespace
{
    i32 gGuiPresses = 0, gPanelClicks = 0;
}

HX_TEST(ImmediateGui, WhatTheGuiUsesThePanelsUnderItDoNotGet)
{
    HX_REQUIRE(RegisterModules());
    gGuiPresses = gPanelClicks = 0;
    GuiScene scene([](StringView) {
        if (Gui::Button(Rect(0, 0, 100, 40), "Gui"))
            gGuiPresses++;
    });
    scene.folder.Write("under.hxml",
                       R"(<HXML><Button name="under" text="Panel" style="position: absolute; left: 0px; top: 0px; width: 300px; height: 200px;"/></HXML>)");
    scene.folder.Open();
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    view->SetSource(LoadAt<UITemplate>("under.hxml"));
    view->Update();
    auto* under = static_cast<Button*>(view->rootElement()->FindNative("under"));
    HX_REQUIRE(under != nullptr);
    under->clickedNative().Add([](void*) { gPanelClicks++; });
    // A click on the GUI's button: the GUI uses it, the panel's button under it does not see it.
    Click(50, 20);
    view->Update();
    HX_CHECK(gGuiPresses == 1 && gPanelClicks == 0);
    // Beside the GUI's button the panel has it.
    Click(200, 150);
    view->Update();
    HX_CHECK(gGuiPresses == 1 && gPanelClicks == 1);
    DestroyImmediate(ui);
}

HX_TEST(ImmediateGui, CppScriptsDrawAndClickThroughTheScriptApi)
{
    HX_REQUIRE(RegisterModules());
    AssetFolder folder;
    ReportScreen(400, 300);
    system::ResetForTests();
    engine::InputFeed::Reset();
    CppNotes().Clear();
    GameObject* host = GameObject::Create("CppGui");
    host->AddComponent(scripting::TypeOf<helix::ui::testing::CppGuiScript>());
    RunScriptPhase(ScriptMessage::Update);
    Frame();
    Click(50, 20);
    Click(50, 60);
    Click(240, 65);
    for (const String& n : CppNotes())
        std::printf("    %s\n", n.CStr());
    HX_CHECK(CppNotes().Size() == 3 && CppNotes()[0] == StringView("cpp-gui-button") && CppNotes()[1] == StringView("cpp-layout-button") &&
             CppNotes()[2] == StringView("cpp-window-button"));
    DestroyImmediate(host);
    system::ResetForTests();
    engine::InputFeed::Reset();
}

namespace
{
    // The numbers of a note's fields after its name ("name:x:y:w:h").
    Rect NoteRect(StringView prefix)
    {
        for (const String& n : Notes())
            if (n.View().StartsWith(prefix))
            {
                f32 v[4] = {0, 0, 0, 0};
                String rest(n.View().Substr(prefix.Size()));
                std::sscanf(rest.CStr(), "%f:%f:%f:%f", &v[0], &v[1], &v[2], &v[3]);
                return Rect(v[0], v[1], v[2], v[3]);
            }
        return Rect(-100, -100, 0, 0);
    }
}

HX_TEST(UIScripts, CSharpOnGuiDrawsLaysOutAndTakesInput)
{
    HX_REQUIRE(StartScripts());
    AssetFolder folder;
    ReportScreen(400, 300);
    system::ResetForTests();
    engine::InputFeed::Reset();
    Notes().Clear();
    GameObject* host = GameObject::Create("CsGui");
    host->AddComponent(helix::ui::testing::CSharp("CsGuiTests").type);
    RunScriptPhase(ScriptMessage::Update);
    Frame();
    Click(50, 20);
    Click(50, 50);
    Rect a = NoteRect("cs-rect-a:"), field = NoteRect("cs-rect-field:");
    Click(a.x + 10, a.y + a.height * 0.5f);
    Click(240, 50);
    Click(field.x + field.width - 4, field.y + field.height * 0.5f);
    Type("c");
    for (const String& n : Notes())
        std::printf("    %s\n", n.CStr());
    bool painted = false, button = false, toggle = false, layout = false, window = false, text = false;
    for (const String& n : Notes())
    {
        painted = painted || n == StringView("cs-repaint:button:button:True");
        button = button || n == StringView("cs-button");
        toggle = toggle || n == StringView("cs-toggle:True");
        layout = layout || n == StringView("cs-layout-a");
        window = window || n == StringView("cs-window-button");
        text = text || n == StringView("cs-text:abc");
    }
    HX_CHECK(painted && button && toggle && layout && window && text);
    DestroyImmediate(host);
    system::ResetForTests();
    engine::InputFeed::Reset();
}

namespace
{
    u32 gGoldenToken = 0;
}

HX_TEST(ImmediateGui, TheControlsDrawTheSameOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 330, height = 160;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        Array<u8> font;
        folder.Folder("Fonts");
        folder.Write("Fonts/Page.ttf", PageFont(font));
        folder.Open();
        FontAsset* page = LoadAt<FontAsset>("Fonts/Page.ttf");
        HX_REQUIRE(page != nullptr);
        GuiSkin::Default()->SetFont(page);
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = width, .height = height, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth = device.CreateTexture({.format = rhi::Format::D32Float, .width = width, .height = height, .usage = rhi::TextureDepthStencil});
        Graphics::SetDevice(&device);
        Graphics::SetTarget({color, depth, width, height, rhi::Format::RGBA8Unorm, rhi::Format::D32Float});
        ReportScreen(i32(width), i32(height));
        auto* asset = ScriptableObject::CreateInstanceByName("ClearPipelineAsset");
        HX_REQUIRE(asset != nullptr);
        GraphicsSettings::SetDefaultRenderPipeline(asset);
        GameObject* cameraObject = GameObject::Create("Camera");
        Camera* camera = cameraObject->AddComponent<Camera>();
        camera->SetClearFlags(CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0.3f, 0.32f, 0.36f, 1));
        gGoldenToken = Gui::windowFunctionCalled().Add([](void*, i32) {
            Gui::Label(Rect(8, 24, 84, 18), "Inside");
            Gui::Button(Rect(8, 46, 84, 22), "OK");
        });
        GuiHook() = [](StringView) {
            Gui::Box(Rect(8, 8, 184, 144), "Box");
            Gui::Label(Rect(16, 28, 100, 18), "Label");
            Gui::Button(Rect(16, 50, 80, 22), "Button");
            Gui::Toggle(Rect(104, 50, 80, 18), true, "On");
            Gui::Toggle(Rect(104, 72, 80, 18), false, "Off");
            Gui::HorizontalSlider(Rect(16, 80, 80, 16), 30, 0, 100);
            Gui::TextField(Rect(16, 100, 168, 22), "Field");
            const String tabs[3] = {String("One"), String("Two"), String("Three")};
            Gui::Toolbar(Rect(16, 128, 168, 18), 1, Span<const String>(tabs, 3));
            Gui::VerticalScrollbar(Rect(200, 8, 12, 144), 20, 30, 0, 100);
            GuiContent title("Window");
            Gui::WindowInternal(9, Rect(222, 8, 100, 80), &title, nullptr, false);
        };
        system::ResetForTests();
        GameObject* host = GameObject::Create("Gui");
        host->AddComponent(scripting::TypeOf<helix::ui::testing::CppGuiHook>());
        Graphics::RenderFrame();
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(engine::testing::MatchGolden("ImmediateGuiControls", pixels.Data(), width, height, 8));
        GuiHook() = nullptr;
        Gui::windowFunctionCalled().Remove(gGoldenToken);
        GuiSkin::Default()->SetFont(nullptr);
        DestroyImmediate(host);
        DestroyImmediate(cameraObject);
        system::ResetForTests();
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}
