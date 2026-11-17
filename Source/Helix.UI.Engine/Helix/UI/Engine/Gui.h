#pragma once

#include <Helix/Engine/Event/Event.h>
#include <Helix/Math/Matrix4x4.h>
#include <Helix/UI/Engine/Controls.h>
#include <Helix/UI/Engine/GuiTypes.h>
#include <Helix/UI/Rendering/UIMesh.h>

namespace helix::engine
{
    class CommandBuffer;
}

// The immediate GUI (namespace HelixEngine): Gui's controls in rectangles, GuiLayout's in groups laid out by the Layout
// pass, GuiUtility's control ids and focus, TextEditor. Called from OnGui of C# and C++ scripts; what they draw goes
// over the cameras each frame.
namespace helix::ui::gui
{
    // A layout option of GuiLayout's controls (GuiLayout.Width and the others make them).
    HX_CLASS(Root, Owned, Namespace = "HelixEngine", Sealed)
    class GuiLayoutOption : public GuiObjectBase
    {
    public:
        enum class Kind : i32
        {
            FixedWidth,
            FixedHeight,
            MinWidth,
            MaxWidth,
            MinHeight,
            MaxHeight,
            StretchWidth,
            StretchHeight,
        };
        GuiLayoutOption(Kind kind, f32 value) : kind(kind), value(value) {}
        scripting::TypeId HelixType() const override;
        Kind kind;
        f32 value;
    };

    using Options = Span<GuiLayoutOption* const>;

    HX_CLASS(Namespace = "HelixEngine", Static)
    class Gui
    {
    public:
        // How a toolbar's buttons share its width: equally, or by their contents.
        HX_ENUM()
        enum class ToolbarButtonSize : i32
        {
            Fixed = 0,
            FitToContents = 1,
        };

        // --------------------------------------------------------------------------------------------- state
        HX_PROPERTY() static GuiSkin* skin();
        static void SetSkin(GuiSkin* value);
        HX_PROPERTY() static Color color();
        static void SetColor(Color value);
        HX_PROPERTY() static Color backgroundColor();
        static void SetBackgroundColor(Color value);
        HX_PROPERTY() static Color contentColor();
        static void SetContentColor(Color value);
        HX_PROPERTY() static bool changed();
        static void SetChanged(bool value);
        HX_PROPERTY() static bool enabled();
        static void SetEnabled(bool value);
        HX_PROPERTY() static i32 depth();
        static void SetDepth(i32 value);
        HX_PROPERTY() static Matrix4x4 matrix();
        static void SetMatrix(Matrix4x4 value);
        HX_PROPERTY() static String tooltip();
        static void SetTooltip(StringView value);

