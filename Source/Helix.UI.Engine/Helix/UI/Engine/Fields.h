#pragma once

#include <Helix/Engine/Event/Event.h>
#include <Helix/UI/Engine/Controls.h>

#include <memory>

// The fields: a label and an input showing a value. AbstractBaseField holds what they share: the label, the input,
// the mixed value, the value in slots of numbers, integers and a string (each field's own layout of them, the C#
// generic bases' typed value), the previous value of a change the user made, and when a field edits text or picks
// from a menu, its text input or its popup. The concrete fields name their C# base (CSharpBase); a change the user
// makes reaches the C# side, which sends the ChangeEvent.
namespace helix::ui
{
    class TextInput;
    class PopupMenu;

    HX_CLASS(Namespace = "HelixEngine.UI")
    class AbstractBaseField : public BindableElement
    {
    public:
        using ControlBase = BindableElement;
        static constexpr u32 kSlots = 6;

        HX_FUNCTION(Constructor) AbstractBaseField();
        AbstractBaseField(ElementWorld& world, ElementHandle e);
        ~AbstractBaseField() override;
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() String label() const;
        void SetLabel(StringView value);
        HX_PROPERTY(ReadOnly) Label* labelElement() const;
        HX_PROPERTY() bool showMixedValue() const { return mixed_; }
        void SetShowMixedValue(bool value);
        // Each change of the value by the user, for C++ scripts (valueChanged in their API; C# sends ChangeEvent).
        HX_EVENT(Internal, ApiName = "valueChanged") engine::Event<>& valueChangedNative() { return valueChanged_; }

        // ----------------------------------------------------------------------------- for C++ scripts
        // The typed values of the C++ script API (C# has them on its generic bases): set by a script, shown and told
        // when they changed, as the reference's value setters do.
        void SetIntegerByScript(i32 slot, i64 value);
        void SetNumberByScript(i32 slot, f64 value);
        void SetStringByScript(StringView value);

        // ----------------------------------------------------------------------------- for the C# bases
        HX_FUNCTION(Internal) Element* VisualInputInternal() const;
        HX_FUNCTION(Internal) f64 NumberInternal(i32 slot) const;
        HX_FUNCTION(Internal) i64 IntegerInternal(i32 slot) const;
        HX_FUNCTION(Internal) String StringInternal() const { return string_; }
        HX_FUNCTION(Internal) f64 PreviousNumberInternal(i32 slot) const;
        HX_FUNCTION(Internal) i64 PreviousIntegerInternal(i32 slot) const;
        HX_FUNCTION(Internal) String PreviousStringInternal() const { return previousString_; }
        // The value set without telling (the C# side sends its ChangeEvent); the parts show it.
        HX_FUNCTION(Internal) void SetNumberInternal(i32 slot, f64 value);
        HX_FUNCTION(Internal) void SetIntegerInternal(i32 slot, i64 value);
        HX_FUNCTION(Internal) void SetStringInternal(StringView value);
        // A field's own settings by number (each field's meaning; see the fields).
        HX_FUNCTION(Internal) f64 SettingInternal(i32 which) const;
        HX_FUNCTION(Internal) void SetSettingInternal(i32 which, f64 value);

        // Text input (the text fields): its text, its options by TextOption, its selection.
        HX_FUNCTION(Internal) String TextInternal() const;
        HX_FUNCTION(Internal) void SetTextInternal(StringView text);
        HX_FUNCTION(Internal) i32 TextOptionInternal(i32 option) const;
        HX_FUNCTION(Internal) void SetTextOptionInternal(i32 option, i32 value);
        HX_FUNCTION(Internal) void SelectRangeInternal(i32 cursorIndex, i32 selectionIndex);
        // Popups: the choices (one per line), the shown text, the menu opened under the field.
        HX_FUNCTION(Internal) String ChoicesInternal() const;
        HX_FUNCTION(Internal) void SetChoicesInternal(StringView choices);
        HX_FUNCTION(Internal) void SetShownTextInternal(StringView text);
        HX_FUNCTION(Internal) void ShowMenuInternal();

