#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Engine/Object/Object.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>
#include <Helix/Platform/Window/Input.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/UI/Engine/FontAsset.h>

// The immediate GUI's types (namespace HelixEngine): the event of a pass, styles and their states, contents, the skin.
// Their objects are C++ (one implementation for C# and C++ scripts); what a script makes with new is its own.
namespace helix::ui::gui
{
    // ------------------------------------------------------------------------------------------------- enums

    HX_ENUM(Namespace = "HelixEngine")
    enum class EventType : i32
    {
        MouseDown = 0,
        MouseUp = 1,
        MouseMove = 2,
        MouseDrag = 3,
        KeyDown = 4,
        KeyUp = 5,
        ScrollWheel = 6,
        Repaint = 7,
        Layout = 8,
        DragUpdated = 9,
        DragPerform = 10,
        DragExited = 15,
        Ignore = 11,
        Used = 12,
        ValidateCommand = 13,
        ExecuteCommand = 14,
        ContextClick = 16,
        MouseEnterWindow = 20,
        MouseLeaveWindow = 21,
        TouchDown = 30,
        TouchUp = 31,
        TouchMove = 32,
        TouchEnter = 33,
        TouchLeave = 34,
        TouchStationary = 35,
        // The old names of the first values.
        mouseDown = 0,
        mouseUp = 1,
        mouseMove = 2,
        mouseDrag = 3,
        keyDown = 4,
        keyUp = 5,
        scrollWheel = 6,
        repaint = 7,
        layout = 8,
        dragUpdated = 9,
        dragPerform = 10,
        ignore = 11,
        used = 12,
    };

    HX_ENUM(Namespace = "HelixEngine", Attributes = "[System.Flags]")
    enum class EventModifiers : i32
    {
        None = 0,
        Shift = 1,
        Control = 2,
        Alt = 4,
        Command = 8,
        Numeric = 16,
        CapsLock = 32,
        FunctionKey = 64,
    };

    // The state of a pen's tip and buttons.
    HX_ENUM(Namespace = "HelixEngine", Attributes = "[System.Flags]")
    enum class PenStatus : i32
    {
        None = 0,
        Contact = 1,
        Barrel = 2,
        Inverted = 4,
        Eraser = 8,
    };

    HX_ENUM(Namespace = "HelixEngine")
    enum class PointerType : i32
    {
        Mouse = 0,
        Touch = 1,
        Pen = 2,
    };

    HX_ENUM(Namespace = "HelixEngine")
    enum class FocusType : i32
    {
        Native = 0,
        Keyboard = 1,
        Passive = 2,
    };

    HX_ENUM(Namespace = "HelixEngine")
    enum class TextClipping : i32
    {
        Overflow = 0,
        Clip = 1,
        Ellipsis = 2,
    };

    HX_ENUM(Namespace = "HelixEngine")
    enum class ImagePosition : i32
    {
        ImageLeft = 0,
        ImageAbove = 1,
        ImageOnly = 2,
        TextOnly = 3,
    };

    // Text placement and font styles (of the styles of the immediate GUI).
    HX_ENUM(Namespace = "HelixEngine")
    enum class TextAnchor : i32
    {
        UpperLeft = 0,
        UpperCenter = 1,
        UpperRight = 2,
        MiddleLeft = 3,
        MiddleCenter = 4,
        MiddleRight = 5,
        LowerLeft = 6,
        LowerCenter = 7,
        LowerRight = 8,
    };

    HX_ENUM(Namespace = "HelixEngine")
    enum class FontStyle : i32
    {
        Normal = 0,
        Bold = 1,
        Italic = 2,
        BoldAndItalic = 3,
    };

    // ------------------------------------------------------------------------------------------------- objects

    // What every object of the immediate GUI shares with the bindings: its id, its C# object, its type.
    class GuiObjectBase
    {
    public:
        GuiObjectBase();
        virtual ~GuiObjectBase();
        GuiObjectBase(const GuiObjectBase&) = delete;
        GuiObjectBase& operator=(const GuiObjectBase&) = delete;
        u64 Id() const { return id_; }
        u64& Wrapper() { return wrapper_; }
        virtual scripting::TypeId HelixType() const = 0;
        static GuiObjectBase* Find(u64 id);

    private:
        u64 id_ = 0;
        u64 wrapper_ = 0;
    };