        // --------------------------------------------------------------------------------------------- controls
        HX_FUNCTION() static void Label(Rect position, StringView text);
        HX_FUNCTION() static void Label(Rect position, engine::Texture2D* image);
        HX_FUNCTION() static void Label(Rect position, GuiContent* content);
        HX_FUNCTION() static void Label(Rect position, StringView text, GuiStyle* style);
        HX_FUNCTION() static void Label(Rect position, engine::Texture2D* image, GuiStyle* style);
        HX_FUNCTION() static void Label(Rect position, GuiContent* content, GuiStyle* style);
        HX_FUNCTION() static void Box(Rect position, StringView text);
        HX_FUNCTION() static void Box(Rect position, engine::Texture2D* image);
        HX_FUNCTION() static void Box(Rect position, GuiContent* content);
        HX_FUNCTION() static void Box(Rect position, StringView text, GuiStyle* style);
        HX_FUNCTION() static void Box(Rect position, engine::Texture2D* image, GuiStyle* style);
        HX_FUNCTION() static void Box(Rect position, GuiContent* content, GuiStyle* style);
        HX_FUNCTION() static bool Button(Rect position, StringView text);
        HX_FUNCTION() static bool Button(Rect position, engine::Texture2D* image);
        HX_FUNCTION() static bool Button(Rect position, GuiContent* content);
        HX_FUNCTION() static bool Button(Rect position, StringView text, GuiStyle* style);
        HX_FUNCTION() static bool Button(Rect position, engine::Texture2D* image, GuiStyle* style);
        HX_FUNCTION() static bool Button(Rect position, GuiContent* content, GuiStyle* style);
        HX_FUNCTION() static bool RepeatButton(Rect position, StringView text);
        HX_FUNCTION() static bool RepeatButton(Rect position, engine::Texture2D* image);
        HX_FUNCTION() static bool RepeatButton(Rect position, GuiContent* content);
        HX_FUNCTION() static bool RepeatButton(Rect position, StringView text, GuiStyle* style);
        HX_FUNCTION() static bool RepeatButton(Rect position, engine::Texture2D* image, GuiStyle* style);
        HX_FUNCTION() static bool RepeatButton(Rect position, GuiContent* content, GuiStyle* style);
        HX_FUNCTION() static bool Toggle(Rect position, bool value, StringView text);
        HX_FUNCTION() static bool Toggle(Rect position, bool value, engine::Texture2D* image);
        HX_FUNCTION() static bool Toggle(Rect position, bool value, GuiContent* content);
        HX_FUNCTION() static bool Toggle(Rect position, bool value, StringView text, GuiStyle* style);
        HX_FUNCTION() static bool Toggle(Rect position, bool value, engine::Texture2D* image, GuiStyle* style);
        HX_FUNCTION() static bool Toggle(Rect position, bool value, GuiContent* content, GuiStyle* style);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<const String> texts);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<engine::Texture2D* const> images);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<GuiContent* const> contents);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<const String> texts, GuiStyle* style);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<GuiContent* const> contents, GuiStyle* style);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<engine::Texture2D* const> images, GuiStyle* style);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<const String> texts, GuiStyle* style, ToolbarButtonSize buttonSize);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<engine::Texture2D* const> images, GuiStyle* style, ToolbarButtonSize buttonSize);
        HX_FUNCTION() static i32 Toolbar(Rect position, i32 selected, Span<GuiContent* const> contents, GuiStyle* style, ToolbarButtonSize buttonSize);
        HX_FUNCTION() static i32 SelectionGrid(Rect position, i32 selected, Span<const String> texts, i32 xCount);
        HX_FUNCTION() static i32 SelectionGrid(Rect position, i32 selected, Span<engine::Texture2D* const> images, i32 xCount);
        HX_FUNCTION() static i32 SelectionGrid(Rect position, i32 selected, Span<GuiContent* const> contents, i32 xCount);
        HX_FUNCTION() static i32 SelectionGrid(Rect position, i32 selected, Span<const String> texts, i32 xCount, GuiStyle* style);
        HX_FUNCTION() static i32 SelectionGrid(Rect position, i32 selected, Span<GuiContent* const> contents, i32 xCount, GuiStyle* style);
        HX_FUNCTION() static String TextField(Rect position, StringView text);
        HX_FUNCTION() static String TextField(Rect position, StringView text, i32 maxLength);
        HX_FUNCTION() static String TextField(Rect position, StringView text, GuiStyle* style);
        HX_FUNCTION() static String TextField(Rect position, StringView text, i32 maxLength, GuiStyle* style);
        HX_FUNCTION() static String PasswordField(Rect position, StringView password, char16_t maskChar);
        HX_FUNCTION() static String PasswordField(Rect position, StringView password, char16_t maskChar, i32 maxLength);
        HX_FUNCTION() static String PasswordField(Rect position, StringView password, char16_t maskChar, GuiStyle* style);
        HX_FUNCTION() static String PasswordField(Rect position, StringView password, char16_t maskChar, i32 maxLength, GuiStyle* style);
        HX_FUNCTION() static String TextArea(Rect position, StringView text);
        HX_FUNCTION() static String TextArea(Rect position, StringView text, i32 maxLength);
        HX_FUNCTION() static String TextArea(Rect position, StringView text, GuiStyle* style);
        HX_FUNCTION() static String TextArea(Rect position, StringView text, i32 maxLength, GuiStyle* style);
        HX_FUNCTION() static f32 HorizontalSlider(Rect position, f32 value, f32 leftValue, f32 rightValue);
        HX_FUNCTION() static f32 HorizontalSlider(Rect position, f32 value, f32 leftValue, f32 rightValue, GuiStyle* slider, GuiStyle* thumb);
        HX_FUNCTION() static f32 VerticalSlider(Rect position, f32 value, f32 topValue, f32 bottomValue);
        HX_FUNCTION() static f32 VerticalSlider(Rect position, f32 value, f32 topValue, f32 bottomValue, GuiStyle* slider, GuiStyle* thumb);
        HX_FUNCTION() static f32 Slider(Rect position, f32 value, f32 size, f32 start, f32 end, GuiStyle* slider, GuiStyle* thumb, bool horiz, i32 id);
        HX_FUNCTION() static f32 HorizontalScrollbar(Rect position, f32 value, f32 size, f32 leftValue, f32 rightValue);
        HX_FUNCTION() static f32 HorizontalScrollbar(Rect position, f32 value, f32 size, f32 leftValue, f32 rightValue, GuiStyle* style);
        HX_FUNCTION() static f32 VerticalScrollbar(Rect position, f32 value, f32 size, f32 topValue, f32 bottomValue);
        HX_FUNCTION() static f32 VerticalScrollbar(Rect position, f32 value, f32 size, f32 topValue, f32 bottomValue, GuiStyle* style);
        HX_FUNCTION() static void DrawTexture(Rect position, engine::Texture2D* image);
        HX_FUNCTION() static void DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode);
        HX_FUNCTION() static void DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode, bool alphaBlend);
        HX_FUNCTION() static void DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode, bool alphaBlend, f32 imageAspect);
        HX_FUNCTION() static void DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode, bool alphaBlend, f32 imageAspect, Color color,
                                              f32 borderWidth, f32 borderRadius);
        HX_FUNCTION() static void DrawTextureWithTexCoords(Rect position, engine::Texture2D* image, Rect texCoords);
        HX_FUNCTION() static void DrawTextureWithTexCoords(Rect position, engine::Texture2D* image, Rect texCoords, bool alphaBlend);

        // --------------------------------------------------------------------------------------------- groups
        HX_FUNCTION() static void BeginGroup(Rect position);
        HX_FUNCTION() static void BeginGroup(Rect position, StringView text);
        HX_FUNCTION() static void BeginGroup(Rect position, GuiContent* content, GuiStyle* style);
        HX_FUNCTION() static void BeginGroup(Rect position, GuiStyle* style);
        HX_FUNCTION() static void EndGroup();
        HX_FUNCTION() static void BeginClip(Rect position);
        HX_FUNCTION() static void BeginClip(Rect position, Vector2 scrollOffset, Vector2 renderOffset, bool resetOffset);
        HX_FUNCTION() static void EndClip();
        HX_FUNCTION() static Vector2 BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect);
        HX_FUNCTION() static Vector2 BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect, bool alwaysShowHorizontal, bool alwaysShowVertical);
        HX_FUNCTION() static Vector2 BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect, GuiStyle* horizontalScrollbar,
                                                     GuiStyle* verticalScrollbar);
        HX_FUNCTION() static Vector2 BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect, bool alwaysShowHorizontal, bool alwaysShowVertical,
                                                     GuiStyle* horizontalScrollbar, GuiStyle* verticalScrollbar);
        HX_FUNCTION() static void EndScrollView();
        HX_FUNCTION() static void EndScrollView(bool handleScrollWheel);
        HX_FUNCTION() static void ScrollTo(Rect position);
        // Toward showing the rect, at most maxDelta pixels a call; true while it still moves.
        HX_FUNCTION() static bool ScrollTowards(Rect position, f32 maxDelta);

        // --------------------------------------------------------------------------------------------- windows
        // A window: its contents come from the script's window function, called later in the pass with its id
        // (windowFunctionCalled; C# takes a WindowFunction delegate written by hand over it).
        HX_FUNCTION(Internal) static Rect WindowInternal(i32 id, Rect clientRect, GuiContent* title, GuiStyle* style, bool modal);
        HX_EVENT(Internal) static engine::Event<i32>& windowFunctionCalled();
        HX_FUNCTION() static void DragWindow();
        HX_FUNCTION() static void DragWindow(Rect position);
        HX_FUNCTION() static void BringWindowToFront(i32 windowID);
        HX_FUNCTION() static void BringWindowToBack(i32 windowID);
        HX_FUNCTION() static void FocusWindow(i32 windowID);
        HX_FUNCTION() static void UnfocusWindow();

        // --------------------------------------------------------------------------------------------- focus
        HX_FUNCTION() static void SetNextControlName(StringView name);
        HX_FUNCTION() static String GetNameOfFocusedControl();
        HX_FUNCTION() static void FocusControl(StringView name);

        // The calls' own content and style picking (for GuiLayout).
        static bool ButtonContent(Rect position, const GuiContent& content, const GuiStyle& style, bool repeat);
        static bool ToggleContent(Rect position, bool value, const GuiContent& content, const GuiStyle& style);
        static String TextContent(Rect position, StringView text, i32 maxLength, const GuiStyle& style, bool multiline, char16_t mask);
        static i32 GridContent(Rect position, i32 selected, Span<GuiContent* const> contents, i32 xCount, const GuiStyle* style, bool toolbar,
                               ToolbarButtonSize buttonSize = ToolbarButtonSize::Fixed, Span<const bool> enabled = {});
        static void LabelContent(Rect position, const GuiContent& content, const GuiStyle& style);
        static void BoxContent(Rect position, const GuiContent& content, const GuiStyle& style);
    };

    HX_CLASS(Namespace = "HelixEngine", Static)
    class GuiLayout
    {
    public:
        HX_FUNCTION(Params = "options") static void Label(StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Label(engine::Texture2D* image, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Label(GuiContent* content, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Label(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Label(GuiContent* content, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Box(StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Box(GuiContent* content, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Box(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void Box(GuiContent* content, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Button(StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Button(engine::Texture2D* image, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Button(GuiContent* content, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Button(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Button(GuiContent* content, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool RepeatButton(StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool RepeatButton(GuiContent* content, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool RepeatButton(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Toggle(bool value, StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Toggle(bool value, GuiContent* content, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static bool Toggle(bool value, StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<const String> texts, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<GuiContent* const> contents, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<engine::Texture2D* const> images, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<const String> texts, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<GuiContent* const> contents, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<const String> texts, GuiStyle* style, Gui::ToolbarButtonSize buttonSize,
                                                           Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<GuiContent* const> contents, GuiStyle* style, Gui::ToolbarButtonSize buttonSize,
                                                           Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<const String> texts, Span<const bool> enabled, GuiStyle* style,
                                                           Gui::ToolbarButtonSize buttonSize, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 Toolbar(i32 selected, Span<GuiContent* const> contents, Span<const bool> enabled, GuiStyle* style,
                                                           Gui::ToolbarButtonSize buttonSize, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 SelectionGrid(i32 selected, Span<const String> texts, i32 xCount, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static i32 SelectionGrid(i32 selected, Span<GuiContent* const> contents, i32 xCount, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static String TextField(StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static String TextField(StringView text, i32 maxLength, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static String TextField(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static String PasswordField(StringView password, char16_t maskChar, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static String TextArea(StringView text, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static String TextArea(StringView text, i32 maxLength, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static f32 HorizontalSlider(f32 value, f32 leftValue, f32 rightValue, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static f32 VerticalSlider(f32 value, f32 topValue, f32 bottomValue, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static f32 HorizontalScrollbar(f32 value, f32 size, f32 leftValue, f32 rightValue, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static f32 VerticalScrollbar(f32 value, f32 size, f32 topValue, f32 bottomValue, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION() static void Space(f32 pixels);
        HX_FUNCTION() static void FlexibleSpace();
        HX_FUNCTION(Params = "options") static void BeginHorizontal(Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void BeginHorizontal(GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void BeginHorizontal(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION() static void EndHorizontal();
        HX_FUNCTION(Params = "options") static void BeginVertical(Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void BeginVertical(GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static void BeginVertical(StringView text, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION() static void EndVertical();
        HX_FUNCTION() static void BeginArea(Rect screenRect);
        HX_FUNCTION() static void BeginArea(Rect screenRect, StringView text);
        HX_FUNCTION() static void BeginArea(Rect screenRect, GuiStyle* style);
        HX_FUNCTION() static void BeginArea(Rect screenRect, StringView text, GuiStyle* style);
        HX_FUNCTION() static void EndArea();
        HX_FUNCTION(Params = "options") static Vector2 BeginScrollView(Vector2 scrollPosition, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Vector2 BeginScrollView(Vector2 scrollPosition, bool alwaysShowHorizontal, bool alwaysShowVertical, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Vector2 BeginScrollView(Vector2 scrollPosition, GuiStyle* horizontalScrollbar, GuiStyle* verticalScrollbar, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Vector2 BeginScrollView(Vector2 scrollPosition, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Vector2 BeginScrollView(Vector2 scrollPosition, bool alwaysShowHorizontal, bool alwaysShowVertical,
                                                                       GuiStyle* horizontalScrollbar, GuiStyle* verticalScrollbar, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION() static void EndScrollView();
        HX_FUNCTION(Internal, Params = "options") static Rect WindowInternal(i32 id, Rect screenRect, GuiContent* title, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION() static GuiLayoutOption* Width(f32 width);
        HX_FUNCTION() static GuiLayoutOption* Height(f32 height);
        HX_FUNCTION() static GuiLayoutOption* MinWidth(f32 minWidth);
        HX_FUNCTION() static GuiLayoutOption* MaxWidth(f32 maxWidth);
        HX_FUNCTION() static GuiLayoutOption* MinHeight(f32 minHeight);
        HX_FUNCTION() static GuiLayoutOption* MaxHeight(f32 maxHeight);
        HX_FUNCTION() static GuiLayoutOption* ExpandWidth(bool expand);
        HX_FUNCTION() static GuiLayoutOption* ExpandHeight(bool expand);
    };

    HX_CLASS(Namespace = "HelixEngine", Static)
    class GuiLayoutUtility
    {
    public:
        HX_FUNCTION(Params = "options") static Rect GetRect(f32 width, f32 height, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Rect GetRect(f32 width, f32 height, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Rect GetRect(f32 minWidth, f32 maxWidth, f32 minHeight, f32 maxHeight, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION(Params = "options") static Rect GetRect(GuiContent* content, GuiStyle* style, Span<GuiLayoutOption* const> options = {});
        HX_FUNCTION() static Rect GetLastRect();
        HX_FUNCTION(Params = "options") static Rect GetAspectRect(f32 aspect, Span<GuiLayoutOption* const> options = {});
    };

    HX_CLASS(Namespace = "HelixEngine", Static)
    class GuiUtility
    {
    public:
        HX_PROPERTY() static i32 hotControl();
        static void SetHotControl(i32 value);
        HX_PROPERTY() static i32 keyboardControl();
        static void SetKeyboardControl(i32 value);
        HX_PROPERTY() static String systemCopyBuffer();
        static void SetSystemCopyBuffer(StringView value);
        HX_PROPERTY(ReadOnly) static bool hasModalWindow();
        HX_FUNCTION() static i32 GetControlID(FocusType focus);
        HX_FUNCTION() static i32 GetControlID(i32 hint, FocusType focus);
        HX_FUNCTION() static i32 GetControlID(FocusType focus, Rect position);
        HX_FUNCTION() static i32 GetControlID(i32 hint, FocusType focus, Rect position);
        HX_FUNCTION() static i32 GetControlID(GuiContent* contents, FocusType focus);
        HX_FUNCTION() static i32 GetControlID(GuiContent* contents, FocusType focus, Rect position);
        HX_FUNCTION() static void ExitGui();
        HX_FUNCTION() static Vector2 GuiToScreenPoint(Vector2 guiPoint);
        HX_FUNCTION() static Rect GuiToScreenRect(Rect guiRect);
        HX_FUNCTION() static Vector2 ScreenToGuiPoint(Vector2 screenPoint);
        HX_FUNCTION() static Rect ScreenToGuiRect(Rect screenRect);
        HX_FUNCTION() static void RotateAroundPivot(f32 angle, Vector2 pivotPoint);
        HX_FUNCTION() static void ScaleAroundPivot(Vector2 scale, Vector2 pivotPoint);
        // The rect with its corners on the screen's pixels (through the matrix), and its size in them.
        HX_FUNCTION() static Rect AlignRectToDevice(Rect rect);
        HX_FUNCTION(Out = "widthInPixels,heightInPixels") static Rect AlignRectToDevice(Rect rect, i32& widthInPixels, i32& heightInPixels);
        // A control's state object (by its id), made on its first query: C# keeps them (GetStateObject is generic
        // there); C++ scripts keep theirs by id.
    };

    // The editing of a text field: its text, cursor and selection, moves and edits by the keys.
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class TextEditor : public GuiObjectBase
    {
    public:
        // What a double click's drag selects whole of.
        HX_ENUM()
        enum class DblClickSnapping : u8
        {
            WORDS = 0,
            PARAGRAPHS = 1,
        };

        HX_FUNCTION(Constructor) TextEditor() = default;
        ~TextEditor() override;
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() String text() const { return text_; }
        void SetText(StringView v);
        HX_PROPERTY() i32 cursorIndex() const { return cursor_; }
        void SetCursorIndex(i32 v);
        HX_PROPERTY() i32 selectIndex() const { return select_; }
        void SetSelectIndex(i32 v);
        HX_PROPERTY() i32 controlID() const { return controlID_; }
        void SetControlID(i32 v) { controlID_ = v; }
        HX_PROPERTY() bool multiline() const { return multiline_; }
        void SetMultiline(bool v) { multiline_ = v; }
        HX_PROPERTY(ReadOnly) bool isMultiline() const { return multiline_; }
        HX_PROPERTY() bool hasHorizontalCursor() const { return hasHorizontalCursor_; }
        void SetHasHorizontalCursor(bool v) { hasHorizontalCursor_ = v; }
        HX_PROPERTY() bool hasHorizontalCursorPos() const { return hasHorizontalCursor_; }
        void SetHasHorizontalCursorPos(bool v) { hasHorizontalCursor_ = v; }
        HX_PROPERTY() bool showCursor() const { return showCursor_; }
        void SetShowCursor(bool v) { showCursor_ = v; }
        HX_PROPERTY() bool revealCursor() const { return revealCursor_; }
        void SetRevealCursor(bool v) { revealCursor_ = v; }
        HX_PROPERTY() i32 altCursorPosition() const { return alt_; }
        void SetAltCursorPosition(i32 v) { alt_ = v; }
        HX_PROPERTY() DblClickSnapping doubleClickSnapping() const { return snapping_; }
        void SetDoubleClickSnapping(DblClickSnapping v) { snapping_ = v; }
        // The text as content (the old field).
        HX_PROPERTY() GuiContent* content();
        void SetContent(GuiContent* v);
        HX_PROPERTY() bool isPasswordField() const { return password_; }
        void SetIsPasswordField(bool v) { password_ = v; }
        HX_PROPERTY() Rect position() const { return position_; }
        void SetPosition(Rect v) { position_ = v; }
        HX_PROPERTY() GuiStyle* style() const { return style_; }
        void SetStyle(GuiStyle* v) { style_ = v; }
        HX_PROPERTY() Vector2 scrollOffset() const { return scroll_; }
        void SetScrollOffset(Vector2 v) { scroll_ = v; }
        HX_PROPERTY() bool hasSelection() const { return cursor_ != select_; }
        HX_PROPERTY(ReadOnly) String SelectedText() const;
        HX_PROPERTY() Vector2 graphicalCursorPos() const { return graphicalCursor_; }
        void SetGraphicalCursorPos(Vector2 v) { graphicalCursor_ = v; }
        HX_PROPERTY() Vector2 graphicalSelectCursorPos() const { return graphicalSelect_; }
        void SetGraphicalSelectCursorPos(Vector2 v) { graphicalSelect_ = v; }

        HX_FUNCTION() void OnFocus();
        HX_FUNCTION() void OnLostFocus();
        HX_FUNCTION() bool HandleKeyEvent(Event* e);
        HX_FUNCTION() bool DeleteLineBack();
        HX_FUNCTION() bool DeleteWordBack();
        HX_FUNCTION() bool DeleteWordForward();
        HX_FUNCTION() bool Delete();
        HX_FUNCTION() bool CanPaste();
        HX_FUNCTION() bool Backspace();
        HX_FUNCTION() void SelectAll();
        HX_FUNCTION() void SelectNone();
        HX_FUNCTION() bool DeleteSelection();
        HX_FUNCTION() void ReplaceSelection(StringView replace);
        HX_FUNCTION() void Insert(char16_t c);
        HX_FUNCTION() void MoveRight();
        HX_FUNCTION() void MoveLeft();
        HX_FUNCTION() void MoveUp();
        HX_FUNCTION() void MoveDown();
        HX_FUNCTION() void MoveLineStart();
        HX_FUNCTION() void MoveLineEnd();
        HX_FUNCTION() void MoveTextStart();
        HX_FUNCTION() void MoveTextEnd();
        HX_FUNCTION() void MoveWordRight();
        HX_FUNCTION() void MoveWordLeft();
        HX_FUNCTION() void SelectLeft();
        HX_FUNCTION() void SelectRight();
        HX_FUNCTION() void SelectUp();
        HX_FUNCTION() void SelectDown();
        HX_FUNCTION() void SelectTextStart();
        HX_FUNCTION() void SelectTextEnd();
        HX_FUNCTION() void SelectToStartOfNextWord();
        HX_FUNCTION() void SelectToEndOfPreviousWord();
        HX_FUNCTION() void SelectWordRight();
        HX_FUNCTION() void SelectWordLeft();
        HX_FUNCTION() void SelectCurrentWord();
        HX_FUNCTION() void SelectCurrentParagraph();
        HX_FUNCTION() void MoveCursorToPosition(Vector2 cursorPosition);
        HX_FUNCTION() void SelectToPosition(Vector2 cursorPosition);
        HX_FUNCTION() void DblClickSnap(DblClickSnapping snapping);
        HX_FUNCTION() void MouseDragSelectsWholeWords(bool on);
        HX_FUNCTION() void MoveGraphicalLineStart();
        HX_FUNCTION() void MoveGraphicalLineEnd();
        HX_FUNCTION() void MoveParagraphForward();
        HX_FUNCTION() void MoveParagraphBackward();
        HX_FUNCTION() void MoveToStartOfNextWord();
        HX_FUNCTION() void MoveToEndOfPreviousWord();
        HX_FUNCTION() void MoveAltCursorToPosition(Vector2 cursorPosition);
        HX_FUNCTION() void MoveSelectionToAltCursor();
        HX_FUNCTION() void SelectGraphicalLineStart();
        HX_FUNCTION() void SelectGraphicalLineEnd();
        HX_FUNCTION() void ExpandSelectGraphicalLineStart();
        HX_FUNCTION() void ExpandSelectGraphicalLineEnd();
        HX_FUNCTION() void SelectParagraphForward();
        HX_FUNCTION() void SelectParagraphBackward();
        HX_FUNCTION() bool IsOverSelection(Vector2 cursorPosition);
        HX_FUNCTION() i32 FindStartOfNextWord(i32 p);
        HX_FUNCTION(Out = "linkData") bool HasClickedOnLink(Vector2 mousePosition, String& linkData);
        HX_FUNCTION(Out = "href") bool HasClickedOnHREF(Vector2 mousePosition, String& href);
        HX_FUNCTION() void DetectFocusChange();
        HX_FUNCTION() bool Cut();
        HX_FUNCTION() void Copy();
        HX_FUNCTION() bool Paste();
        HX_FUNCTION() void DrawCursor(StringView newText);
        HX_FUNCTION() void SaveBackup();
        HX_FUNCTION() void Undo();
        HX_FUNCTION() void UpdateScrollOffsetIfNeeded(Event* evt);

    private:
        i32 Clamp(i32 i) const;
        // The visual line of an index (its first and its end index), the link under a point.
        bool GraphicalLine(i32 at, i32& start, i32& end) const;
        bool LinkAt(Vector2 point, bool href, String& out) const;
        i32 IndexAt(Vector2 point) const;
        i32 WordStart(i32 at) const;
        i32 WordEnd(i32 at) const;
        i32 LineStart(i32 at) const;
        i32 LineEnd(i32 at) const;
        // The text as it is measured (a password field's mask) and the double click's snapping.
        String Shown() const;
        friend struct TextControl;
        String text_, backup_;
        char16_t mask_ = u'*';
        i32 snap_ = 0;
        DblClickSnapping snapping_ = DblClickSnapping::WORDS;
        bool hasHorizontalCursor_ = false, showCursor_ = true, revealCursor_ = false, hasFocus_ = false;
        i32 alt_ = -1;
        Vector2 graphicalSelect_ = Vector2(0, 0);
        GuiContent* content_ = nullptr;
        i32 cursor_ = 0, select_ = 0, backupCursor_ = 0, backupSelect_ = 0, controlID_ = 0;
        bool multiline_ = false, password_ = false;
        Rect position_;
        GuiStyle* style_ = nullptr;
        Vector2 scroll_ = Vector2(0, 0), graphicalCursor_ = Vector2(0, 0);
    };

    // ------------------------------------------------------------------------------------------------- the system
    // Run by the UI module: the frame's input events through the OnGui scripts (the player loop's
    // IMGUISendQueuedEvents, before Update), then Layout and Repaint as the frame is drawn (after the panels).
    namespace system
    {
        void SendQueuedEvents();
        // Whether the GUI used the input event at an index of the input frame (the panels leave it then).
        bool UsedInput(u64 inputFrame, usize index);
        // The OnGui scripts' Repaint into the mesh (by depth: the deepest first).
        void Repaint();
        // The GUI's mesh of the last Repaint.
        const UIMesh& Mesh();
        // Its draw calls into the command buffer (the screen's view).
        void Draw(engine::CommandBuffer& cmd);
        void ResetForTests();
    }
}

HX_GUI_OBJECT_HANDLES(GuiLayoutOption)
HX_GUI_OBJECT_HANDLES(TextEditor)
