#pragma once

#include <Helix/UI/Engine/Control.h>

// The simple controls: elements whose objects build their parts and behave by callbacks on the world's events. Their
// looks come from the theme by their helix- classes (the block's class, its parts' block__part, modifiers
// block--modifier).
namespace helix::ui
{
    // An element a binding can find by its path.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class BindableElement : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) BindableElement();
        BindableElement(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() String bindingPath() const { return bindingPath_; }
        void SetBindingPath(StringView value) { bindingPath_ = String(value); }

        static Span<const ControlAttribute> MarkupAttributes();

    protected:
        explicit BindableElement(Atom type);

    private:
        String bindingPath_;
    };

    // An element that shows text (its own, laid out in its content box).
    HX_CLASS(Namespace = "HelixEngine.UI")
    class TextElement : public BindableElement
    {
    public:
        using ControlBase = BindableElement;
        HX_FUNCTION(Constructor) TextElement();
        TextElement(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() virtual String text() const;
        virtual void SetText(StringView value);
        HX_PROPERTY() bool enableRichText() const { return enableRichText_; }
        void SetEnableRichText(bool value);
        HX_PROPERTY() bool parseEscapeSequences() const { return parseEscapeSequences_; }
        void SetParseEscapeSequences(bool value) { parseEscapeSequences_ = value; }
        HX_PROPERTY() bool displayTooltipWhenElided() const { return displayTooltipWhenElided_; }
        void SetDisplayTooltipWhenElided(bool value) { displayTooltipWhenElided_ = value; }
        // The text did not fit its box and was cut with an ellipsis (as of the last layout).
        HX_PROPERTY(ReadOnly) bool isElided() const;
        // The size of a text in the element's style for a box (a negative side: no limit).
        HX_FUNCTION() Vector2 MeasureTextSize(StringView textToMeasure, f32 width, f32 height);

        static Span<const ControlAttribute> MarkupAttributes();
        // Whether an element's text is parsed for rich text tags.
        static bool RichTextOf(ElementHandle e);

    protected:
        explicit TextElement(Atom type);
        // The element whose text this one shows (itself unless a control keeps its text in a part).
        ElementHandle textHandle_;

    private:
        void InitText();
        bool enableRichText_ = true;
        bool parseEscapeSequences_ = false;
        bool displayTooltipWhenElided_ = true;
    };

    HX_CLASS(Namespace = "HelixEngine.UI")
    class Label : public TextElement
    {
    public:
        using ControlBase = TextElement;
        HX_FUNCTION(Constructor) Label();
        HX_FUNCTION(Constructor) explicit Label(StringView text);
        Label(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }

    protected:
        explicit Label(Atom type);

    private:
        void InitLabel();
    };

    // A button: pressed while a pointer holds it (:active), clicked when it is let go over the button or submitted
    // with the keyboard while it has the focus.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class Button : public TextElement
    {
    public:
        using ControlBase = TextElement;
        HX_FUNCTION(Constructor) Button();
        Button(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() engine::Texture2D* iconImage() const { return icon_.Get(); }
        void SetIconImage(engine::Texture2D* value);
        // Clicks so far (the C# side's clicked runs for each).
        HX_PROPERTY(ReadOnly) i32 clickCount() const { return clicks_; }
        // A C++ owner told of each click (a group of buttons).
        void SetClickListener(void (*listener)(void* user, Button& button), void* user)
        {
            listener_ = listener;
            listenerUser_ = user;
        }
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }

    protected:
        explicit Button(Atom type);
        // A press (and each repetition of a repeat button) or a release over it.
        virtual void OnPress();
        virtual void OnRelease(bool inside);
        void Click();

    private:
        void InitButton();
        static void PointerDown(void* user, EventSystem& events, UIEvent& e);
        static void PointerMove(void* user, EventSystem& events, UIEvent& e);
        static void PointerUp(void* user, EventSystem& events, UIEvent& e);
        static void CaptureOut(void* user, EventSystem& events, UIEvent& e);
        static void Submit(void* user, EventSystem& events, UIEvent& e);
        engine::PPtr<engine::Texture2D> icon_;
        ElementHandle iconElement_;
        void (*listener_)(void* user, Button& button) = nullptr;
        void* listenerUser_ = nullptr;
        i32 clicks_ = 0;
        i32 pointer_ = -1;
    };

