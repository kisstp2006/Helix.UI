#pragma once

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/UI/Engine/Gui.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

// The immediate GUI's state inside the module: the pass and its event, the control ids, the clips and groups, the
// windows, the layout caches and the frame's mesh.
namespace helix::ui::gui::detail
{
    // The ids of a list of calls (a script's OnGui or a window's function): the same calls in the same order get the
    // same ids in every pass and frame.
    struct IdList
    {
        Array<i32> hints, ids;
        u32 at = 0;
    };

    // A group or clip: the part of the GUI's space seen through it and where its local origin is (GUI space: the
    // space before the matrix).
    struct Clip
    {
        Rect visible;
        Vector2 origin;
    };

    struct LayoutCache;

    // What a window keeps between calls and frames.
    struct Window
    {
        u64 owner = 0;
        i32 id = 0;
        Rect rect, returned;
        bool hasReturned = false, modal = false, layout = false;
        // Called in a pass (it takes input and is drawn), and in this frame's Repaint (else it goes).
        bool seen = false, called = false;
        // Its place among the windows (larger: in front).
        u64 order = 0;
        GuiContent title;
        GuiStyle style;
        // The GUI's state at its call, which its function starts with.
        engine::PPtr<GuiSkin> skin;
        Color color, backgroundColor, contentColor;
        bool enabled = true;
        Matrix4x4 matrix = Matrix4x4::identity;
        IdList ids;
        LayoutCache* cache = nullptr;
        Array<GuiLayoutOption*> options;
        // DragWindow's start: the mouse's and the window's place.
        Vector2 dragMouse, dragFrom;
        ~Window();
    };

    // A script with OnGui: its depth, its ids and its layout.
    struct Owner
    {
        u64 key = 0;
        i32 depth = 0;
        IdList ids;
        LayoutCache* cache = nullptr;
        bool seen = false;
        ~Owner();
    };

    struct ScrollState
    {
        Vector2 scroll;
        bool apply = false;
        Rect position, visible, view;
        bool horizontal = false, vertical = false;
    };

    // A scroll view between its Begin and End.
    struct OpenScroll
    {
        i32 id = 0;
        Rect position, clip, view;
    };

    struct Named
    {
        String name;
        i32 id = 0;
        Rect rect;
    };

    struct State
    {
        // Gui's state of the pass.
        engine::PPtr<GuiSkin> skin;
        Color color = Color(1, 1, 1, 1), backgroundColor = Color(1, 1, 1, 1), contentColor = Color(1, 1, 1, 1);
        bool changed = false, enabled = true;
        i32 depth = 0;
        Matrix4x4 matrix = Matrix4x4::identity;
        Affine toScreen;
        String tooltip, nextTooltip;
        // GuiUtility's.
        i32 hotControl = 0, keyboardControl = 0, nextId = 1;
        // The pass: its event (the scripts' Event.current), the raw one it came from, the mouse in GUI space.
        Event* current = nullptr;
        Event event, raw;
        bool inPass = false;
        Vector2 mouse;
        Array<Clip> clips;
        Array<OpenScroll> scrolls;
        HashMap<i32, ScrollState> scrollStates;
        IdList* ids = nullptr;
        Owner* owner = nullptr;
        Window* window = nullptr;
        // The window a mouse event goes to (null: the GUI outside the windows), the modal window.
        Window* target = nullptr;
        bool targetChosen = false;
        // Names and keyboard controls of the pass (the focus moves through them with tab).
        String nextControlName, focusName;
        bool focusByName = false;
        Array<Named> named, keyboardControls;
        // Text editing: the editor of the keyboard control.
        TextEditor* editor = nullptr;
        // Windows and owners by their keys.
        Array<Window*> windows;
        Array<Owner*> owners;
        u64 windowOrder = 0;
        // Clicks: the last down's time, place and count.
        f64 lastClickTime = -1;
        Vector2 lastClickAt;
        i32 clickCount = 0;
        i32 heldButtons = 0;
        Vector2 screenMouse;
        // The frame's drawing.
        UIMesh mesh;
        TextGenerator generator;
        Array<TextFont*> fallbacks;
        bool drawerSet = false;
        // Warnings said once.
        bool layoutMismatchSaid = false;
        // The frame's events as the GUI made them (Event.PopEvent takes them in order).
        Array<Event*> queue;
        u32 popped = 0;
    };

    State& S();
    i32 Hint(StringView name);

    // ------------------------------------------------------------------------------------------------- the pass
    EventType Type();
    Event& Current();
    bool Repainting();
    bool Layouting();
    // The mouse in the current clip's space (far away when it is outside the clip).
    Vector2 Mouse();
    bool Contains(const Rect& r, Vector2 p);
    // Whether the pointer is over the rect for hovering (inside the clip, no other control held).
    bool Hover(const Rect& r, i32 id);
    void PushClip(const Rect& local, Vector2 scroll);
    void PopClip();
    const Clip& TopClip();
    // A point or rect of the current clip in GUI space (before the matrix).
    Vector2 ToGuiSpace(Vector2 local);
    void UpdateMouse();
    i32 ControlId(i32 hint, FocusType focus, const Rect& rect);
    GuiSkin& Skin();

    // ------------------------------------------------------------------------------------------------- drawing
    // Colors with the GUI's tints (enabled off halves the alpha).
    Color BackgroundTint();
    Color ContentTint(Color text);
    // An image over a rect, 9-sliced by the border (texture pixels).
    void DrawImage(const Rect& rect, engine::Texture2D* texture, const RectOffset* border, Color tint);
    // A texture over a rect, scaled by the mode, with uvs (0 to 1, v up).
    void DrawTextureRect(const Rect& rect, engine::Texture2D* texture, const Rect& uv, Color tint);
    void DrawRectangle(const Rect& rect, Color color);
    // Text laid out in a rect by a style (the generator's settings from it).
    TextGenerationSettings TextSettings(const GuiStyle& style, f32 width, f32 height, Color color);
    void DrawText(const TextInfo& info, Vector2 at, f32 opacity = 1);
    TextFont* FontOf(const GuiStyle& style);

    // ------------------------------------------------------------------------------------------------- windows
    Window* FindWindow(u64 owner, i32 id);
    // The windows of the owner after its OnGui returned: drawn back to front, events front to back.
    void DoWindows(Owner& owner);
    // Before an input event: the window it goes to.
    void ChooseTarget(Vector2 screenMouse);

    // ------------------------------------------------------------------------------------------------- layout
    LayoutCache* NewLayoutCache();
    void DeleteLayoutCache(LayoutCache* c);
    // The pass starts in a cache (its top group the area given); Layout records, the other passes read.
    void BeginLayout(LayoutCache* cache, const Rect& area, const GuiStyle* style);
    void EndLayout(LayoutCache* cache);
    // After a Layout pass: the rects laid out; the size the top group wants.
    Vector2 LayoutCacheMinSize(LayoutCache* cache);
    void ResetLayoutForTests();

    // ------------------------------------------------------------------------------------------------- text
    // Text indices are the reference's (UTF-16 units) over UTF-8 strings.
    i32 Utf16Length(StringView text);
    u32 ByteOfUnit(StringView text, i32 unit);
    i32 UnitOfByte(StringView text, u32 byte);
    void AppendUtf8(String& out, char32_t c);
    // The text control's editor (made with its focus) and its keys.
    TextEditor* EditorFor(i32 id);
    void ClearEditor();
}