    // Left, right, top and bottom offsets (borders, margins, padding, overflow).
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class RectOffset : public GuiObjectBase
    {
    public:
        HX_FUNCTION(Constructor) RectOffset() = default;
        HX_FUNCTION(Constructor) RectOffset(i32 left, i32 right, i32 top, i32 bottom) : left_(left), right_(right), top_(top), bottom_(bottom) {}
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() i32 left() const { return left_; }
        void SetLeft(i32 v) { left_ = v; }
        HX_PROPERTY() i32 right() const { return right_; }
        void SetRight(i32 v) { right_ = v; }
        HX_PROPERTY() i32 top() const { return top_; }
        void SetTop(i32 v) { top_ = v; }
        HX_PROPERTY() i32 bottom() const { return bottom_; }
        void SetBottom(i32 v) { bottom_ = v; }
        HX_PROPERTY(ReadOnly) i32 horizontal() const { return left_ + right_; }
        HX_PROPERTY(ReadOnly) i32 vertical() const { return top_ + bottom_; }
        HX_FUNCTION() Rect Add(Rect rect) const { return Rect(rect.x - f32(left_), rect.y - f32(top_), rect.width + f32(horizontal()), rect.height + f32(vertical())); }
        HX_FUNCTION() Rect Remove(Rect rect) const { return Rect(rect.x + f32(left_), rect.y + f32(top_), rect.width - f32(horizontal()), rect.height - f32(vertical())); }
        HX_FUNCTION() String ToString() const;
        void Set(i32 l, i32 r, i32 t, i32 b) { left_ = l, right_ = r, top_ = t, bottom_ = b; }
        void CopyFrom(const RectOffset& o) { Set(o.left_, o.right_, o.top_, o.bottom_); }

    private:
        i32 left_ = 0, right_ = 0, top_ = 0, bottom_ = 0;
    };

    // A control's content: its text, image and tooltip.
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class GuiContent : public GuiObjectBase
    {
    public:
        HX_FUNCTION(Constructor) GuiContent() = default;
        HX_FUNCTION(Constructor) explicit GuiContent(StringView text) : text_(text) {}
        HX_FUNCTION(Constructor) explicit GuiContent(engine::Texture2D* image) : image_(image) {}
        HX_FUNCTION(Constructor) GuiContent(StringView text, engine::Texture2D* image) : text_(text), image_(image) {}
        HX_FUNCTION(Constructor) GuiContent(StringView text, StringView tooltip) : text_(text), tooltip_(tooltip) {}
        HX_FUNCTION(Constructor) GuiContent(engine::Texture2D* image, StringView tooltip) : tooltip_(tooltip), image_(image) {}
        HX_FUNCTION(Constructor) GuiContent(StringView text, engine::Texture2D* image, StringView tooltip) : text_(text), tooltip_(tooltip), image_(image) {}
        HX_FUNCTION(Constructor) explicit GuiContent(GuiContent* source);
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() String text() const { return text_; }
        void SetText(StringView v) { text_ = String(v); }
        HX_PROPERTY() engine::Texture2D* image() const { return image_.Get(); }
        void SetImage(engine::Texture2D* v) { image_ = engine::PPtr<engine::Texture2D>(v); }
        HX_PROPERTY() String tooltip() const { return tooltip_; }
        void SetTooltip(StringView v) { tooltip_ = String(v); }
        HX_PROPERTY(ReadOnly) static GuiContent* none();
        HX_FUNCTION() String ToString() const { return text_; }

        // A temporary content of the calls that take a text or an image (one of a few reused slots).
        static GuiContent& Temp(StringView text, engine::Texture2D* image = nullptr, StringView tooltip = StringView());

    private:
        String text_, tooltip_;
        engine::PPtr<engine::Texture2D> image_;
    };

    // A style's look in one state: its background and text color.
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class GuiStyleState : public GuiObjectBase
    {
    public:
        HX_FUNCTION(Constructor) GuiStyleState() = default;
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() engine::Texture2D* background() const { return background_.Get(); }
        void SetBackground(engine::Texture2D* v) { background_ = engine::PPtr<engine::Texture2D>(v); }
        HX_PROPERTY() Array<engine::Texture2D*> scaledBackgrounds() const;
        void SetScaledBackgrounds(Span<engine::Texture2D* const> v);
        HX_PROPERTY() Color textColor() const { return textColor_; }
        void SetTextColor(Color v) { textColor_ = v; }
        void CopyFrom(const GuiStyleState& o);

    private:
        engine::PPtr<engine::Texture2D> background_;
        Array<engine::PPtr<engine::Texture2D>> scaled_;
        Color textColor_ = Color(0, 0, 0, 1);
    };