    // A button that clicks when pressed, then again after delay and every interval (milliseconds) while held.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class RepeatButton : public Button
    {
    public:
        using ControlBase = Button;
        HX_FUNCTION(Constructor) RepeatButton();
        RepeatButton(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() i64 delay() const { return delay_; }
        void SetDelay(i64 value) { delay_ = value; }
        HX_PROPERTY() i64 interval() const { return interval_; }
        void SetInterval(i64 value) { interval_ = value; }
        static Span<const ControlAttribute> MarkupAttributes();

    protected:
        void OnPress() override;
        void OnRelease(bool inside) override;

    private:
        void InitRepeat();
        static bool Repeat(Element& self, f64 time);
        i64 delay_ = 0, interval_ = 0;
        bool held_ = false;
        f64 next_ = -1;
    };

    // How an image fills its box.
    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class ScaleMode : i32
    {
        StretchToFill = 0,
        ScaleAndCrop = 1,
        ScaleToFit = 2,
    };

    HX_CLASS(Namespace = "HelixEngine.UI")
    class Image : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) Image();
        Image(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() engine::Texture2D* image() const { return image_.Get(); }
        void SetImage(engine::Texture2D* value);
        HX_PROPERTY() ScaleMode scaleMode() const { return scaleMode_; }
        void SetScaleMode(ScaleMode value);
        HX_PROPERTY() Color tintColor() const { return tint_; }
        void SetTintColor(Color value);
        // The part of the texture shown: uv in 0..1 (y up), or sourceRect in its pixels (y down).
        HX_PROPERTY() Rect uv() const { return uv_; }
        void SetUv(Rect value);
        HX_PROPERTY() Rect sourceRect() const;
        void SetSourceRect(Rect value);
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }

    private:
        void InitImage();
        void Apply();
        engine::PPtr<engine::Texture2D> image_;
        ScaleMode scaleMode_ = ScaleMode::ScaleToFit;
        Color tint_ = Color(1, 1, 1, 1);
        Rect uv_ = Rect(0, 0, 1, 1);
    };

    HX_CLASS(Namespace = "HelixEngine.UI")
    class Box : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) Box();
        Box(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    // A titled group; the radio buttons in it are exclusive.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class GroupBox : public BindableElement
    {
    public:
        using ControlBase = BindableElement;
        HX_FUNCTION(Constructor) GroupBox();
        HX_FUNCTION(Constructor) explicit GroupBox(StringView text);
        GroupBox(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() String text() const { return text_; }
        void SetText(StringView value);
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitGroup();
        String text_;
        ElementHandle label_;
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class HelpBoxMessageType : i32
    {
        None = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
    };

    // A message with an icon of its kind.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class HelpBox : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) HelpBox();
        HX_FUNCTION(Constructor) HelpBox(StringView text, HelpBoxMessageType messageType);
        HelpBox(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() String text() const;
        void SetText(StringView value);
        HX_PROPERTY() HelpBoxMessageType messageType() const { return type_; }
        void SetMessageType(HelpBoxMessageType value);
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitHelp();
        HelpBoxMessageType type_ = HelpBoxMessageType::None;
        ElementHandle icon_, label_;
    };

    // A titled frame for its content.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class PopupWindow : public TextElement
    {
    public:
        using ControlBase = TextElement;
        HX_FUNCTION(Constructor) PopupWindow();
        PopupWindow(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }

    private:
        void InitPopup();
        ElementHandle content_;
    };

    // A bar filled from lowValue to highValue, its title over it.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class ProgressBar : public BindableElement
    {
    public:
        using ControlBase = BindableElement;
        HX_FUNCTION(Constructor) ProgressBar();
        ProgressBar(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        HX_PROPERTY() f32 value() const { return value_; }
        void SetValue(f32 v);
        HX_PROPERTY() f32 lowValue() const { return low_; }
        void SetLowValue(f32 v);
        HX_PROPERTY() f32 highValue() const { return high_; }
        void SetHighValue(f32 v);
        HX_PROPERTY() String title() const;
        void SetTitle(StringView value);
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitProgress();
        void Fill();
        f32 value_ = 0, low_ = 0, high_ = 100;
        ElementHandle progress_, title_;
    };

    // Registers the controls' markup types (once).
    void RegisterControls();
}