        static Span<const ControlAttribute> MarkupAttributes();
        // A C++ owner told of the user's changes instead of the C# side (a control's part).
        void SetChangeListener(void (*listener)(void* user, AbstractBaseField& field), void* user)
        {
            onChange_ = listener;
            onChangeUser_ = user;
        }

        // The options of the text input.
        enum TextOption : i32
        {
            ReadOnly = 0,
            Password = 1,
            MaskChar = 2,
            MaxLength = 3,
            Delayed = 4,
            SelectAllOnFocus = 5,
            SelectAllOnMouseUp = 6,
            DoubleClickSelectsWord = 7,
            TripleClickSelectsLine = 8,
            Multiline = 9,
            CursorIndex = 10,
            SelectIndex = 11,
            // Which characters it takes: 0 any, 1 an integer, 2 a positive integer, 3 a number, 4 hex digits.
            Characters = 12,
        };

    protected:
        explicit AbstractBaseField(Atom type);
        // A change the user makes: the value as it was is kept for the C# side's ChangeEvent.
        void RememberPrevious();
        void ChangedByInput();
        // The parts show the value (after a change from either side).
        virtual void ShowValue() {}
        // The text input's text changed (each edit, or when it is committed for a delayed field).
        virtual void OnTextChanged(bool committed);
        // A popup's choice was picked.
        virtual void OnChoice(i32 index);
        TextInput& Text();
        PopupMenu& Popup();

        // A field inside another (a composite's part) tells its owner instead of the C# side.
        void (*onChange_)(void* user, AbstractBaseField& field) = nullptr;
        void* onChangeUser_ = nullptr;
        // A popup's or a radio group's choices changed.
        virtual void OnChoicesChanged() {}
        Array<String> choices_;
        ElementHandle label_, input_;
        f64 numbers_[kSlots] = {}, previousNumbers_[kSlots] = {};
        i64 integers_[kSlots] = {}, previousIntegers_[kSlots] = {};
        String string_, previousString_;
        f64 settings_[8] = {};
        bool mixed_ = false;

    private:
        engine::Event<> valueChanged_;
        friend class TextInput;
        friend class PopupMenu;
        void InitField();
        std::unique_ptr<TextInput> text_;
        std::unique_ptr<PopupMenu> popup_;
        ElementHandle mixedLabel_;
    };

