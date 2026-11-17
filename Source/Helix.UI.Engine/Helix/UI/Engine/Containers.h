#pragma once

#include <Helix/Engine/Event/Event.h>
#include <Helix/UI/Engine/Fields.h>

// The controls that hold other elements: a foldout, scrollers and the scroll view, tabs, a split view.
namespace helix::ui
{
    // A toggle that shows or hides its content.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class Foldout : public BindableElement
    {
    public:
        // Each change of the value by the user, for C++ scripts (valueChanged in their API).
        HX_EVENT(Internal, ApiName = "valueChanged") engine::Event<>& valueChangedNative() { return valueChangedEvent_; }
        using ControlBase = BindableElement;
        HX_FUNCTION(Constructor) Foldout();
        Foldout(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;

        HX_PROPERTY() String text() const;
        void SetText(StringView value);
        HX_PROPERTY() bool value() const { return value_; }
        void SetValue(bool v);
        HX_FUNCTION() void SetValueWithoutNotify(bool newValue);
        HX_PROPERTY() bool toggleOnLabelClick() const { return toggleOnLabelClick_; }
        void SetToggleOnLabelClick(bool v) { toggleOnLabelClick_ = v; }
        // The value before the last change (the C# side's ChangeEvent).
        HX_FUNCTION(Internal) bool PreviousValueInternal() const { return previous_; }
        HX_PROPERTY(ReadOnly) Toggle* toggle() const;
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitFoldout();
        void Show();
        static void ToggleChanged(void* user, AbstractBaseField& field);
        Toggle* toggle_ = nullptr;
        ElementHandle toggleHandle_, content_;
        bool value_ = true, previous_ = true, toggleOnLabelClick_ = true;
        engine::Event<> valueChangedEvent_;
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class SliderDirection : i32
    {
        Horizontal = 0,
        Vertical = 1,
    };

    // A slider between two repeat buttons: the value from lowValue to highValue, the dragger as large as the part
    // of the content shown (Adjust).
    HX_CLASS(Namespace = "HelixEngine.UI")
    class Scroller : public Element
    {
    public:
        // Each change of the value, for C++ scripts (valueChanged in their API).
        HX_EVENT(Internal, ApiName = "valueChanged") engine::Event<>& valueChangedNative() { return valueChangedEvent_; }
        using ControlBase = Element;
        HX_FUNCTION(Constructor) Scroller();
        Scroller(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;

        HX_PROPERTY() f32 value() const;
        void SetValue(f32 v);
        HX_PROPERTY() f32 lowValue() const;
        void SetLowValue(f32 v);
        HX_PROPERTY() f32 highValue() const;
        void SetHighValue(f32 v);
        HX_PROPERTY() SliderDirection direction() const { return direction_; }
        void SetDirection(SliderDirection v);
        HX_PROPERTY(ReadOnly) Slider* slider() const;
        HX_PROPERTY(ReadOnly) RepeatButton* lowButton() const;
        HX_PROPERTY(ReadOnly) RepeatButton* highButton() const;
        // The dragger's size as the factor (0..1) of the tracker.
        HX_FUNCTION() void Adjust(f32 factor);
        HX_FUNCTION() void ScrollPageUp();
        HX_FUNCTION() void ScrollPageDown();
        HX_FUNCTION(Name = "ScrollPageUp") void ScrollPageUpBy(f32 factor);
        HX_FUNCTION(Name = "ScrollPageDown") void ScrollPageDownBy(f32 factor);
        // A C++ owner told of each change (the scroll view).
        void SetValueListener(void (*listener)(void* user, Scroller& s), void* user)
        {
            listener_ = listener;
            listenerUser_ = user;
        }
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitScroller();
        void Changed(f32 before);
        static void SliderChanged(void* user, AbstractBaseField& field);
        static void ButtonClicked(void* user, Button& button);
        SliderDirection direction_ = SliderDirection::Vertical;
        Slider* slider_ = nullptr;
        RepeatButton *low_ = nullptr, *high_ = nullptr;
        f32 factor_ = 1;
        void (*listener_)(void* user, Scroller& s) = nullptr;
        void* listenerUser_ = nullptr;
        engine::Event<> valueChangedEvent_;
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class ScrollViewMode : i32
    {
        Vertical = 0,
        Horizontal = 1,
        VerticalAndHorizontal = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class ScrollerVisibility : i32
    {
        Auto = 0,
        AlwaysVisible = 1,
        Hidden = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class TouchScrollBehavior : i32
    {
        Unrestricted = 0,
        Elastic = 1,
        Clamped = 2,
    };

    // Its content in a viewport, moved by its scrollers, the wheel and touch drags (with inertia, and elastic at the
    // edges).
    HX_CLASS(Namespace = "HelixEngine.UI")
    class ScrollView : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) ScrollView();
        HX_FUNCTION(Constructor) explicit ScrollView(ScrollViewMode scrollViewMode);
        ScrollView(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;

        HX_PROPERTY() ScrollViewMode mode() const { return mode_; }
        void SetMode(ScrollViewMode v);
        HX_PROPERTY() Vector2 scrollOffset() const { return offset_; }
        void SetScrollOffset(Vector2 v);
        HX_PROPERTY() ScrollerVisibility horizontalScrollerVisibility() const { return horizontalVisibility_; }
        void SetHorizontalScrollerVisibility(ScrollerVisibility v);
        HX_PROPERTY() ScrollerVisibility verticalScrollerVisibility() const { return verticalVisibility_; }
        void SetVerticalScrollerVisibility(ScrollerVisibility v);
        HX_PROPERTY() TouchScrollBehavior touchScrollBehavior() const { return touch_; }
        void SetTouchScrollBehavior(TouchScrollBehavior v) { touch_ = v; }
        HX_PROPERTY() f32 elasticity() const { return elasticity_; }
        void SetElasticity(f32 v) { elasticity_ = v; }
        HX_PROPERTY() f32 scrollDecelerationRate() const { return deceleration_; }
        void SetScrollDecelerationRate(f32 v) { deceleration_ = v; }
        HX_PROPERTY() f32 mouseWheelScrollSize() const { return wheelSize_; }
        void SetMouseWheelScrollSize(f32 v) { wheelSize_ = v; }
        HX_PROPERTY() f32 horizontalPageSize() const { return horizontalPage_; }
        void SetHorizontalPageSize(f32 v) { horizontalPage_ = v; }
        HX_PROPERTY() f32 verticalPageSize() const { return verticalPage_; }
        void SetVerticalPageSize(f32 v) { verticalPage_ = v; }
        HX_PROPERTY(ReadOnly) Element* contentViewport() const;
        HX_PROPERTY(ReadOnly) Scroller* horizontalScroller() const { return horizontal_; }
        HX_PROPERTY(ReadOnly) Scroller* verticalScroller() const { return vertical_; }
        // The offset that shows the child (nearest edge) in the viewport.
        HX_FUNCTION() void ScrollTo(Element* child);
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitScrollView();
        void Place();
        void UpdateScrollers();
        Vector2 Limits() const;
        static void Geometry(void* user, EventSystem& events, UIEvent& e);
        static void Wheel(void* user, EventSystem& events, UIEvent& e);
        static void PointerDown(void* user, EventSystem& events, UIEvent& e);
        static void PointerMove(void* user, EventSystem& events, UIEvent& e);
        static void PointerUp(void* user, EventSystem& events, UIEvent& e);
        static void ScrollerChanged(void* user, Scroller& s);
        static bool Inertia(Element& self, f64 time);
        ScrollViewMode mode_ = ScrollViewMode::Vertical;
        ScrollerVisibility horizontalVisibility_ = ScrollerVisibility::Auto, verticalVisibility_ = ScrollerVisibility::Auto;
        TouchScrollBehavior touch_ = TouchScrollBehavior::Clamped;
        f32 elasticity_ = 0.1f, deceleration_ = 0.135f, wheelSize_ = 18, horizontalPage_ = -1, verticalPage_ = -1;
        Vector2 offset_ = Vector2(0, 0), velocity_ = Vector2(0, 0), last_ = Vector2(0, 0);
        ElementHandle row_, viewport_, content_;
        Scroller *horizontal_ = nullptr, *vertical_ = nullptr;
        i32 pointer_ = -1;
        f64 lastTime_ = -1, moveTime_ = 0;
        bool syncing_ = false;
    };

    // A tab of a TabView: its header (in the view's header row) and its content.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class Tab : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) Tab();
        HX_FUNCTION(Constructor) explicit Tab(StringView label);
        Tab(ElementWorld& world, ElementHandle e);
        ~Tab() override;
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;

        HX_PROPERTY() String label() const;
        void SetLabel(StringView value);
        HX_PROPERTY() engine::Texture2D* iconImage() const { return icon_.Get(); }
        void SetIconImage(engine::Texture2D* value);
        HX_PROPERTY(ReadOnly) Element* tabHeader() const;
        ElementHandle headerHandle() const { return header_; }
        void SetShown(bool shown);
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitTab();
        ElementHandle header_, headerLabel_, headerIcon_, content_;
        engine::PPtr<engine::Texture2D> icon_;
    };

    // Tabs, one shown: its tabs' headers in a row, the selected one's content under them.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class TabView : public Element
    {
    public:
        // Each tab picked, for C++ scripts (activeTabChanged in their API).
        HX_EVENT(Internal, ApiName = "activeTabChanged") engine::Event<>& activeTabChangedNative() { return activeTabChangedEvent_; }
        using ControlBase = Element;
        HX_FUNCTION(Constructor) TabView();
        TabView(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;

        HX_PROPERTY() i32 selectedTabIndex() const { return selected_; }
        void SetSelectedTabIndex(i32 index);
        HX_PROPERTY() Tab* activeTab() const;
        void SetActiveTab(Tab* tab);
        // The tab before the last change (the C# side's activeTabChanged).
        HX_FUNCTION(Internal) Tab* PreviousTabInternal() const;
        HX_FUNCTION() Tab* GetTab(i32 index) const;
        HX_FUNCTION() Element* GetTabHeader(i32 index) const;
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitTabView();
        Array<ElementHandle> Tabs() const;
        void Show();
        void Select(i32 index, bool notify);
        static bool Sync(Element& self, f64 time);
        static void HeaderDown(void* user, EventSystem& events, UIEvent& e);
        ElementHandle header_, content_;
        Array<ElementHandle> known_;
        i32 selected_ = 0;
        ElementHandle previous_;
        engine::Event<> activeTabChangedEvent_;
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class TwoPaneSplitViewOrientation : i32
    {
        Horizontal = 0,
        Vertical = 1,
    };

    // Two panes side by side (or one over the other): one of fixed size, the other filling the rest; the line
    // between them dragged to resize.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class TwoPaneSplitView : public Element
    {
    public:
        using ControlBase = Element;
        HX_FUNCTION(Constructor) TwoPaneSplitView();
        HX_FUNCTION(Constructor) TwoPaneSplitView(i32 fixedPaneIndex, f32 fixedPaneStartDimension, TwoPaneSplitViewOrientation orientation);
        TwoPaneSplitView(ElementWorld& world, ElementHandle e);
        scripting::TypeId HelixType() const override;
        Element* contentContainer() override;

        HX_PROPERTY() i32 fixedPaneIndex() const { return fixedIndex_; }
        void SetFixedPaneIndex(i32 v);
        HX_PROPERTY() f32 fixedPaneInitialDimension() const { return initial_; }
        void SetFixedPaneInitialDimension(f32 v);
        HX_PROPERTY() TwoPaneSplitViewOrientation orientation() const { return orientation_; }
        void SetOrientation(TwoPaneSplitViewOrientation v);
        HX_PROPERTY(ReadOnly) Element* fixedPane() const;
        HX_PROPERTY(ReadOnly) Element* flexedPane() const;
        HX_FUNCTION() void CollapseChild(i32 index);
        HX_FUNCTION() void UnCollapse();
        static Span<const ControlAttribute> MarkupAttributes();

    private:
        void InitSplit();
        void Layout();
        ElementHandle Pane(i32 index) const;
        static bool Sync(Element& self, f64 time);
        static void AnchorDown(void* user, EventSystem& events, UIEvent& e);
        static void AnchorMove(void* user, EventSystem& events, UIEvent& e);
        static void AnchorUp(void* user, EventSystem& events, UIEvent& e);
        ElementHandle content_, anchor_, line_;
        i32 fixedIndex_ = 0, collapsed_ = -1, pointer_ = -1;
        f32 initial_ = 100, size_ = -1, grab_ = 0, grabSize_ = 0;
        TwoPaneSplitViewOrientation orientation_ = TwoPaneSplitViewOrientation::Horizontal;
    };
}
