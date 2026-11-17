#include <Helix/UI/Engine/Containers.h>

#include <Helix/Core/Strings/Format.h>
#include <Helix/UI/Engine/ControlParts.h>
#include <Helix/UI/Engine/Panel.h>

#include <cmath>

namespace helix::ui
{
    namespace
    {
        bool BoolAttribute(StringView v, bool& out) { return ParseBoolText(v, out); }

        void DestroyPart(ElementHandle e)
        {
            ElementTree& tree = ControlTree();
            if (!tree.IsAlive(e))
                return;
            EventCallbacks& callbacks = ElementWorld::Main().callbacks();
            tree.ForEachInSubtree(e, [&](ElementHandle x, u32) { callbacks.Clear(x); });
            tree.Destroy(e);
        }
    }

    // ------------------------------------------------------------------------------------------------- Foldout

    Foldout::Foldout() : BindableElement(Intern("Foldout")) { InitFoldout(); }
    Foldout::Foldout(ElementWorld& world, ElementHandle e) : BindableElement(world, e) { InitFoldout(); }
    scripting::TypeId Foldout::HelixType() const { return scripting::TypeOf<Foldout>(); }
    Element* Foldout::contentContainer() { return Element::Of(world(), content_); }
    Toggle* Foldout::toggle() const { return world().tree().IsAlive(toggleHandle_) ? toggle_ : nullptr; }

    void Foldout::InitFoldout()
    {
        AddToClassList("helix-foldout");
        toggle_ = new Toggle();
        toggleHandle_ = toggle_->handle();
        toggle_->AddToClassList("helix-foldout__toggle");
        world().tree().Append(handle(), toggleHandle_);
        // The toggle's parts take the foldout's looks.
        world().tree().ForEachInSubtree(toggleHandle_, [&](ElementHandle e, u32) {
            ElementTree& tree = world().tree();
            if (tree.HasClass(e, Intern("helix-toggle__input")))
                tree.AddClass(e, Intern("helix-foldout__input"));
            if (tree.HasClass(e, Intern("helix-toggle__checkmark")))
                tree.AddClass(e, Intern("helix-foldout__checkmark"));
            if (tree.HasClass(e, Intern("helix-toggle__text")))
                tree.AddClass(e, Intern("helix-foldout__text"));
        });
        toggle_->SetChangeListener(&Foldout::ToggleChanged, this);
        content_ = MakePart(handle(), "helix-foldout__content");
        Show();
    }

    String Foldout::text() const { return toggle_->StringInternal(); }
    void Foldout::SetText(StringView value) { toggle_->SetStringInternal(value); }

    void Foldout::SetValue(bool v)
    {
        if (v == value_)
            return;
        previous_ = value_;
        value_ = v;
        Show();
        NotifyControl(handle(), ControlNotify::ValueChanged);
    }

    void Foldout::SetValueWithoutNotify(bool newValue)
    {
        value_ = newValue;
        Show();
    }

    void Foldout::Show()
    {
        if (toggle_->IntegerInternal(0) != (value_ ? 1 : 0))
            toggle_->SetIntegerInternal(0, value_ ? 1 : 0);
        ShowPart(content_, value_);
        SetPartState(handle(), PseudoStates::Checked, value_);
    }

    void Foldout::ToggleChanged(void* user, AbstractBaseField& field) { static_cast<Foldout*>(user)->SetValue(field.IntegerInternal(0) != 0); }