    // A check box with its text: integer slot 0 is the value, the string its text.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseBoolField")
    class Toggle : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) Toggle();
        Toggle(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "valueNative", ApiName = "value") bool valueNative() const { return IntegerInternal(0) != 0; }
        void SetValueNative(bool value) { SetIntegerByScript(0, value ? 1 : 0); }
        HX_PROPERTY(Internal, Name = "textNative", ApiName = "text") String textNative() const { return StringInternal(); }
        void SetTextNative(StringView value) { SetStringInternal(value); }

    protected:
        explicit Toggle(Atom type);
        Toggle(ElementWorld& world, ElementHandle e, bool radio);
        void ShowValue() override;
        // Turned by the user: flips (a radio button only turns on).
        virtual void Turn();
        void InitBool(StringView block, bool radio);
        ElementHandle checkmark_, textPart_;

    private:
        static void PointerDown(void* user, EventSystem& events, UIEvent& e);
        static void PointerUp(void* user, EventSystem& events, UIEvent& e);
        static void Submit(void* user, EventSystem& events, UIEvent& e);
        i32 pointer_ = -1;
    };

    // One of a group of choices: on, the others of its group (its nearest GroupBox or RadioButtonGroup, else its
    // parent's children) go off.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseBoolField")
    class RadioButton : public Toggle
    {
    public:
        using ControlBase = Toggle;
        HX_FUNCTION(Constructor) RadioButton();
        RadioButton(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
        // The others of its group off (it went on).
        void Exclusive();

    protected:
        void Turn() override;
    };

    // Radio buttons of its choices: integer slot 0 is the index of the one on (-1: none).
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseField<int>")
    class RadioButtonGroup : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) RadioButtonGroup();
        RadioButtonGroup(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "valueNative", ApiName = "value") i32 valueNative() const { return i32(IntegerInternal(0)); }
        void SetValueNative(i32 value) { SetIntegerByScript(0, value); }
        // A radio button of it went on.
        void Picked(ElementHandle radio);

    protected:
        void ShowValue() override;
        void OnChoice(i32 index) override;

    private:
        void Rebuild();
        ElementHandle container_;
        Array<ElementHandle> choices_;
    };

    // Buttons of which some are on: integer slot 0 the mask of those on, slot 1 how many buttons count; settings 0
    // multiple selection, 1 an empty selection allowed.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseField<HelixEngine.UI.ToggleButtonGroupState>")
    class ToggleButtonGroup : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) ToggleButtonGroup();
        ToggleButtonGroup(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;
        static Span<const ControlAttribute> MarkupAttributes();

    protected:
        void ShowValue() override;

    private:
        static bool Sync(Element& self, f64 time);
        static void OnButton(void* user, Button& button);
        Array<ElementHandle> Buttons() const;
        ElementHandle container_;
    };

    // A dragger on a tracker between lowValue and highValue (settings 0 low, 1 high, 2 page size, 3 direction
    // (0 horizontal, 1 vertical), 4 inverted, 5 fill, 6 whole numbers, 7 the input field shown). Number slot 0 is the
    // value (a whole number for SliderInt). MinMaxSlider takes a range: number slots 0 and 1.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseSlider<float>")
    class Slider : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) Slider();
        Slider(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "valueNative", ApiName = "value") f32 valueNative() const { return f32(NumberInternal(0)); }
        void SetValueNative(f32 value) { SetNumberByScript(0, value); }
        // The value of a point along the tracker (0..1 of its length).
        f64 ValueAt(f64 k) const;
        void SetByUser(f64 value);

    protected:
        explicit Slider(Atom type, bool whole);
        void ShowValue() override;
        void InitSlider(StringView block, bool whole);
        static void PointerDown(void* user, EventSystem& events, UIEvent& e);
        static void PointerMove(void* user, EventSystem& events, UIEvent& e);
        static void PointerUp(void* user, EventSystem& events, UIEvent& e);
        static void Navigate(void* user, EventSystem& events, UIEvent& e);
        static void Geometry(void* user, EventSystem& events, UIEvent& e);
        f64 Fraction(EventSystem& events, Vector2 position) const;
        ElementHandle tracker_, dragger_, fill_, drag_;
        i32 pointer_ = -1;
        f64 grab_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseSlider<int>")
    class SliderInt : public Slider
    {
    public:
        HX_PROPERTY(Internal, Name = "valueIntNative", ApiName = "value") i32 valueIntNative() const { return i32(NumberInternal(0)); }
        void SetValueIntNative(i32 value) { SetNumberByScript(0, value); }
        using ControlBase = Slider;
        HX_FUNCTION(Constructor) SliderInt();
        SliderInt(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    // Number slots 0 and 1 the range (minValue, maxValue) between settings 0 and 1 (lowLimit, highLimit).
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseField<HelixEngine.Vector2>")
    class MinMaxSlider : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) MinMaxSlider();
        MinMaxSlider(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();

    protected:
        void ShowValue() override;

    private:
        void InitMinMax();
        static void PointerDown(void* user, EventSystem& events, UIEvent& e);
        static void PointerMove(void* user, EventSystem& events, UIEvent& e);
        static void PointerUp(void* user, EventSystem& events, UIEvent& e);
        static void Geometry(void* user, EventSystem& events, UIEvent& e);
        f64 Fraction(EventSystem& events, Vector2 position) const;
        ElementHandle tracker_, range_, minThumb_, maxThumb_;
        i32 pointer_ = -1;
        // What is dragged: 0 the min thumb, 1 the max, 2 the range.
        i32 part_ = 0;
        f64 grab_ = 0;
    };

    // A popup of choices: integer slot 0 the index picked (-1: none), the string its text. DropdownField's choices
    // are strings; EnumField's an enum's names (integer slot 1 its value); MaskField and Mask64Field pick several:
    // integer slot 0 the mask (settings 0 the names' bits given by the C# side).
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.PopupField<string>")
    class DropdownField : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) DropdownField();
        DropdownField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "indexNative", ApiName = "index") i32 indexNative() const { return i32(IntegerInternal(0)); }
        void SetIndexNative(i32 value) { SetIntegerByScript(0, value); }

    protected:
        explicit DropdownField(Atom type);
        void InitPopupField(StringView block);
        void ShowValue() override;
        void OnChoice(i32 index) override;
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseField<System.Enum>")
    class EnumField : public DropdownField
    {
    public:
        using ControlBase = DropdownField;
        HX_FUNCTION(Constructor) EnumField();
        EnumField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseMaskField<int>")
    class MaskField : public DropdownField
    {
    public:
        using ControlBase = DropdownField;
        HX_FUNCTION(Constructor) MaskField();
        MaskField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();

    protected:
        explicit MaskField(Atom type);
        void ShowValue() override;
        void OnChoice(i32 index) override;
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseMaskField<ulong>")
    class Mask64Field : public MaskField
    {
    public:
        using ControlBase = MaskField;
        HX_FUNCTION(Constructor) Mask64Field();
        Mask64Field(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    // ------------------------------------------------------------------------------------------ text fields

    // Text: the string slot is its value (the text input's text).
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextInputBaseField<string>")
    class TextField : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) TextField();
        TextField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "valueNative", ApiName = "value") String valueNative() const { return StringInternal(); }
        void SetValueNative(StringView value) { SetStringByScript(value); }

    protected:
        explicit TextField(Atom type, StringView block, i32 characters);
        void ShowValue() override;
        void OnTextChanged(bool committed) override;
        void InitTextField(StringView block, i32 characters);
    };

    // A number as text, dragged on its label (settings 0: the kind, 1: the drag speed). Integer slot 0 holds the
    // integers (an unsigned one's bits), number slot 0 the floating ones; the string slot the format.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextValueField<int>")
    class IntegerField : public TextField
    {
    public:
        using ControlBase = TextField;
        HX_FUNCTION(Constructor) IntegerField();
        IntegerField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "valueIntNative", ApiName = "value") i32 valueIntNative() const { return i32(IntegerInternal(0)); }
        void SetValueIntNative(i32 value) { SetIntegerByScript(0, value); }
        // The kinds of number.
        enum Kind : i32
        {
            Int = 0,
            Long = 1,
            UInt = 2,
            ULong = 3,
            Float = 4,
            Double = 5,
        };
        Kind kind() const { return Kind(i32(settings_[0])); }
        // The value as text, and from text (false: no number of the kind).
        String Format() const;
        bool Parse(StringView text);
        void SetByUser(f64 number, i64 integer);

    protected:
        IntegerField(Atom type, StringView block, Kind kind);
        void ShowValue() override;
        void OnTextChanged(bool committed) override;

    private:
        void InitNumber(StringView block, Kind kind);
        static void LabelDown(void* user, EventSystem& events, UIEvent& e);
        static void LabelMove(void* user, EventSystem& events, UIEvent& e);
        static void LabelUp(void* user, EventSystem& events, UIEvent& e);
        i32 pointer_ = -1;
        f64 dragStart_ = 0, dragNumber_ = 0;
        i64 dragInteger_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextValueField<long>")
    class LongField : public IntegerField
    {
    public:
        using ControlBase = IntegerField;
        HX_FUNCTION(Constructor) LongField();
        LongField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextValueField<uint>")
    class UnsignedIntegerField : public IntegerField
    {
    public:
        using ControlBase = IntegerField;
        HX_FUNCTION(Constructor) UnsignedIntegerField();
        UnsignedIntegerField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextValueField<ulong>")
    class UnsignedLongField : public IntegerField
    {
    public:
        using ControlBase = IntegerField;
        HX_FUNCTION(Constructor) UnsignedLongField();
        UnsignedLongField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextValueField<float>")
    class FloatField : public IntegerField
    {
    public:
        using ControlBase = IntegerField;
        HX_FUNCTION(Constructor) FloatField();
        FloatField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextValueField<double>")
    class DoubleField : public IntegerField
    {
    public:
        using ControlBase = IntegerField;
        HX_FUNCTION(Constructor) DoubleField();
        DoubleField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    // A hash as 32 hex digits: integer slots 0 and 1 its halves.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.TextInputBaseField<HelixEngine.Hash128>")
    class Hash128Field : public TextField
    {
    public:
        using ControlBase = TextField;
        HX_FUNCTION(Constructor) Hash128Field();
        Hash128Field(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }

    protected:
        void ShowValue() override;
        void OnTextChanged(bool committed) override;
    };

    // Number fields side by side, one per part of the value (settings 0: the parts, 1: integers; the parts' values
    // in the number or integer slots). Vector2Field to BoundsIntField.
    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Vector2, HelixEngine.UI.FloatField, float>")
    class Vector2Field : public AbstractBaseField
    {
    public:
        using ControlBase = AbstractBaseField;
        HX_FUNCTION(Constructor) Vector2Field();
        Vector2Field(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes();
        HX_PROPERTY(Internal, Name = "valueFloatNative", ApiName = "value") f32 valueFloatNative() const { return f32(NumberInternal(0)); }
        void SetValueFloatNative(f32 value) { SetNumberByScript(0, value); }
        // A part changed by the user.
        void PartChanged(ElementHandle part);

    protected:
        // The parts' labels; for bounds the rows' labels (Center and Extent, Position and Size).
        Vector2Field(Atom type, StringView block, Span<const char* const> labels, bool integers, Span<const char* const> rows = {});
        Vector2Field(ElementWorld& world, ElementHandle e, StringView block, Span<const char* const> labels, bool integers,
                     Span<const char* const> rows = {});
        void ShowValue() override;

    private:
        void InitComposite(StringView block, Span<const char* const> labels, bool integers, Span<const char* const> rows);
        Array<ElementHandle> parts_;
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Vector3, HelixEngine.UI.FloatField, float>")
    class Vector3Field : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) Vector3Field();
        Vector3Field(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Vector4, HelixEngine.UI.FloatField, float>")
    class Vector4Field : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) Vector4Field();
        Vector4Field(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Vector2Int, HelixEngine.UI.IntegerField, int>")
    class Vector2IntField : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) Vector2IntField();
        Vector2IntField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Vector3Int, HelixEngine.UI.IntegerField, int>")
    class Vector3IntField : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) Vector3IntField();
        Vector3IntField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Rect, HelixEngine.UI.FloatField, float>")
    class RectField : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) RectField();
        RectField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.RectInt, HelixEngine.UI.IntegerField, int>")
    class RectIntField : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) RectIntField();
        RectIntField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.Bounds, HelixEngine.UI.FloatField, float>")
    class BoundsField : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) BoundsField();
        BoundsField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };

    HX_CLASS(Namespace = "HelixEngine.UI", CSharpBase = "HelixEngine.UI.BaseCompositeField<HelixEngine.BoundsInt, HelixEngine.UI.IntegerField, int>")
    class BoundsIntField : public Vector2Field
    {
    public:
        using ControlBase = Vector2Field;
        HX_FUNCTION(Constructor) BoundsIntField();
        BoundsIntField(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        static Span<const ControlAttribute> MarkupAttributes() { return {}; }
    };
}