    // How a control looks and is laid out: its states (normal, hover, active, focused and their on states), its
    // offsets, font, text placement and fixed sizes.
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class GuiStyle : public GuiObjectBase
    {
    public:
        HX_FUNCTION(Constructor) GuiStyle();
        HX_FUNCTION(Constructor) explicit GuiStyle(GuiStyle* other);
        ~GuiStyle() override;
        scripting::TypeId HelixType() const override;
        void CopyFrom(const GuiStyle& other);

        HX_PROPERTY() String name() const { return name_; }
        void SetName(StringView v) { name_ = String(v); }
        HX_PROPERTY() GuiStyleState* normal() const { return states_[0]; }
        void SetNormal(GuiStyleState* v) { SetState(0, v); }
        HX_PROPERTY() GuiStyleState* hover() const { return states_[1]; }
        void SetHover(GuiStyleState* v) { SetState(1, v); }
        HX_PROPERTY() GuiStyleState* active() const { return states_[2]; }
        void SetActive(GuiStyleState* v) { SetState(2, v); }
        HX_PROPERTY() GuiStyleState* focused() const { return states_[3]; }
        void SetFocused(GuiStyleState* v) { SetState(3, v); }
        HX_PROPERTY() GuiStyleState* onNormal() const { return states_[4]; }
        void SetOnNormal(GuiStyleState* v) { SetState(4, v); }
        HX_PROPERTY() GuiStyleState* onHover() const { return states_[5]; }
        void SetOnHover(GuiStyleState* v) { SetState(5, v); }
        HX_PROPERTY() GuiStyleState* onActive() const { return states_[6]; }
        void SetOnActive(GuiStyleState* v) { SetState(6, v); }
        HX_PROPERTY() GuiStyleState* onFocused() const { return states_[7]; }
        void SetOnFocused(GuiStyleState* v) { SetState(7, v); }
        HX_PROPERTY() RectOffset* border() const { return offsets_[0]; }
        void SetBorder(RectOffset* v) { SetOffset(0, v); }
        HX_PROPERTY() RectOffset* margin() const { return offsets_[1]; }
        void SetMargin(RectOffset* v) { SetOffset(1, v); }
        HX_PROPERTY() RectOffset* padding() const { return offsets_[2]; }
        void SetPadding(RectOffset* v) { SetOffset(2, v); }
        HX_PROPERTY() RectOffset* overflow() const { return offsets_[3]; }
        void SetOverflow(RectOffset* v) { SetOffset(3, v); }
        HX_PROPERTY() FontAsset* font() const { return font_.Get(); }
        void SetFont(FontAsset* v) { font_ = engine::PPtr<FontAsset>(v); }
        HX_PROPERTY() i32 fontSize() const { return fontSize_; }
        void SetFontSize(i32 v) { fontSize_ = v; }
        HX_PROPERTY() FontStyle fontStyle() const { return fontStyle_; }
        void SetFontStyle(FontStyle v) { fontStyle_ = v; }
        HX_PROPERTY() TextAnchor alignment() const { return alignment_; }
        void SetAlignment(TextAnchor v) { alignment_ = v; }
        HX_PROPERTY() bool wordWrap() const { return wordWrap_; }
        void SetWordWrap(bool v) { wordWrap_ = v; }
        HX_PROPERTY() bool richText() const { return richText_; }
        void SetRichText(bool v) { richText_ = v; }
        HX_PROPERTY() TextClipping clipping() const { return clipping_; }
        void SetClipping(TextClipping v) { clipping_ = v; }
        HX_PROPERTY() ImagePosition imagePosition() const { return imagePosition_; }
        void SetImagePosition(ImagePosition v) { imagePosition_ = v; }
        HX_PROPERTY() Vector2 contentOffset() const { return contentOffset_; }
        void SetContentOffset(Vector2 v) { contentOffset_ = v; }
        HX_PROPERTY() f32 fixedWidth() const { return fixedWidth_; }
        void SetFixedWidth(f32 v) { fixedWidth_ = v; }
        HX_PROPERTY() f32 fixedHeight() const { return fixedHeight_; }
        void SetFixedHeight(f32 v) { fixedHeight_ = v; }
        HX_PROPERTY() bool stretchWidth() const { return stretchWidth_; }
        void SetStretchWidth(bool v) { stretchWidth_ = v; }
        HX_PROPERTY() bool stretchHeight() const { return stretchHeight_; }
        void SetStretchHeight(bool v) { stretchHeight_ = v; }
        HX_PROPERTY(ReadOnly) f32 lineHeight() const;
        HX_PROPERTY(ReadOnly) bool isHeightDependantOnWidth() const { return fixedHeight_ == 0 && wordWrap_ && imagePosition_ != ImagePosition::ImageOnly; }
        HX_PROPERTY(ReadOnly) static GuiStyle* none();

        // Drawing (in Repaint only): the background of the state, then the content.
        HX_FUNCTION() void Draw(Rect position, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const;
        HX_FUNCTION() void Draw(Rect position, StringView text, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const;
        HX_FUNCTION() void Draw(Rect position, engine::Texture2D* image, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const;
        HX_FUNCTION() void Draw(Rect position, GuiContent* content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const;
        HX_FUNCTION() void Draw(Rect position, GuiContent* content, i32 controlID) const;
        HX_FUNCTION() void Draw(Rect position, GuiContent* content, i32 controlID, bool on) const;
        HX_FUNCTION() void Draw(Rect position, GuiContent* content, i32 controlID, bool on, bool hover) const;
        HX_FUNCTION() void DrawCursor(Rect position, GuiContent* content, i32 controlID, i32 character) const;
        HX_FUNCTION() void DrawWithTextSelection(Rect position, GuiContent* content, i32 controlID, i32 firstSelectedCharacter, i32 lastSelectedCharacter) const;
        // Measuring.
        HX_FUNCTION() Vector2 CalcSize(GuiContent* content) const;
        HX_FUNCTION() Vector2 CalcScreenSize(Vector2 contentSize) const;
        HX_FUNCTION() f32 CalcHeight(GuiContent* content, f32 width) const;
        HX_FUNCTION(Out = "minWidth,maxWidth") void CalcMinMaxWidth(GuiContent* content, f32& minWidth, f32& maxWidth) const;
        HX_FUNCTION() Vector2 GetCursorPixelPosition(Rect position, GuiContent* content, i32 cursorStringIndex) const;
        HX_FUNCTION() i32 GetCursorStringIndex(Rect position, GuiContent* content, Vector2 cursorPixelPosition) const;
        HX_FUNCTION() String ToString() const;

        // For the controls: the content drawn in a state chosen by the caller (the text cursor and selection hidden).
        void DrawState(Rect position, const GuiContent& content, bool isHover, bool isActive, bool on, bool hasKeyboardFocus) const;
        f32 EffectiveFontSize() const;
        // The id of the link (<link="..."> or, href, <a href="...">) whose characters are under a point.
        bool LinkAtPoint(Rect position, const GuiContent& content, Vector2 point, bool href, String& out) const;

    private:
        void SetState(u32 i, GuiStyleState* v);
        void SetOffset(u32 i, RectOffset* v);
        String name_;
        GuiStyleState* states_[8] = {};
        RectOffset* offsets_[4] = {};
        engine::PPtr<FontAsset> font_;
        i32 fontSize_ = 0;
        FontStyle fontStyle_ = FontStyle::Normal;
        TextAnchor alignment_ = TextAnchor::UpperLeft;
        bool wordWrap_ = false, richText_ = true;
        TextClipping clipping_ = TextClipping::Overflow;
        ImagePosition imagePosition_ = ImagePosition::ImageLeft;
        Vector2 contentOffset_ = Vector2(0, 0);
        f32 fixedWidth_ = 0, fixedHeight_ = 0;
        bool stretchWidth_ = true, stretchHeight_ = false;
    };

    // The skin's settings of the text controls.
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class GuiSettings : public GuiObjectBase
    {
    public:
        HX_FUNCTION(Constructor) GuiSettings() = default;
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() bool doubleClickSelectsWord() const { return doubleClickSelectsWord_; }
        void SetDoubleClickSelectsWord(bool v) { doubleClickSelectsWord_ = v; }
        HX_PROPERTY() bool tripleClickSelectsLine() const { return tripleClickSelectsLine_; }
        void SetTripleClickSelectsLine(bool v) { tripleClickSelectsLine_ = v; }
        HX_PROPERTY() Color cursorColor() const { return cursorColor_; }
        void SetCursorColor(Color v) { cursorColor_ = v; }
        HX_PROPERTY() f32 cursorFlashSpeed() const { return cursorFlashSpeed_; }
        void SetCursorFlashSpeed(f32 v) { cursorFlashSpeed_ = v; }
        HX_PROPERTY() Color selectionColor() const { return selectionColor_; }
        void SetSelectionColor(Color v) { selectionColor_ = v; }

    private:
        bool doubleClickSelectsWord_ = true, tripleClickSelectsLine_ = true;
        Color cursorColor_ = Color(1, 1, 1, 1);
        f32 cursorFlashSpeed_ = -1;
        Color selectionColor_ = Color(0.24f, 0.49f, 0.91f, 0.65f);
    };

    // The styles of the built-in controls, custom styles by name, the font and the settings (an asset).
    HX_CLASS(Namespace = "HelixEngine")
    class GuiSkin : public engine::ScriptableObject
    {
    public:
        GuiSkin();
        ~GuiSkin() override;
        HX_PROPERTY() FontAsset* font() const { return font_.Get(); }
        void SetFont(FontAsset* v) { font_ = engine::PPtr<FontAsset>(v); }
        HX_PROPERTY() GuiStyle* box() const { return styles_[0]; }
        void SetBox(GuiStyle* v) { SetStyle(0, v); }
        HX_PROPERTY() GuiStyle* label() const { return styles_[1]; }
        void SetLabel(GuiStyle* v) { SetStyle(1, v); }
        HX_PROPERTY() GuiStyle* textField() const { return styles_[2]; }
        void SetTextField(GuiStyle* v) { SetStyle(2, v); }
        HX_PROPERTY() GuiStyle* textArea() const { return styles_[3]; }
        void SetTextArea(GuiStyle* v) { SetStyle(3, v); }
        HX_PROPERTY() GuiStyle* button() const { return styles_[4]; }
        void SetButton(GuiStyle* v) { SetStyle(4, v); }
        HX_PROPERTY() GuiStyle* toggle() const { return styles_[5]; }
        void SetToggle(GuiStyle* v) { SetStyle(5, v); }
        HX_PROPERTY() GuiStyle* window() const { return styles_[6]; }
        void SetWindow(GuiStyle* v) { SetStyle(6, v); }
        HX_PROPERTY() GuiStyle* horizontalSlider() const { return styles_[7]; }
        void SetHorizontalSlider(GuiStyle* v) { SetStyle(7, v); }
        HX_PROPERTY() GuiStyle* horizontalSliderThumb() const { return styles_[8]; }
        void SetHorizontalSliderThumb(GuiStyle* v) { SetStyle(8, v); }
        HX_PROPERTY() GuiStyle* verticalSlider() const { return styles_[9]; }
        void SetVerticalSlider(GuiStyle* v) { SetStyle(9, v); }
        HX_PROPERTY() GuiStyle* verticalSliderThumb() const { return styles_[10]; }
        void SetVerticalSliderThumb(GuiStyle* v) { SetStyle(10, v); }
        HX_PROPERTY() GuiStyle* horizontalScrollbar() const { return styles_[11]; }
        void SetHorizontalScrollbar(GuiStyle* v) { SetStyle(11, v); }
        HX_PROPERTY() GuiStyle* horizontalScrollbarThumb() const { return styles_[12]; }
        void SetHorizontalScrollbarThumb(GuiStyle* v) { SetStyle(12, v); }
        HX_PROPERTY() GuiStyle* horizontalScrollbarLeftButton() const { return styles_[13]; }
        void SetHorizontalScrollbarLeftButton(GuiStyle* v) { SetStyle(13, v); }
        HX_PROPERTY() GuiStyle* horizontalScrollbarRightButton() const { return styles_[14]; }
        void SetHorizontalScrollbarRightButton(GuiStyle* v) { SetStyle(14, v); }
        HX_PROPERTY() GuiStyle* verticalScrollbar() const { return styles_[15]; }
        void SetVerticalScrollbar(GuiStyle* v) { SetStyle(15, v); }
        HX_PROPERTY() GuiStyle* verticalScrollbarThumb() const { return styles_[16]; }
        void SetVerticalScrollbarThumb(GuiStyle* v) { SetStyle(16, v); }
        HX_PROPERTY() GuiStyle* verticalScrollbarUpButton() const { return styles_[17]; }
        void SetVerticalScrollbarUpButton(GuiStyle* v) { SetStyle(17, v); }
        HX_PROPERTY() GuiStyle* verticalScrollbarDownButton() const { return styles_[18]; }
        void SetVerticalScrollbarDownButton(GuiStyle* v) { SetStyle(18, v); }
        HX_PROPERTY() GuiStyle* scrollView() const { return styles_[19]; }
        void SetScrollView(GuiStyle* v) { SetStyle(19, v); }
        HX_PROPERTY() Array<GuiStyle*> customStyles() const;
        void SetCustomStyles(Span<GuiStyle* const> v);
        HX_PROPERTY(ReadOnly) GuiSettings* settings() const { return settings_; }
        HX_FUNCTION() GuiStyle* GetStyle(StringView styleName) const;
        HX_FUNCTION() GuiStyle* FindStyle(StringView styleName) const;
        // Every style: the built-in controls' then the custom ones (C#'s GetEnumerator goes through them).
        HX_FUNCTION(Internal) Array<GuiStyle*> AllStyles() const;

        static constexpr u32 kStyles = 20;
        GuiStyle* StyleAt(u32 i) const { return styles_[i]; }
        // Helix's default skin (made once: its images drawn by the UI).
        static GuiSkin* Default();

    private:
        void SetStyle(u32 i, GuiStyle* v);
        engine::PPtr<FontAsset> font_;
        GuiStyle* styles_[kStyles] = {};
        Array<GuiStyle*> custom_;
        GuiSettings* settings_ = nullptr;
    };

    // The event a pass of OnGui works on: its type, the pointer and keys, the command.
    HX_CLASS(Root, Owned, Namespace = "HelixEngine")
    class Event : public GuiObjectBase
    {
    public:
        HX_FUNCTION(Constructor) Event() = default;
        HX_FUNCTION(Constructor) explicit Event(i32 displayIndex) : displayIndex_(displayIndex) {}
        HX_FUNCTION(Constructor) explicit Event(Event* other);
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() EventType type() const { return type_; }
        void SetType(EventType v) { type_ = v; }
        HX_PROPERTY() EventType rawType() const { return rawType_; }
        void SetRawType(EventType v) { rawType_ = v; }
        HX_PROPERTY() Vector2 mousePosition() const { return mousePosition_; }
        void SetMousePosition(Vector2 v) { mousePosition_ = v; }
        HX_PROPERTY() Vector2 delta() const { return delta_; }
        void SetDelta(Vector2 v) { delta_ = v; }
        HX_PROPERTY() PointerType pointerType() const { return pointerType_; }
        void SetPointerType(PointerType v) { pointerType_ = v; }
        HX_PROPERTY() i32 button() const { return button_; }
        void SetButton(i32 v) { button_ = v; }
        HX_PROPERTY() EventModifiers modifiers() const { return modifiers_; }
        void SetModifiers(EventModifiers v) { modifiers_ = v; }
        HX_PROPERTY() f32 pressure() const { return pressure_; }
        void SetPressure(f32 v) { pressure_ = v; }
        HX_PROPERTY() f32 twist() const { return twist_; }
        void SetTwist(f32 v) { twist_ = v; }
        HX_PROPERTY() Vector2 tilt() const { return tilt_; }
        void SetTilt(Vector2 v) { tilt_ = v; }
        HX_PROPERTY() PenStatus penStatus() const { return penStatus_; }
        void SetPenStatus(PenStatus v) { penStatus_ = v; }
        HX_PROPERTY() i32 clickCount() const { return clickCount_; }
        void SetClickCount(i32 v) { clickCount_ = v; }
        HX_PROPERTY() char16_t character() const { return character_; }
        void SetCharacter(char16_t v) { character_ = v; }
        HX_PROPERTY() String commandName() const { return commandName_; }
        void SetCommandName(StringView v) { commandName_ = String(v); }
        HX_PROPERTY() platform::KeyCode keyCode() const { return keyCode_; }
        void SetKeyCode(platform::KeyCode v) { keyCode_ = v; }
        HX_PROPERTY() i32 displayIndex() const { return displayIndex_; }
        void SetDisplayIndex(i32 v) { displayIndex_ = v; }
        HX_PROPERTY() bool shift() const { return Has(EventModifiers::Shift); }
        void SetShift(bool v) { Set(EventModifiers::Shift, v); }
        HX_PROPERTY() bool control() const { return Has(EventModifiers::Control); }
        void SetControl(bool v) { Set(EventModifiers::Control, v); }
        HX_PROPERTY() bool alt() const { return Has(EventModifiers::Alt); }
        void SetAlt(bool v) { Set(EventModifiers::Alt, v); }
        HX_PROPERTY() bool command() const { return Has(EventModifiers::Command); }
        void SetCommand(bool v) { Set(EventModifiers::Command, v); }
        HX_PROPERTY() bool capsLock() const { return Has(EventModifiers::CapsLock); }
        void SetCapsLock(bool v) { Set(EventModifiers::CapsLock, v); }
        HX_PROPERTY() bool numeric() const { return Has(EventModifiers::Numeric); }
        void SetNumeric(bool v) { Set(EventModifiers::Numeric, v); }
        HX_PROPERTY(ReadOnly) bool functionKey() const { return Has(EventModifiers::FunctionKey); }
        HX_PROPERTY(ReadOnly) bool isKey() const { return type_ == EventType::KeyDown || type_ == EventType::KeyUp; }
        HX_PROPERTY(ReadOnly) bool isMouse() const;
        HX_PROPERTY(ReadOnly) bool isScrollWheel() const { return type_ == EventType::ScrollWheel; }
        // The event OnGui is called with (a static of the pass).
        HX_PROPERTY() static Event* current();
        static void SetCurrent(Event* e);
        HX_FUNCTION() void Use();
        HX_FUNCTION() EventType GetTypeForControl(i32 controlID) const;
        HX_FUNCTION() static Event* KeyboardEvent(StringView key);
        HX_FUNCTION() static i32 GetEventCount();
        // The next event of the frame's queue into outEvent (false when there is none).
        HX_FUNCTION() static bool PopEvent(Event* outEvent);
        HX_FUNCTION() String ToString() const;

        void CopyFrom(const Event& o);

    private:
        bool Has(EventModifiers m) const { return (i32(modifiers_) & i32(m)) != 0; }
        void Set(EventModifiers m, bool on) { modifiers_ = EventModifiers(on ? (i32(modifiers_) | i32(m)) : (i32(modifiers_) & ~i32(m))); }
        EventType type_ = EventType::Ignore, rawType_ = EventType::Ignore;
        Vector2 mousePosition_ = Vector2(0, 0), delta_ = Vector2(0, 0);
        PointerType pointerType_ = PointerType::Mouse;
        i32 button_ = 0;
        EventModifiers modifiers_ = EventModifiers::None;
        f32 pressure_ = 0, twist_ = 0;
        Vector2 tilt_ = Vector2(0, 0);
        PenStatus penStatus_ = PenStatus::None;
        i32 clickCount_ = 0;
        char16_t character_ = 0;
        String commandName_;
        platform::KeyCode keyCode_ = platform::KeyCode::None;
        i32 displayIndex_ = 0;
    };
}

// The bindings' ids of the immediate GUI's objects.
#define HX_GUI_OBJECT_HANDLES(T)                                                                                       \
    template <>                                                                                                        \
    struct helix::scripting::ObjectHandles<helix::ui::gui::T>                                                               \
    {                                                                                                                  \
        static u64 ToId(const ui::gui::T* h) { return h->Id(); }                                                           \
        static ui::gui::T* FromId(u64 id) { return static_cast<ui::gui::T*>(ui::gui::GuiObjectBase::Find(id)); }                     \
        static TypeId TypeOf(const ui::gui::T* h) { return h->HelixType(); }                                               \
        static u64& WrapperOf(ui::gui::T* h) { return h->Wrapper(); }                                                      \
    };

HX_GUI_OBJECT_HANDLES(RectOffset)
HX_GUI_OBJECT_HANDLES(GuiContent)
HX_GUI_OBJECT_HANDLES(GuiStyleState)
HX_GUI_OBJECT_HANDLES(GuiStyle)
HX_GUI_OBJECT_HANDLES(GuiSettings)
HX_GUI_OBJECT_HANDLES(Event)