    Span<const ControlAttribute> Foldout::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"text", [](Element& s, StringView v) { return static_cast<Foldout&>(s).SetText(v), true; }},
            {"value",
             [](Element& s, StringView v) {
                 bool b = false;
                 return BoolAttribute(v, b) && (static_cast<Foldout&>(s).SetValueWithoutNotify(b), true);
             }},
            {"toggle-on-label-click",
             [](Element& s, StringView v) {
                 bool b = false;
                 return BoolAttribute(v, b) && (static_cast<Foldout&>(s).SetToggleOnLabelClick(b), true);
             }},
        };
        return kAttributes;
    }

    // ------------------------------------------------------------------------------------------------ Scroller

    Scroller::Scroller() : Element(Intern("Scroller")) { InitScroller(); }
    Scroller::Scroller(ElementWorld& world, ElementHandle e) : Element(world, e) { InitScroller(); }
    scripting::TypeId Scroller::HelixType() const { return scripting::TypeOf<Scroller>(); }

    void Scroller::InitScroller()
    {
        AddToClassList("helix-scroller");
        ElementTree& tree = world().tree();
        low_ = new RepeatButton();
        low_->AddToClassList("helix-scroller__low-button");
        low_->SetDelay(250);
        low_->SetInterval(30);
        low_->SetFocusable(false);
        slider_ = new Slider();
        slider_->AddToClassList("helix-scroller__slider");
        slider_->SetFocusable(false);
        high_ = new RepeatButton();
        high_->AddToClassList("helix-scroller__high-button");
        high_->SetDelay(250);
        high_->SetInterval(30);
        high_->SetFocusable(false);
        tree.Append(handle(), low_->handle());
        tree.Append(handle(), slider_->handle());
        tree.Append(handle(), high_->handle());
        slider_->SetChangeListener(&Scroller::SliderChanged, this);
        low_->SetClickListener(&Scroller::ButtonClicked, this);
        high_->SetClickListener(&Scroller::ButtonClicked, this);
        slider_->SetSettingInternal(1, 100);
        SetDirection(direction_);
    }

    f32 Scroller::value() const { return f32(slider_->NumberInternal(0)); }
    f32 Scroller::lowValue() const { return f32(slider_->SettingInternal(0)); }
    f32 Scroller::highValue() const { return f32(slider_->SettingInternal(1)); }
    Slider* Scroller::slider() const { return slider_; }
    RepeatButton* Scroller::lowButton() const { return low_; }
    RepeatButton* Scroller::highButton() const { return high_; }

    void Scroller::SetValue(f32 v)
    {
        f32 before = value();
        f32 lo = lowValue() < highValue() ? lowValue() : highValue(), hi = lowValue() < highValue() ? highValue() : lowValue();
        v = v < lo ? lo : v > hi ? hi : v;
        slider_->SetNumberInternal(0, v);
        Changed(before);
    }

    void Scroller::SetLowValue(f32 v)
    {
        slider_->SetSettingInternal(0, v);
        SetValue(value());
    }

    void Scroller::SetHighValue(f32 v)
    {
        slider_->SetSettingInternal(1, v);
        SetValue(value());
    }

    void Scroller::SetDirection(SliderDirection v)
    {
        direction_ = v;
        bool vertical = v == SliderDirection::Vertical;
        SetPartClass(handle(), "helix-scroller--vertical", vertical);
        SetPartClass(handle(), "helix-scroller--horizontal", !vertical);
        slider_->SetSettingInternal(3, vertical ? 1 : 0);
        Adjust(factor_);
    }

    void Scroller::Adjust(f32 factor)
    {
        factor_ = factor < 0.05f ? 0.05f : factor > 1 ? 1 : factor;
        // The dragger as large as the shown part of the content.
        world().tree().ForEachInSubtree(slider_->handle(), [&](ElementHandle e, u32) {
            if (world().tree().HasClass(e, Intern("helix-base-slider__dragger")))
            {
                bool vertical = direction_ == SliderDirection::Vertical;
                SetPartStyle(e, vertical ? "height" : "width", Format("{}%", factor_ * 100).View());
                SetPartStyle(e, vertical ? "width" : "height", "100%");
            }
        });
        slider_->SetNumberInternal(0, slider_->NumberInternal(0));
    }

    void Scroller::ScrollPageUp() { ScrollPageUpBy(1); }
    void Scroller::ScrollPageDown() { ScrollPageDownBy(1); }

    void Scroller::ScrollPageUpBy(f32 factor)
    {
        f32 page = f32(slider_->SettingInternal(2));
        SetValue(value() - factor * (page > 0 ? page : (highValue() - lowValue()) * 0.1f));
    }

    void Scroller::ScrollPageDownBy(f32 factor)
    {
        f32 page = f32(slider_->SettingInternal(2));
        SetValue(value() + factor * (page > 0 ? page : (highValue() - lowValue()) * 0.1f));
    }

    void Scroller::Changed(f32 before)
    {
        if (value() == before)
            return;
        if (listener_)
            listener_(listenerUser_, *this);
        NotifyControl(handle(), ControlNotify::ValueChanged);
    }

    void Scroller::SliderChanged(void* user, AbstractBaseField& field)
    {
        auto* self = static_cast<Scroller*>(user);
        self->Changed(f32(field.PreviousNumberInternal(0)));
    }

    void Scroller::ButtonClicked(void* user, Button& button)
    {
        auto* self = static_cast<Scroller*>(user);
        f32 step = (self->highValue() - self->lowValue()) * 0.02f;
        step = step > 1 ? step : 1;
        self->SetValue(self->value() + (&button == self->low_ ? -step : step));
    }

    Span<const ControlAttribute> Scroller::MarkupAttributes()
    {
        static const StringView kDirections[] = {"Horizontal", "Vertical"};
        static const ControlAttribute kAttributes[] = {
            {"low-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Scroller&>(s).SetLowValue(f32(n)), true);
             }},
            {"high-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Scroller&>(s).SetHighValue(f32(n)), true);
             }},
            {"value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Scroller&>(s).SetValue(f32(n)), true);
             }},
            {"direction",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kDirections, k) && (static_cast<Scroller&>(s).SetDirection(SliderDirection(k)), true);
             }},
        };
        return kAttributes;
    }

    // ---------------------------------------------------------------------------------------------- ScrollView

    ScrollView::ScrollView() : Element(Intern("ScrollView")) { InitScrollView(); }
    ScrollView::ScrollView(ScrollViewMode scrollViewMode) : Element(Intern("ScrollView"))
    {
        InitScrollView();
        SetMode(scrollViewMode);
    }
    ScrollView::ScrollView(ElementWorld& world, ElementHandle e) : Element(world, e) { InitScrollView(); }
    scripting::TypeId ScrollView::HelixType() const { return scripting::TypeOf<ScrollView>(); }
    Element* ScrollView::contentContainer() { return Element::Of(world(), content_); }
    Element* ScrollView::contentViewport() const { return Element::Of(world(), viewport_); }

    void ScrollView::InitScrollView()
    {
        AddToClassList("helix-scroll-view");
        ElementTree& tree = world().tree();
        row_ = MakePart(handle(), "helix-scroll-view__content-and-vertical-scroll-container");
        viewport_ = MakePart(row_, "helix-scroll-view__content-viewport");
        content_ = MakePart(viewport_, "helix-scroll-view__content-container");
        vertical_ = new Scroller();
        vertical_->AddToClassList("helix-scroll-view__vertical-scroller");
        vertical_->SetDirection(SliderDirection::Vertical);
        tree.Append(row_, vertical_->handle());
        horizontal_ = new Scroller();
        horizontal_->AddToClassList("helix-scroll-view__horizontal-scroller");
        horizontal_->SetDirection(SliderDirection::Horizontal);
        tree.Append(handle(), horizontal_->handle());
        vertical_->SetValueListener(&ScrollView::ScrollerChanged, this);
        horizontal_->SetValueListener(&ScrollView::ScrollerChanged, this);
        Listen(*this, viewport_, Events().geometryChanged, &ScrollView::Geometry);
        Listen(*this, content_, Events().geometryChanged, &ScrollView::Geometry);
        Listen(*this, handle(), Events().wheel, &ScrollView::Wheel);
        Listen(*this, viewport_, Events().pointerDown, &ScrollView::PointerDown, TrickleDown::TrickleDown);
        Listen(*this, viewport_, Events().pointerMove, &ScrollView::PointerMove, TrickleDown::TrickleDown);
        Listen(*this, viewport_, Events().pointerUp, &ScrollView::PointerUp, TrickleDown::TrickleDown);
        SetMode(mode_);
    }

    void ScrollView::SetMode(ScrollViewMode v)
    {
        mode_ = v;
        static const char* kModes[] = {"helix-scroll-view--vertical", "helix-scroll-view--horizontal", "helix-scroll-view--vertical-horizontal"};
        for (u32 i = 0; i < 3; i++)
            SetPartClass(handle(), kModes[i], u32(v) == i);
        // The content as wide as the viewport (vertical), as tall (horizontal), or as large as it is (both).
        SetPartStyle(content_, "position", "absolute");
        SetPartStyle(content_, "left", "0");
        SetPartStyle(content_, "top", "0");
        SetPartStyle(content_, "right", v == ScrollViewMode::Vertical ? "0" : "auto");
        SetPartStyle(content_, "bottom", v == ScrollViewMode::Horizontal ? "0" : "auto");
        SetPartStyle(content_, "flex-direction", v == ScrollViewMode::Horizontal ? "row" : "column");
        UpdateScrollers();
    }

    void ScrollView::SetHorizontalScrollerVisibility(ScrollerVisibility v)
    {
        horizontalVisibility_ = v;
        UpdateScrollers();
    }

    void ScrollView::SetVerticalScrollerVisibility(ScrollerVisibility v)
    {
        verticalVisibility_ = v;
        UpdateScrollers();
    }

    Vector2 ScrollView::Limits() const
    {
        Rect view = PartRect(*this, viewport_), content = PartRect(*this, content_);
        f32 w = content.width - view.width, h = content.height - view.height;
        return Vector2(w > 0 ? w : 0, h > 0 ? h : 0);
    }

    void ScrollView::SetScrollOffset(Vector2 v)
    {
        // Clamped unless an elastic drag or its spring back moves it.
        if (touch_ != TouchScrollBehavior::Unrestricted && pointer_ < 0 && lastTime_ < 0)
        {
            Vector2 l = Limits();
            v.x = v.x < 0 ? 0 : v.x > l.x ? l.x : v.x;
            v.y = v.y < 0 ? 0 : v.y > l.y ? l.y : v.y;
        }
        offset_ = v;
        Place();
    }

    void ScrollView::Place()
    {
        SetPartStyle(content_, "translate", Format("{}px {}px", -offset_.x, -offset_.y).View());
        syncing_ = true;
        if (horizontal_->value() != offset_.x)
            horizontal_->SetValue(offset_.x);
        if (vertical_->value() != offset_.y)
            vertical_->SetValue(offset_.y);
        syncing_ = false;
    }

    void ScrollView::UpdateScrollers()
    {
        Rect view = PartRect(*this, viewport_), content = PartRect(*this, content_);
        Vector2 l = Limits();
        bool wantH = mode_ != ScrollViewMode::Vertical, wantV = mode_ != ScrollViewMode::Horizontal;
        auto shown = [](ScrollerVisibility v, bool want, f32 limit) { return v == ScrollerVisibility::AlwaysVisible || (v == ScrollerVisibility::Auto && want && limit > 0); };
        ShowPart(horizontal_->handle(), shown(horizontalVisibility_, wantH, l.x));
        ShowPart(vertical_->handle(), shown(verticalVisibility_, wantV, l.y));
        syncing_ = true;
        horizontal_->SetHighValue(l.x);
        vertical_->SetHighValue(l.y);
        horizontal_->Adjust(content.width > 0 ? view.width / content.width : 1);
        vertical_->Adjust(content.height > 0 ? view.height / content.height : 1);
        syncing_ = false;
        SetScrollOffset(offset_);
    }

    void ScrollView::ScrollTo(Element* child)
    {
        if (!child || !child->alive())
            return;
        Rect view = PartRect(*this, viewport_), r = PartRect(*this, child->handle());
        Vector2 o = offset_;
        // The child's place in the content (the content is moved by the offset).
        f32 top = r.y - view.y, left = r.x - view.x;
        if (top < 0)
            o.y += top;
        else if (top + r.height > view.height)
            o.y += top + r.height - view.height;
        if (left < 0)
            o.x += left;
        else if (left + r.width > view.width)
            o.x += left + r.width - view.width;
        SetScrollOffset(o);
    }

    void ScrollView::Geometry(void* user, EventSystem&, UIEvent&) { static_cast<ScrollView*>(user)->UpdateScrollers(); }

    void ScrollView::ScrollerChanged(void* user, Scroller&)
    {
        auto* self = static_cast<ScrollView*>(user);
        if (self->syncing_)
            return;
        self->SetScrollOffset(Vector2(self->horizontal_->value(), self->vertical_->value()));
    }

    void ScrollView::Wheel(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<ScrollView*>(user);
        Vector2 l = self->Limits();
        Vector2 o = self->offset_;
        bool horizontalOnly = self->mode_ == ScrollViewMode::Horizontal || (l.y <= 0 && l.x > 0);
        f32 dy = e.delta.y * self->wheelSize_, dx = e.delta.x * self->wheelSize_;
        if (horizontalOnly)
            o.x += dy + dx;
        else
            o.y += dy, o.x += dx;
        Vector2 before = self->offset_;
        self->SetScrollOffset(o);
        // A scroll view that moved takes the wheel; one at its end lets an outer one have it.
        if (self->offset_.x != before.x || self->offset_.y != before.y)
            e.StopPropagation();
    }

    void ScrollView::PointerDown(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<ScrollView*>(user);
        // Touches and pens drag the content (a mouse uses the scrollers and the wheel).
        if (e.pointerKind == PointerKind::Mouse)
            return;
        self->pointer_ = e.pointerId;
        self->last_ = e.position;
        self->velocity_ = Vector2(0, 0);
        self->moveTime_ = e.timestamp;
        self->lastTime_ = -1;
    }

    void ScrollView::PointerMove(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<ScrollView*>(user);
        if (e.pointerId != self->pointer_)
            return;
        Vector2 d(self->last_.x - e.position.x, self->last_.y - e.position.y);
        if (!events.HasPointerCapture(self->viewport_, e.pointerId) && std::fabs(d.x) + std::fabs(d.y) > 4)
            events.CapturePointer(self->viewport_, e.pointerId);
        if (self->mode_ == ScrollViewMode::Vertical)
            d.x = 0;
        if (self->mode_ == ScrollViewMode::Horizontal)
            d.y = 0;
        Vector2 o(self->offset_.x + d.x, self->offset_.y + d.y);
        // Elastic: past the edges the content follows by less.
        if (self->touch_ == TouchScrollBehavior::Elastic)
        {
            Vector2 l = self->Limits();
            auto resist = [](f32 v, f32 limit, f32 delta) { return (v < 0 && delta < 0) || (v > limit && delta > 0) ? delta * 0.5f : delta; };
            o = Vector2(self->offset_.x + resist(self->offset_.x, l.x, d.x), self->offset_.y + resist(self->offset_.y, l.y, d.y));
        }
        f64 dt = e.timestamp - self->moveTime_;
        if (dt > 0)
            self->velocity_ = Vector2(f32(d.x / dt), f32(d.y / dt));
        self->moveTime_ = e.timestamp;
        self->last_ = e.position;
        self->SetScrollOffset(o);
    }

    void ScrollView::PointerUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<ScrollView*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        if (events.HasPointerCapture(self->viewport_, e.pointerId))
            events.ReleasePointer(self->viewport_, e.pointerId);
        // Inertia (and the elastic spring back) on the controls' ticks.
        self->lastTime_ = 0;
        StartControlTick(*self, &ScrollView::Inertia);
    }

    bool ScrollView::Inertia(Element& self, f64 time)
    {
        auto& v = static_cast<ScrollView&>(self);
        if (v.pointer_ >= 0)
            return false;
        if (v.lastTime_ <= 0)
        {
            v.lastTime_ = time;
            return true;
        }
        f32 dt = f32(time - v.lastTime_);
        v.lastTime_ = time;
        Vector2 o(v.offset_.x + v.velocity_.x * dt, v.offset_.y + v.velocity_.y * dt);
        // The velocity falls to the deceleration rate's share each second.
        f32 keep = std::pow(v.deceleration_ > 0 ? v.deceleration_ : 0.0001f, dt);
        v.velocity_ = Vector2(v.velocity_.x * keep, v.velocity_.y * keep);
        Vector2 l = v.Limits();
        bool outside = false;
        if (v.touch_ == TouchScrollBehavior::Elastic)
        {
            // Back to the edge within about the elasticity's time.
            f32 k = v.elasticity_ > 0 ? 1 - std::exp(-dt / v.elasticity_) : 1;
            auto spring = [&](f32 x, f32 limit, f32& vel) {
                if (x < 0 || x > limit)
                {
                    outside = true;
                    vel = 0;
                    f32 edge = x < 0 ? 0 : limit;
                    f32 next = x + (edge - x) * k;
                    return std::fabs(next - edge) < 0.5f ? edge : next;
                }
                return x;
            };
            o.x = spring(o.x, l.x, v.velocity_.x);
            o.y = spring(o.y, l.y, v.velocity_.y);
        }
        bool moving = std::fabs(v.velocity_.x) + std::fabs(v.velocity_.y) > 5 || outside;
        if (!moving)
        {
            v.lastTime_ = -1;
            v.SetScrollOffset(o);
            return false;
        }
        v.offset_ = o;
        v.Place();
        return true;
    }

    Span<const ControlAttribute> ScrollView::MarkupAttributes()
    {
        static const StringView kModes[] = {"Vertical", "Horizontal", "VerticalAndHorizontal"};
        static const StringView kVisibility[] = {"Auto", "AlwaysVisible", "Hidden"};
        static const StringView kTouch[] = {"Unrestricted", "Elastic", "Clamped"};
        static const ControlAttribute kAttributes[] = {
            {"mode",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kModes, k) && (static_cast<ScrollView&>(s).SetMode(ScrollViewMode(k)), true);
             }},
            {"horizontal-scroller-visibility",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kVisibility, k) && (static_cast<ScrollView&>(s).SetHorizontalScrollerVisibility(ScrollerVisibility(k)), true);
             }},
            {"vertical-scroller-visibility",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kVisibility, k) && (static_cast<ScrollView&>(s).SetVerticalScrollerVisibility(ScrollerVisibility(k)), true);
             }},
            {"touch-scroll-type",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kTouch, k) && (static_cast<ScrollView&>(s).SetTouchScrollBehavior(TouchScrollBehavior(k)), true);
             }},
            {"elasticity",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<ScrollView&>(s).SetElasticity(f32(n)), true);
             }},
            {"scroll-deceleration-rate",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<ScrollView&>(s).SetScrollDecelerationRate(f32(n)), true);
             }},
            {"mouse-wheel-scroll-size",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<ScrollView&>(s).SetMouseWheelScrollSize(f32(n)), true);
             }},
        };
        return kAttributes;
    }

    // ----------------------------------------------------------------------------------------------------- Tab

    Tab::Tab() : Element(Intern("Tab")) { InitTab(); }
    Tab::Tab(StringView label) : Element(Intern("Tab"))
    {
        InitTab();
        SetLabel(label);
    }
    Tab::Tab(ElementWorld& world, ElementHandle e) : Element(world, e) { InitTab(); }
    Tab::~Tab()
    {
        // Its header goes with it (it lives in a view's header row).
        if (world().tree().IsAlive(header_) && !world().tree().IsAlive(handle()))
            DestroyPart(header_);
    }
    scripting::TypeId Tab::HelixType() const { return scripting::TypeOf<Tab>(); }
    Element* Tab::contentContainer() { return Element::Of(world(), content_); }
    Element* Tab::tabHeader() const { return Element::Of(world(), header_); }

    void Tab::InitTab()
    {
        AddToClassList("helix-tab");
        header_ = MakePart(ElementHandle(), "helix-tab__header");
        headerIcon_ = MakePart(header_, "helix-tab__header-image");
        headerLabel_ = MakePart(header_, "helix-text-element helix-label helix-tab__header-label", "Label");
        ElementHandle underline = MakePart(header_, "helix-tab__header-underline");
        world().tree().SetPicking(headerIcon_, PickingMode::Ignore);
        world().tree().SetPicking(headerLabel_, PickingMode::Ignore);
        world().tree().SetPicking(underline, PickingMode::Ignore);
        ShowPart(headerIcon_, false);
        content_ = MakePart(handle(), "helix-tab__content-container");
    }

    String Tab::label() const { return String(world().tree().Text(headerLabel_)); }
    void Tab::SetLabel(StringView value) { world().tree().SetText(headerLabel_, value); }

    void Tab::SetIconImage(engine::Texture2D* value)
    {
        icon_ = engine::PPtr<engine::Texture2D>(value);
        if (Element* icon = Element::Of(world(), headerIcon_))
            icon->SetInlineStyleTexture("background-image", value, Vector2(0, 0));
        ShowPart(headerIcon_, value != nullptr);
    }

    void Tab::SetShown(bool shown)
    {
        ShowPart(handle(), shown);
        SetPartState(header_, PseudoStates::Checked, shown);
    }

    Span<const ControlAttribute> Tab::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"label", [](Element& s, StringView v) { return static_cast<Tab&>(s).SetLabel(v), true; }},
        };
        return kAttributes;
    }

    // ------------------------------------------------------------------------------------------------- TabView

    TabView::TabView() : Element(Intern("TabView")) { InitTabView(); }
    TabView::TabView(ElementWorld& world, ElementHandle e) : Element(world, e) { InitTabView(); }
    scripting::TypeId TabView::HelixType() const { return scripting::TypeOf<TabView>(); }
    Element* TabView::contentContainer() { return Element::Of(world(), content_); }

    void TabView::InitTabView()
    {
        AddToClassList("helix-tab-view");
        header_ = MakePart(handle(), "helix-tab-view__header-container");
        content_ = MakePart(handle(), "helix-tab-view__content-container");
        StartControlTick(*this, &TabView::Sync);
    }

    Array<ElementHandle> TabView::Tabs() const
    {
        Array<ElementHandle> out;
        Atom tab = Intern("helix-tab");
        for (ElementHandle c = world().tree().FirstChild(content_); c.IsValid(); c = world().tree().NextSibling(c))
            if (world().tree().HasClass(c, tab))
                out.Push(c);
        return out;
    }

    Tab* TabView::GetTab(i32 index) const
    {
        Array<ElementHandle> tabs = Tabs();
        return index >= 0 && index < i32(tabs.Size()) ? static_cast<Tab*>(Element::Of(world(), tabs[usize(index)])) : nullptr;
    }

    Element* TabView::GetTabHeader(i32 index) const
    {
        Tab* t = GetTab(index);
        return t ? t->tabHeader() : nullptr;
    }

    Tab* TabView::activeTab() const { return GetTab(selected_); }
    Tab* TabView::PreviousTabInternal() const { return world().tree().IsAlive(previous_) ? static_cast<Tab*>(Element::Of(world(), previous_)) : nullptr; }

    void TabView::SetActiveTab(Tab* tab)
    {
        Array<ElementHandle> tabs = Tabs();
        for (usize i = 0; i < tabs.Size(); i++)
            if (tab && tabs[i] == tab->handle())
                Select(i32(i), true);
    }

    void TabView::SetSelectedTabIndex(i32 index) { Select(index, true); }

    void TabView::Select(i32 index, bool notify)
    {
        Array<ElementHandle> tabs = Tabs();
        if (tabs.Empty())
        {
            selected_ = index;
            return;
        }
        index = index < 0 ? 0 : index >= i32(tabs.Size()) ? i32(tabs.Size()) - 1 : index;
        if (index == selected_ && known_.Size() == tabs.Size())
        {
            Show();
            return;
        }
        previous_ = selected_ >= 0 && selected_ < i32(tabs.Size()) ? tabs[usize(selected_)] : ElementHandle();
        selected_ = index;
        Show();
        if (notify)
            NotifyControl(handle(), ControlNotify::Selected);
    }

    void TabView::Show()
    {
        Array<ElementHandle> tabs = Tabs();
        for (usize i = 0; i < tabs.Size(); i++)
            if (auto* t = static_cast<Tab*>(Element::Of(world(), tabs[i])))
                t->SetShown(i32(i) == selected_);
    }

    bool TabView::Sync(Element& self, f64)
    {
        // The headers of the tabs added since in the header row, in the tabs' order.
        auto& v = static_cast<TabView&>(self);
        ElementTree& tree = v.world().tree();
        Array<ElementHandle> tabs = v.Tabs();
        bool same = tabs.Size() == v.known_.Size();
        for (usize i = 0; same && i < tabs.Size(); i++)
            same = tabs[i] == v.known_[i];
        if (same)
            return true;
        while (tree.FirstChild(v.header_).IsValid())
            tree.Detach(tree.FirstChild(v.header_));
        for (ElementHandle t : tabs)
            if (auto* tab = static_cast<Tab*>(Element::Of(v.world(), t)))
            {
                tree.Append(v.header_, tab->headerHandle());
                v.world().callbacks().Register(tab->headerHandle(), Events().pointerDown, &TabView::HeaderDown, &v, TrickleDown::NoTrickleDown);
            }
        v.known_ = tabs;
        if (Panel* p = v.panel())
            p->AttachTypes(v.header_);
        v.Select(v.selected_, false);
        return true;
    }

    void TabView::HeaderDown(void* user, EventSystem&, UIEvent& e)
    {
        auto* v = static_cast<TabView*>(user);
        if (e.button != 0 || !v->enabledInHierarchy())
            return;
        for (usize i = 0; i < v->known_.Size(); i++)
            if (auto* t = static_cast<Tab*>(Element::Of(v->world(), v->known_[i])))
                if (t->headerHandle() == e.currentTarget)
                {
                    v->Select(i32(i), true);
                    e.StopPropagation();
                    return;
                }
    }

    Span<const ControlAttribute> TabView::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"selected-tab-index",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<TabView&>(s).Select(i32(n), false), true);
             }},
        };
        return kAttributes;
    }

    // ---------------------------------------------------------------------------------------- TwoPaneSplitView

    TwoPaneSplitView::TwoPaneSplitView() : Element(Intern("TwoPaneSplitView")) { InitSplit(); }
    TwoPaneSplitView::TwoPaneSplitView(i32 fixedPaneIndex, f32 fixedPaneStartDimension, TwoPaneSplitViewOrientation orientation)
        : Element(Intern("TwoPaneSplitView"))
    {
        InitSplit();
        fixedIndex_ = fixedPaneIndex;
        initial_ = fixedPaneStartDimension;
        SetOrientation(orientation);
    }
    TwoPaneSplitView::TwoPaneSplitView(ElementWorld& world, ElementHandle e) : Element(world, e) { InitSplit(); }
    scripting::TypeId TwoPaneSplitView::HelixType() const { return scripting::TypeOf<TwoPaneSplitView>(); }
    Element* TwoPaneSplitView::contentContainer() { return Element::Of(world(), content_); }

    void TwoPaneSplitView::InitSplit()
    {
        AddToClassList("helix-two-pane-split-view");
        content_ = MakePart(handle(), "helix-two-pane-split-view__content-container");
        anchor_ = MakePart(handle(), "helix-two-pane-split-view__dragline-anchor");
        line_ = MakePart(anchor_, "helix-two-pane-split-view__dragline");
        Listen(*this, anchor_, Events().pointerDown, &TwoPaneSplitView::AnchorDown);
        Listen(*this, anchor_, Events().pointerMove, &TwoPaneSplitView::AnchorMove);
        Listen(*this, anchor_, Events().pointerUp, &TwoPaneSplitView::AnchorUp);
        StartControlTick(*this, &TwoPaneSplitView::Sync);
        SetOrientation(orientation_);
    }

    ElementHandle TwoPaneSplitView::Pane(i32 index) const
    {
        ElementTree& tree = world().tree();
        ElementHandle c = tree.FirstChild(content_);
        for (i32 i = 0; i < index && c.IsValid(); i++)
            c = tree.NextSibling(c);
        return c;
    }

    Element* TwoPaneSplitView::fixedPane() const
    {
        ElementHandle p = Pane(fixedIndex_);
        return p.IsValid() ? Element::Of(world(), p) : nullptr;
    }

    Element* TwoPaneSplitView::flexedPane() const
    {
        ElementHandle p = Pane(fixedIndex_ == 0 ? 1 : 0);
        return p.IsValid() ? Element::Of(world(), p) : nullptr;
    }

    void TwoPaneSplitView::SetFixedPaneIndex(i32 v)
    {
        fixedIndex_ = v == 1 ? 1 : 0;
        Layout();
    }

    void TwoPaneSplitView::SetFixedPaneInitialDimension(f32 v)
    {
        initial_ = v;
        size_ = -1;
        Layout();
    }

    void TwoPaneSplitView::SetOrientation(TwoPaneSplitViewOrientation v)
    {
        orientation_ = v;
        bool vertical = v == TwoPaneSplitViewOrientation::Vertical;
        SetPartClass(handle(), "helix-two-pane-split-view--horizontal", !vertical);
        SetPartClass(handle(), "helix-two-pane-split-view--vertical", vertical);
        SetPartStyle(content_, "flex-direction", vertical ? "column" : "row");
        Layout();
    }

    void TwoPaneSplitView::CollapseChild(i32 index)
    {
        collapsed_ = index == 1 ? 1 : 0;
        Layout();
    }

    void TwoPaneSplitView::UnCollapse()
    {
        collapsed_ = -1;
        Layout();
    }

    void TwoPaneSplitView::Layout()
    {
        bool vertical = orientation_ == TwoPaneSplitViewOrientation::Vertical;
        if (size_ < 0)
            size_ = initial_;
        StringView side = vertical ? "height" : "width";
        for (i32 i = 0; i < 2; i++)
        {
            ElementHandle p = Pane(i);
            if (!p.IsValid())
                continue;
            bool fixed = i == fixedIndex_;
            ShowPart(p, collapsed_ != i);
            SetPartStyle(p, "flex-grow", fixed || collapsed_ >= 0 ? (collapsed_ >= 0 && collapsed_ != i ? "1" : "0") : "1");
            SetPartStyle(p, "flex-shrink", fixed ? "0" : "1");
            SetPartStyle(p, side, fixed && collapsed_ < 0 ? Format("{}px", size_).View() : StringView("auto"));
            SetPartStyle(p, vertical ? "width" : "height", "auto");
        }
        ShowPart(anchor_, collapsed_ < 0 && Pane(1).IsValid());
        // The anchor on the line between the panes.
        Rect content = PartRect(*this, content_), first = PartRect(*this, Pane(0));
        f32 at = vertical ? first.height : first.width;
        if (at <= 0)
            at = fixedIndex_ == 0 ? size_ : (vertical ? content.height : content.width) - size_;
        SetPartStyle(anchor_, vertical ? "top" : "left", Format("{}px", at).View());
        SetPartStyle(anchor_, vertical ? "left" : "top", "0");
        SetPartStyle(anchor_, vertical ? "right" : "bottom", "0");
        SetPartStyle(anchor_, vertical ? "bottom" : "right", "auto");
    }

    bool TwoPaneSplitView::Sync(Element& self, f64)
    {
        static_cast<TwoPaneSplitView&>(self).Layout();
        return true;
    }

    void TwoPaneSplitView::AnchorDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<TwoPaneSplitView*>(user);
        if (e.button != 0)
            return;
        self->pointer_ = e.pointerId;
        bool vertical = self->orientation_ == TwoPaneSplitViewOrientation::Vertical;
        self->grab_ = vertical ? e.position.y : e.position.x;
        self->grabSize_ = self->size_;
        events.CapturePointer(self->anchor_, e.pointerId);
        SetPartState(self->anchor_, PseudoStates::Active, true);
        e.StopPropagation();
    }

    void TwoPaneSplitView::AnchorMove(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<TwoPaneSplitView*>(user);
        if (e.pointerId != self->pointer_)
            return;
        bool vertical = self->orientation_ == TwoPaneSplitViewOrientation::Vertical;
        f32 d = (vertical ? e.position.y : e.position.x) - self->grab_;
        Rect content = PartRect(*self, self->content_);
        f32 total = vertical ? content.height : content.width;
        f32 size = self->grabSize_ + (self->fixedIndex_ == 0 ? d : -d);
        self->size_ = size < 0 ? 0 : (total > 0 && size > total ? total : size);
        self->Layout();
    }

    void TwoPaneSplitView::AnchorUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<TwoPaneSplitView*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        events.ReleasePointer(self->anchor_, e.pointerId);
        SetPartState(self->anchor_, PseudoStates::Active, false);
    }

    Span<const ControlAttribute> TwoPaneSplitView::MarkupAttributes()
    {
        static const StringView kOrientations[] = {"Horizontal", "Vertical"};
        static const ControlAttribute kAttributes[] = {
            {"fixed-pane-index",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<TwoPaneSplitView&>(s).SetFixedPaneIndex(i32(n)), true);
             }},
            {"fixed-pane-initial-dimension",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<TwoPaneSplitView&>(s).SetFixedPaneInitialDimension(f32(n)), true);
             }},
            {"orientation",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kOrientations, k) && (static_cast<TwoPaneSplitView&>(s).SetOrientation(TwoPaneSplitViewOrientation(k)), true);
             }},
        };
        return kAttributes;
    }
}
