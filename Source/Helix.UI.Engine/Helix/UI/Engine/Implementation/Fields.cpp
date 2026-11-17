#include <Helix/UI/Engine/Fields.h>

#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Math/Hash128.h>
#include <Helix/UI/Engine/ControlParts.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/TextInput.h>

#include <cmath>
#include <limits>

namespace helix::ui
{
    namespace
    {
        i32 Slot(i32 s) { return s < 0 ? 0 : s >= i32(AbstractBaseField::kSlots) ? i32(AbstractBaseField::kSlots) - 1 : s; }

        // A list from markup: items separated by commas.
        Array<String> SplitList(StringView text, char separator)
        {
            Array<String> out;
            usize at = 0;
            while (at <= text.Size())
            {
                usize end = text.Find(separator, at);
                if (end == StringView::npos)
                    end = text.Size();
                StringView item = text.Substr(at, end - at);
                if (separator == ',')
                    item = item.Trimmed();
                if (!item.Empty() || end < text.Size())
                    out.Push(String(item));
                at = end + 1;
            }
            return out;
        }
    }

    // ---------------------------------------------------------------------------------------- AbstractBaseField

    AbstractBaseField::AbstractBaseField() : BindableElement(Intern("AbstractBaseField")) { InitField(); }
    AbstractBaseField::AbstractBaseField(Atom type) : BindableElement(type) { InitField(); }
    AbstractBaseField::AbstractBaseField(ElementWorld& world, ElementHandle e) : BindableElement(world, e) { InitField(); }
    AbstractBaseField::~AbstractBaseField() = default;
    scripting::TypeId AbstractBaseField::HelixType() const { return scripting::TypeOf<AbstractBaseField>(); }

    void AbstractBaseField::InitField()
    {
        AddToClassList("helix-base-field");
        label_ = MakePart(handle(), "helix-text-element helix-label helix-base-field__label", "Label");
        input_ = MakePart(handle(), "helix-base-field__input");
        SetLabel("");
    }

    String AbstractBaseField::label() const { return String(world().tree().Text(label_)); }

    void AbstractBaseField::SetLabel(StringView value)
    {
        world().tree().SetText(label_, value);
        ShowPart(label_, !value.Empty());
        SetPartClass(handle(), "helix-base-field--no-label", value.Empty());
    }

    Label* AbstractBaseField::labelElement() const { return static_cast<Label*>(Element::Of(world(), label_)); }

    void AbstractBaseField::SetShowMixedValue(bool value)
    {
        mixed_ = value;
        SetPartClass(input_, "helix-base-field__input--mixed-value", value);
        ShowValue();
    }

    Element* AbstractBaseField::VisualInputInternal() const { return Element::Of(world(), input_); }
    f64 AbstractBaseField::NumberInternal(i32 slot) const { return numbers_[Slot(slot)]; }
    i64 AbstractBaseField::IntegerInternal(i32 slot) const { return integers_[Slot(slot)]; }
    f64 AbstractBaseField::PreviousNumberInternal(i32 slot) const { return previousNumbers_[Slot(slot)]; }
    i64 AbstractBaseField::PreviousIntegerInternal(i32 slot) const { return previousIntegers_[Slot(slot)]; }

    void AbstractBaseField::SetNumberInternal(i32 slot, f64 value)
    {
        numbers_[Slot(slot)] = value;
        ShowValue();
    }

    void AbstractBaseField::SetIntegerInternal(i32 slot, i64 value)
    {
        integers_[Slot(slot)] = value;
        ShowValue();
    }

    void AbstractBaseField::SetStringInternal(StringView value)
    {
        string_ = String(value);
        ShowValue();
    }

    f64 AbstractBaseField::SettingInternal(i32 which) const { return which >= 0 && which < 8 ? settings_[which] : 0; }

    void AbstractBaseField::SetSettingInternal(i32 which, f64 value)
    {
        if (which < 0 || which >= 8)
            return;
        settings_[which] = value;
        ShowValue();
    }

    TextInput& AbstractBaseField::Text()
    {
        if (!text_)
        {
            text_ = std::make_unique<TextInput>(*this);
            text_->Build(input_);
        }
        return *text_;
    }

    String AbstractBaseField::TextInternal() const { return text_ ? text_->Text() : String(); }
    void AbstractBaseField::SetTextInternal(StringView text) { Text().SetText(text); }
    i32 AbstractBaseField::TextOptionInternal(i32 option) const { return text_ ? text_->Option(option) : 0; }
    void AbstractBaseField::SetTextOptionInternal(i32 option, i32 value) { Text().SetOption(option, value); }
    void AbstractBaseField::SelectRangeInternal(i32 cursorIndex, i32 selectionIndex) { Text().Select(cursorIndex, selectionIndex); }

    PopupMenu& AbstractBaseField::Popup()
    {
        if (!popup_)
        {
            popup_ = std::make_unique<PopupMenu>(*this);
            AddToClassList("helix-base-popup-field");
            SetPartClass(input_, "helix-base-popup-field__input", true);
            popup_->text = MakePart(input_, "helix-text-element helix-base-popup-field__text", "TextElement");
            ElementHandle arrow = MakePart(input_, "helix-base-popup-field__arrow");
            world().tree().SetPicking(popup_->text, PickingMode::Ignore);
            world().tree().SetPicking(arrow, PickingMode::Ignore);
            SetFocusable(true);
            // A press on the input opens the menu (the keyboard's submit too).
            auto open = [](void* user, EventSystem&, UIEvent& e) {
                auto* self = static_cast<AbstractBaseField*>(user);
                if (!self->enabledInHierarchy() || (e.Is(Events().pointerDown) && e.button != 0))
                    return;
                if (!self->Popup().open())
                    self->Popup().Open();
                e.StopPropagation();
            };
            Listen(*this, input_, Events().pointerDown, open);
            Listen(*this, handle(), Events().navigationSubmit, open);
        }
        return *popup_;
    }

    String AbstractBaseField::ChoicesInternal() const
    {
        String out;
        for (usize i = 0; i < choices_.Size(); i++)
        {
            if (i)
                out += '\n';
            out += choices_[i].View();
        }
        return out;
    }

    void AbstractBaseField::SetChoicesInternal(StringView choices)
    {
        choices_ = choices.Empty() ? Array<String>() : SplitList(choices, '\n');
        OnChoicesChanged();
        ShowValue();
    }

    void AbstractBaseField::SetShownTextInternal(StringView text) { world().tree().SetText(Popup().text, text); }
    void AbstractBaseField::ShowMenuInternal() { Popup().Open(); }

    void AbstractBaseField::RememberPrevious()
    {
        for (u32 i = 0; i < kSlots; i++)
        {
            previousNumbers_[i] = numbers_[i];
            previousIntegers_[i] = integers_[i];
        }
        previousString_ = string_;
    }

    void AbstractBaseField::ChangedByInput()
    {
        ShowValue();
        if (onChange_)
            onChange_(onChangeUser_, *this);
        else
            NotifyControl(handle(), ControlNotify::ValueChanged);
    }

    void AbstractBaseField::OnTextChanged(bool committed)
    {
        if (!text_ || (TextOptionInternal(Delayed) && !committed))
            return;
        String now = text_->Text();
        if (now == string_)
            return;
        RememberPrevious();
        string_ = now;
        ChangedByInput();
    }

    void AbstractBaseField::OnChoice(i32 index)
    {
        if (index < 0 || index >= i32(choices_.Size()))
            return;
        RememberPrevious();
        integers_[0] = index;
        string_ = choices_[usize(index)];
        ChangedByInput();
    }

    Span<const ControlAttribute> AbstractBaseField::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"label", [](Element& s, StringView v) { return static_cast<AbstractBaseField&>(s).SetLabel(v), true; }},
            {"show-mixed-value",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<AbstractBaseField&>(s).SetShowMixedValue(b), true);
             }},
        };
        return kAttributes;
    }

    // ------------------------------------------------------------------------------------------------ PopupMenu

    PopupMenu::PopupMenu(AbstractBaseField& field) : field_(field) {}
    PopupMenu::~PopupMenu() { Close(); }
    bool PopupMenu::open() const { return ControlTree().IsAlive(menu_); }

    void PopupMenu::Open()
    {
        Close();
        Panel* p = field_.panel();
        if (!p)
            return;
        ElementTree& tree = ControlTree();
        // Over the whole panel (a press outside the items closes it), the items under the field's input.
        menu_ = MakePart(p->root(), "helix-base-dropdown");
        SetPartStyle(menu_, "position", "absolute");
        SetPartStyle(menu_, "left", "0");
        SetPartStyle(menu_, "top", "0");
        SetPartStyle(menu_, "right", "0");
        SetPartStyle(menu_, "bottom", "0");
        Rect at = PartRect(field_, field_.input_);
        ElementHandle outer = MakePart(menu_, "helix-base-dropdown__container-outer");
        SetPartStyle(outer, "position", "absolute");
        SetPartStyle(outer, "left", Format("{}px", at.x).View());
        SetPartStyle(outer, "top", Format("{}px", at.y + at.height).View());
        SetPartStyle(outer, "min-width", Format("{}px", at.width).View());
        ElementHandle inner = MakePart(outer, "helix-base-dropdown__container-inner");
        items_.Clear();
        EventCallbacks& callbacks = field_.world().callbacks();
        auto item = [&](StringView name, bool on) {
            ElementHandle i = MakePart(inner, "helix-base-dropdown__item");
            ElementHandle check = MakePart(i, "helix-base-dropdown__checkmark");
            ElementHandle label = MakePart(i, "helix-text-element helix-base-dropdown__label", "TextElement");
            tree.SetText(label, name);
            tree.SetPicking(check, PickingMode::Ignore);
            tree.SetPicking(label, PickingMode::Ignore);
            SetPartState(i, PseudoStates::Checked, on);
            callbacks.Register(i, Events().pointerUp, &PopupMenu::ItemUp, this, TrickleDown::NoTrickleDown);
            items_.Push(i);
        };
        for (usize k = 0; k < field_.choices_.Size(); k++)
        {
            bool on = false;
            for (u32 c : checked)
                on = on || c == k;
            item(field_.choices_[k].View(), on);
        }
        callbacks.Register(menu_, Events().pointerDown, &PopupMenu::OutsideDown, this, TrickleDown::NoTrickleDown);
        // Built in the panel's next update.
        p->AttachTypes(menu_);
    }

    void PopupMenu::Close()
    {
        ElementTree& tree = ControlTree();
        if (!tree.IsAlive(menu_))
            return;
        EventCallbacks& callbacks = ElementWorld::Main().callbacks();
        tree.ForEachInSubtree(menu_, [&](ElementHandle e, u32) { callbacks.Clear(e); });
        tree.Destroy(menu_);
        menu_ = ElementHandle();
        items_.Clear();
    }

    void PopupMenu::ItemUp(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<PopupMenu*>(user);
        for (usize i = 0; i < self->items_.Size(); i++)
            if (self->items_[i] == e.currentTarget)
            {
                bool multiple = self->multiple;
                if (!multiple)
                    self->Close();
                self->field_.OnChoice(i32(i));
                if (multiple && self->open())
                    for (usize k = 0; k < self->items_.Size(); k++)
                    {
                        bool on = false;
                        for (u32 c : self->checked)
                            on = on || c == k;
                        SetPartState(self->items_[k], PseudoStates::Checked, on);
                    }
                e.StopPropagation();
                return;
            }
    }

    void PopupMenu::OutsideDown(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<PopupMenu*>(user);
        if (e.target == self->menu_)
        {
            self->Close();
            e.StopPropagation();
        }
    }

    // --------------------------------------------------------------------------------------- Toggle, RadioButton

    Toggle::Toggle() : AbstractBaseField(Intern("Toggle")) { InitBool("helix-toggle", false); }
    Toggle::Toggle(Atom type) : AbstractBaseField(type) {}
    Toggle::Toggle(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e) { InitBool("helix-toggle", false); }
    Toggle::Toggle(ElementWorld& world, ElementHandle e, bool radio) : AbstractBaseField(world, e)
    {
        InitBool(radio ? "helix-radio-button" : "helix-toggle", radio);
    }
    scripting::TypeId Toggle::HelixType() const { return scripting::TypeOf<Toggle>(); }

    void Toggle::InitBool(StringView block, bool radio)
    {
        AddToClassList(block);
        SetPartClass(input_, Format("{}__input", block).View(), true);
        ElementHandle under = input_;
        if (radio)
            under = MakePart(input_, Format("{}__checkmark-background", block).View());
        checkmark_ = MakePart(under, Format("{}__checkmark", block).View());
        textPart_ = MakePart(input_, Format("helix-text-element helix-label {}__text", block).View(), "Label");
        SetFocusable(true);
        const EventTypes& t = Events();
        Listen(*this, handle(), t.pointerDown, &Toggle::PointerDown);
        Listen(*this, handle(), t.pointerUp, &Toggle::PointerUp);
        Listen(*this, handle(), t.navigationSubmit, &Toggle::Submit);
        ShowValue();
    }

    void Toggle::ShowValue()
    {
        bool on = integers_[0] != 0;
        SetPartState(handle(), PseudoStates::Checked, on && !mixed_);
        SetPartState(input_, PseudoStates::Checked, on && !mixed_);
        world().tree().SetText(textPart_, string_.View());
        ShowPart(textPart_, !string_.Empty());
    }

    void Toggle::Turn()
    {
        RememberPrevious();
        integers_[0] = integers_[0] ? 0 : 1;
        mixed_ = false;
        ChangedByInput();
    }

    void Toggle::PointerDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Toggle*>(user);
        if (e.button != 0 || !self->enabledInHierarchy())
            return;
        self->pointer_ = e.pointerId;
        events.CapturePointer(self->handle(), e.pointerId);
        SetPartState(self->handle(), PseudoStates::Active, true);
        e.StopPropagation();
    }

    void Toggle::PointerUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Toggle*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        events.ReleasePointer(self->handle(), e.pointerId);
        SetPartState(self->handle(), PseudoStates::Active, false);
        if (PointerInside(events, self->handle(), e.position) && self->enabledInHierarchy())
            self->Turn();
        e.StopPropagation();
    }

    void Toggle::Submit(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<Toggle*>(user);
        if (!self->enabledInHierarchy())
            return;
        self->Turn();
        e.StopPropagation();
    }

    Span<const ControlAttribute> Toggle::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"value",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<Toggle&>(s).SetIntegerInternal(0, b ? 1 : 0), true);
             }},
            {"text", [](Element& s, StringView v) { return static_cast<Toggle&>(s).SetStringInternal(v), true; }},
        };
        return kAttributes;
    }

    RadioButton::RadioButton() : Toggle(Intern("RadioButton")) { InitBool("helix-radio-button", true); }
    RadioButton::RadioButton(ElementWorld& world, ElementHandle e) : Toggle(world, e, true) {}
    scripting::TypeId RadioButton::HelixType() const { return scripting::TypeOf<RadioButton>(); }

    void RadioButton::Turn()
    {
        if (integers_[0])
            return;
        RememberPrevious();
        integers_[0] = 1;
        mixed_ = false;
        ChangedByInput();
        Exclusive();
    }

    void RadioButton::Exclusive()
    {
        ElementTree& tree = world().tree();
        Atom groupBox = Intern("helix-group-box"), radioGroup = Intern("helix-radio-button-group"), radio = Intern("helix-radio-button");
        auto groupOf = [&](ElementHandle e) {
            ElementHandle last = tree.Parent(e);
            for (ElementHandle p = tree.Parent(e); p.IsValid(); p = tree.Parent(p))
            {
                if (tree.HasClass(p, groupBox) || tree.HasClass(p, radioGroup))
                    return p;
                last = p;
            }
            return tree.Parent(e).IsValid() ? tree.Parent(e) : last;
        };
        ElementHandle group = groupOf(handle());
        if (!group.IsValid())
            return;
        tree.ForEachInSubtree(group, [&](ElementHandle e, u32) {
            if (e == handle() || !tree.HasClass(e, radio) || groupOf(e) != group)
                return;
            auto* other = static_cast<RadioButton*>(Element::Of(world(), e));
            if (other && other->integers_[0])
            {
                other->RememberPrevious();
                other->integers_[0] = 0;
                other->ChangedByInput();
            }
        });
        if (tree.HasClass(group, radioGroup))
        {
            if (auto* g = static_cast<RadioButtonGroup*>(Element::Of(world(), group)))
                g->Picked(handle());
        }
    }

    // ---------------------------------------------------------------------------------------- RadioButtonGroup

    RadioButtonGroup::RadioButtonGroup() : AbstractBaseField(Intern("RadioButtonGroup"))
    {
        AddToClassList("helix-radio-button-group");
        container_ = MakePart(input_, "helix-radio-button-group__container");
        integers_[0] = -1;
    }
    RadioButtonGroup::RadioButtonGroup(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e)
    {
        AddToClassList("helix-radio-button-group");
        container_ = MakePart(input_, "helix-radio-button-group__container");
        integers_[0] = -1;
    }
    scripting::TypeId RadioButtonGroup::HelixType() const { return scripting::TypeOf<RadioButtonGroup>(); }
    Element* RadioButtonGroup::contentContainer() { return Element::Of(world(), container_); }

    void RadioButtonGroup::Rebuild()
    {
        ElementTree& tree = world().tree();
        for (ElementHandle c : choices_)
            if (tree.IsAlive(c))
            {
                tree.ForEachInSubtree(c, [&](ElementHandle e, u32) { world().callbacks().Clear(e); });
                tree.Destroy(c);
            }
        choices_.Clear();
        for (const String& name : AbstractBaseField::choices_)
        {
            auto* r = new RadioButton();
            r->SetStringInternal(name.View());
            tree.Append(container_, r->handle());
            choices_.Push(r->handle());
        }
        if (Panel* p = panel())
            p->AttachTypes(container_);
        ShowValue();
    }

    void RadioButtonGroup::OnChoice(i32 index)
    {
        RememberPrevious();
        integers_[0] = index;
        ChangedByInput();
    }

    void RadioButtonGroup::Picked(ElementHandle radio)
    {
        // Its radio buttons: those of its choices, then those in its content.
        i32 index = 0, found = -1;
        world().tree().ForEachInSubtree(container_, [&](ElementHandle e, u32) {
            if (!world().tree().HasClass(e, Intern("helix-radio-button")))
                return;
            if (e == radio)
                found = index;
            index++;
        });
        if (found >= 0 && found != integers_[0])
            OnChoice(found);
    }

    void RadioButtonGroup::ShowValue()
    {
        i32 index = 0;
        world().tree().ForEachInSubtree(container_, [&](ElementHandle e, u32) {
            if (!world().tree().HasClass(e, Intern("helix-radio-button")))
                return;
            if (auto* r = static_cast<RadioButton*>(Element::Of(world(), e)))
            {
                i64 on = index == integers_[0] ? 1 : 0;
                if (r->IntegerInternal(0) != on)
                    r->SetIntegerInternal(0, on);
            }
            index++;
        });
    }

    Span<const ControlAttribute> RadioButtonGroup::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"choices",
             [](Element& s, StringView v) {
                 auto& g = static_cast<RadioButtonGroup&>(s);
                 Array<String> list = SplitList(v, ',');
                 String joined;
                 for (usize i = 0; i < list.Size(); i++)
                     joined += i ? Format("\n{}", list[i].View()).View() : list[i].View();
                 g.SetChoicesInternal(joined.View());
                 g.Rebuild();
                 return true;
             }},
            {"value",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<RadioButtonGroup&>(s).SetIntegerInternal(0, n), true);
             }},
        };
        return kAttributes;
    }

    // --------------------------------------------------------------------------------------- ToggleButtonGroup

    ToggleButtonGroup::ToggleButtonGroup() : AbstractBaseField(Intern("ToggleButtonGroup"))
    {
        AddToClassList("helix-toggle-button-group");
        container_ = MakePart(input_, "helix-toggle-button-group__container");
        StartControlTick(*this, &ToggleButtonGroup::Sync);
    }
    ToggleButtonGroup::ToggleButtonGroup(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e)
    {
        AddToClassList("helix-toggle-button-group");
        container_ = MakePart(input_, "helix-toggle-button-group__container");
        StartControlTick(*this, &ToggleButtonGroup::Sync);
    }
    scripting::TypeId ToggleButtonGroup::HelixType() const { return scripting::TypeOf<ToggleButtonGroup>(); }
    Element* ToggleButtonGroup::contentContainer() { return Element::Of(world(), container_); }

    Array<ElementHandle> ToggleButtonGroup::Buttons() const
    {
        Array<ElementHandle> out;
        Atom button = Intern("helix-button");
        for (ElementHandle c = world().tree().FirstChild(container_); c.IsValid(); c = world().tree().NextSibling(c))
            if (world().tree().HasClass(c, button))
                out.Push(c);
        return out;
    }

    bool ToggleButtonGroup::Sync(Element& self, f64)
    {
        // The buttons added since (markup's, a script's) take the group's clicks.
        auto& g = static_cast<ToggleButtonGroup&>(self);
        Array<ElementHandle> buttons = g.Buttons();
        for (usize i = 0; i < buttons.Size(); i++)
            if (auto* b = static_cast<Button*>(Element::Of(g.world(), buttons[i])))
            {
                b->SetClickListener(&ToggleButtonGroup::OnButton, &g);
                SetPartClass(buttons[i], "helix-toggle-button-group__button", true);
                SetPartClass(buttons[i], "helix-toggle-button-group__button--first", i == 0);
                SetPartClass(buttons[i], "helix-toggle-button-group__button--last", i + 1 == buttons.Size());
            }
        if (g.integers_[1] != i64(buttons.Size()))
        {
            g.integers_[1] = i64(buttons.Size());
            g.ShowValue();
        }
        return true;
    }

    void ToggleButtonGroup::OnButton(void* user, Button& button)
    {
        auto* g = static_cast<ToggleButtonGroup*>(user);
        Array<ElementHandle> buttons = g->Buttons();
        for (usize i = 0; i < buttons.Size() && i < 64; i++)
            if (buttons[i] == button.handle())
            {
                u64 bit = 1ull << i, mask = u64(g->integers_[0]);
                bool multiple = g->settings_[0] != 0, allowEmpty = g->settings_[1] != 0;
                u64 next = multiple ? (mask ^ bit) : ((mask & bit) ? (allowEmpty ? 0 : mask) : bit);
                if (next == 0 && !allowEmpty)
                    next = mask;
                if (next == mask)
                    return;
                g->RememberPrevious();
                g->integers_[0] = i64(next);
                g->integers_[1] = i64(buttons.Size());
                g->ChangedByInput();
                return;
            }
    }

    void ToggleButtonGroup::ShowValue()
    {
        Array<ElementHandle> buttons = Buttons();
        for (usize i = 0; i < buttons.Size() && i < 64; i++)
            SetPartState(buttons[i], PseudoStates::Checked, (u64(integers_[0]) >> i) & 1);
    }

    Span<const ControlAttribute> ToggleButtonGroup::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"is-multiple-selection",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<ToggleButtonGroup&>(s).SetSettingInternal(0, b ? 1 : 0), true);
             }},
            {"allow-empty-selection",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<ToggleButtonGroup&>(s).SetSettingInternal(1, b ? 1 : 0), true);
             }},
        };
        return kAttributes;
    }

    // -------------------------------------------------------------------------------------------------- Slider

    Slider::Slider() : AbstractBaseField(Intern("Slider")) { InitSlider("helix-slider", false); }
    Slider::Slider(Atom type, bool whole) : AbstractBaseField(type) { InitSlider("helix-slider-int", whole); }
    Slider::Slider(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e) { InitSlider("helix-slider", false); }
    scripting::TypeId Slider::HelixType() const { return scripting::TypeOf<Slider>(); }

    void Slider::InitSlider(StringView block, bool whole)
    {
        AddToClassList("helix-base-slider");
        AddToClassList(block);
        AddToClassList("helix-base-slider--horizontal");
        SetPartClass(input_, "helix-base-slider__input", true);
        drag_ = MakePart(input_, "helix-base-slider__drag-container");
        tracker_ = MakePart(drag_, "helix-base-slider__tracker");
        fill_ = MakePart(drag_, "helix-base-slider__fill");
        dragger_ = MakePart(drag_, "helix-base-slider__dragger");
        ShowPart(fill_, false);
        settings_[0] = 0;
        settings_[1] = 10;
        settings_[6] = whole ? 1 : 0;
        SetFocusable(true);
        Listen(*this, drag_, Events().pointerDown, &Slider::PointerDown);
        Listen(*this, drag_, Events().pointerMove, &Slider::PointerMove);
        Listen(*this, drag_, Events().pointerUp, &Slider::PointerUp);
        Listen(*this, handle(), Events().navigationMove, &Slider::Navigate);
        Listen(*this, drag_, Events().geometryChanged, &Slider::Geometry);
        ShowValue();
    }

    f64 Slider::ValueAt(f64 k) const
    {
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        if (settings_[4] != 0)
            k = 1 - k;
        f64 v = settings_[0] + (settings_[1] - settings_[0]) * k;
        return settings_[6] != 0 ? std::round(v) : v;
    }

    void Slider::SetByUser(f64 value)
    {
        f64 lo = settings_[0] < settings_[1] ? settings_[0] : settings_[1], hi = settings_[0] < settings_[1] ? settings_[1] : settings_[0];
        value = value < lo ? lo : value > hi ? hi : value;
        if (settings_[6] != 0)
            value = std::round(value);
        if (value == numbers_[0])
            return;
        RememberPrevious();
        numbers_[0] = value;
        ChangedByInput();
    }

    f64 Slider::Fraction(EventSystem&, Vector2 position) const
    {
        Rect r = PartRect(*this, drag_), d = PartRect(*this, dragger_);
        bool vertical = settings_[3] != 0;
        f64 length = vertical ? r.height - d.height : r.width - d.width;
        if (length <= 0)
            return 0;
        f64 at = vertical ? position.y - r.y - d.height * 0.5 - grab_ : position.x - r.x - d.width * 0.5 - grab_;
        return at / length;
    }

    void Slider::ShowValue()
    {
        bool vertical = settings_[3] != 0;
        SetPartClass(handle(), "helix-base-slider--horizontal", !vertical);
        SetPartClass(handle(), "helix-base-slider--vertical", vertical);
        f64 range = settings_[1] - settings_[0];
        f64 k = range != 0 ? (numbers_[0] - settings_[0]) / range : 0;
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        if (settings_[4] != 0)
            k = 1 - k;
        Rect r = PartRect(*this, drag_), d = PartRect(*this, dragger_);
        f64 length = vertical ? r.height - d.height : r.width - d.width;
        StringView side = vertical ? "top" : "left";
        if (length > 0)
            SetPartStyle(dragger_, side, Format("{}px", k * length).View());
        else
            SetPartStyle(dragger_, side, Format("{}%", k * 100).View());
        ShowPart(fill_, settings_[5] != 0);
        SetPartStyle(fill_, vertical ? "height" : "width", Format("{}%", k * 100).View());
    }

    void Slider::PointerDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Slider*>(user);
        if (e.button != 0 || !self->enabledInHierarchy())
            return;
        events.Focus(self->handle());
        bool onDragger = PointerInside(events, self->dragger_, e.position);
        Rect d = PartRect(*self, self->dragger_);
        bool vertical = self->settings_[3] != 0;
        self->grab_ = onDragger ? (vertical ? e.position.y - d.y - d.height * 0.5 : e.position.x - d.x - d.width * 0.5) : 0;
        if (!onDragger && self->settings_[2] > 0)
        {
            // A page toward the point.
            f64 target = self->ValueAt(self->Fraction(events, e.position));
            f64 step = self->settings_[2] * (target > self->numbers_[0] ? 1 : -1);
            self->SetByUser(std::fabs(target - self->numbers_[0]) < self->settings_[2] ? target : self->numbers_[0] + step);
        }
        else if (!onDragger)
            self->SetByUser(self->ValueAt(self->Fraction(events, e.position)));
        self->pointer_ = e.pointerId;
        events.CapturePointer(self->drag_, e.pointerId);
        SetPartState(self->dragger_, PseudoStates::Active, true);
        e.StopPropagation();
    }

    void Slider::PointerMove(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Slider*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->SetByUser(self->ValueAt(self->Fraction(events, e.position)));
    }

    void Slider::PointerUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Slider*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        events.ReleasePointer(self->drag_, e.pointerId);
        SetPartState(self->dragger_, PseudoStates::Active, false);
    }

    void Slider::Navigate(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<Slider*>(user);
        bool vertical = self->settings_[3] != 0;
        f64 sign = 0;
        if (!vertical && (e.direction == NavigationDirection::Left || e.direction == NavigationDirection::Right))
            sign = e.direction == NavigationDirection::Right ? 1 : -1;
        if (vertical && (e.direction == NavigationDirection::Up || e.direction == NavigationDirection::Down))
            sign = e.direction == NavigationDirection::Down ? 1 : -1;
        if (sign == 0)
            return;
        if (self->settings_[4] != 0)
            sign = -sign;
        f64 range = std::fabs(self->settings_[1] - self->settings_[0]);
        f64 step = self->settings_[6] != 0 ? 1 : range / 100;
        self->SetByUser(self->numbers_[0] + sign * step * (self->settings_[1] >= self->settings_[0] ? 1 : -1));
        e.StopPropagation();
    }

    void Slider::Geometry(void* user, EventSystem&, UIEvent&) { static_cast<Slider*>(user)->ShowValue(); }

    Span<const ControlAttribute> Slider::MarkupAttributes()
    {
        static const StringView kDirections[] = {"Horizontal", "Vertical"};
        static const ControlAttribute kAttributes[] = {
            {"value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Slider&>(s).SetNumberInternal(0, n), true);
             }},
            {"low-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Slider&>(s).SetSettingInternal(0, n), true);
             }},
            {"high-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Slider&>(s).SetSettingInternal(1, n), true);
             }},
            {"page-size",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<Slider&>(s).SetSettingInternal(2, n), true);
             }},
            {"direction",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kDirections, k) && (static_cast<Slider&>(s).SetSettingInternal(3, k), true);
             }},
            {"inverted",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<Slider&>(s).SetSettingInternal(4, b ? 1 : 0), true);
             }},
            {"fill",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<Slider&>(s).SetSettingInternal(5, b ? 1 : 0), true);
             }},
            {"show-input-field",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<Slider&>(s).SetSettingInternal(7, b ? 1 : 0), true);
             }},
        };
        return kAttributes;
    }

    SliderInt::SliderInt() : Slider(Intern("SliderInt"), true) {}
    SliderInt::SliderInt(ElementWorld& world, ElementHandle e) : Slider(world, e)
    {
        RemoveFromClassList("helix-slider");
        AddToClassList("helix-slider-int");
        settings_[6] = 1;
    }
    scripting::TypeId SliderInt::HelixType() const { return scripting::TypeOf<SliderInt>(); }

    // ------------------------------------------------------------------------------------------- MinMaxSlider

    MinMaxSlider::MinMaxSlider() : AbstractBaseField(Intern("MinMaxSlider")) { InitMinMax(); }
    MinMaxSlider::MinMaxSlider(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e) { InitMinMax(); }

    void MinMaxSlider::InitMinMax()
    {
        AddToClassList("helix-min-max-slider");
        SetPartClass(input_, "helix-min-max-slider__input", true);
        tracker_ = MakePart(input_, "helix-min-max-slider__tracker");
        range_ = MakePart(input_, "helix-min-max-slider__dragger");
        minThumb_ = MakePart(range_, "helix-min-max-slider__min-thumb");
        maxThumb_ = MakePart(range_, "helix-min-max-slider__max-thumb");
        settings_[0] = 0, settings_[1] = 10;
        numbers_[0] = 0, numbers_[1] = 10;
        Listen(*this, input_, Events().pointerDown, &MinMaxSlider::PointerDown);
        Listen(*this, input_, Events().pointerMove, &MinMaxSlider::PointerMove);
        Listen(*this, input_, Events().pointerUp, &MinMaxSlider::PointerUp);
        Listen(*this, input_, Events().geometryChanged, &MinMaxSlider::Geometry);
        ShowValue();
    }
    scripting::TypeId MinMaxSlider::HelixType() const { return scripting::TypeOf<MinMaxSlider>(); }

    f64 MinMaxSlider::Fraction(EventSystem&, Vector2 position) const
    {
        Rect r = PartRect(*this, input_);
        return r.width > 0 ? (position.x - r.x) / r.width : 0;
    }

    void MinMaxSlider::ShowValue()
    {
        f64 range = settings_[1] - settings_[0];
        auto k = [&](f64 v) {
            f64 f = range != 0 ? (v - settings_[0]) / range : 0;
            return f < 0 ? 0 : f > 1 ? 1 : f;
        };
        f64 a = k(numbers_[0]), b = k(numbers_[1]);
        SetPartStyle(range_, "left", Format("{}%", a * 100).View());
        SetPartStyle(range_, "width", Format("{}%", (b - a) * 100).View());
    }

    void MinMaxSlider::PointerDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<MinMaxSlider*>(user);
        if (e.button != 0 || !self->enabledInHierarchy())
            return;
        f64 at = self->settings_[0] + (self->settings_[1] - self->settings_[0]) * self->Fraction(events, e.position);
        if (PointerInside(events, self->minThumb_, e.position))
            self->part_ = 0;
        else if (PointerInside(events, self->maxThumb_, e.position))
            self->part_ = 1;
        else if (PointerInside(events, self->range_, e.position))
            self->part_ = 2;
        else
            self->part_ = std::fabs(at - self->numbers_[0]) < std::fabs(at - self->numbers_[1]) ? 0 : 1;
        self->grab_ = at;
        self->pointer_ = e.pointerId;
        events.CapturePointer(self->input_, e.pointerId);
        PointerMove(user, events, e);
        e.StopPropagation();
    }

    void MinMaxSlider::PointerMove(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<MinMaxSlider*>(user);
        if (e.pointerId != self->pointer_)
            return;
        f64 lo = self->settings_[0], hi = self->settings_[1];
        f64 at = lo + (hi - lo) * self->Fraction(events, e.position);
        f64 a = self->numbers_[0], b = self->numbers_[1];
        if (self->part_ == 0)
            a = at < lo ? lo : at > b ? b : at;
        else if (self->part_ == 1)
            b = at > hi ? hi : at < a ? a : at;
        else
        {
            f64 d = at - self->grab_;
            d = a + d < lo ? lo - a : b + d > hi ? hi - b : d;
            a += d, b += d;
            self->grab_ = at;
        }
        if (a == self->numbers_[0] && b == self->numbers_[1])
            return;
        self->RememberPrevious();
        self->numbers_[0] = a, self->numbers_[1] = b;
        self->ChangedByInput();
    }

    void MinMaxSlider::PointerUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<MinMaxSlider*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        events.ReleasePointer(self->input_, e.pointerId);
    }

    void MinMaxSlider::Geometry(void* user, EventSystem&, UIEvent&) { static_cast<MinMaxSlider*>(user)->ShowValue(); }

    Span<const ControlAttribute> MinMaxSlider::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"min-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<MinMaxSlider&>(s).SetNumberInternal(0, n), true);
             }},
            {"max-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<MinMaxSlider&>(s).SetNumberInternal(1, n), true);
             }},
            {"low-limit",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<MinMaxSlider&>(s).SetSettingInternal(0, n), true);
             }},
            {"high-limit",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<MinMaxSlider&>(s).SetSettingInternal(1, n), true);
             }},
        };
        return kAttributes;
    }

    // ------------------------------------------------------------------------------------------- popup fields

    DropdownField::DropdownField() : AbstractBaseField(Intern("DropdownField")) { InitPopupField("helix-dropdown-field"); }
    DropdownField::DropdownField(Atom type) : AbstractBaseField(type) {}
    DropdownField::DropdownField(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e) { InitPopupField("helix-dropdown-field"); }
    scripting::TypeId DropdownField::HelixType() const { return scripting::TypeOf<DropdownField>(); }

    void DropdownField::InitPopupField(StringView block)
    {
        AddToClassList("helix-popup-field");
        AddToClassList(block);
        Popup();
        integers_[0] = -1;
        ShowValue();
    }

    void DropdownField::ShowValue()
    {
        i64 i = integers_[0];
        String shown = i >= 0 && i < i64(choices_.Size()) ? choices_[usize(i)] : string_;
        world().tree().SetText(Popup().text, mixed_ ? StringView("-") : shown.View());
        Popup().checked.Clear();
        if (i >= 0)
            Popup().checked.Push(u32(i));
    }

    void DropdownField::OnChoice(i32 index) { AbstractBaseField::OnChoice(index); }

    Span<const ControlAttribute> DropdownField::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"choices",
             [](Element& s, StringView v) {
                 Array<String> list = SplitList(v, ',');
                 String joined;
                 for (usize i = 0; i < list.Size(); i++)
                 {
                     if (i)
                         joined += '\n';
                     joined += list[i].View();
                 }
                 static_cast<DropdownField&>(s).SetChoicesInternal(joined.View());
                 return true;
             }},
            {"index",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<DropdownField&>(s).SetIntegerInternal(0, n), true);
             }},
            {"value",
             [](Element& s, StringView v) {
                 auto& f = static_cast<DropdownField&>(s);
                 f.SetStringInternal(v);
                 return true;
             }},
        };
        return kAttributes;
    }

    EnumField::EnumField() : DropdownField(Intern("EnumField")) { InitPopupField("helix-enum-field"); }
    EnumField::EnumField(ElementWorld& world, ElementHandle e) : DropdownField(world, e) { AddToClassList("helix-enum-field"); }
    scripting::TypeId EnumField::HelixType() const { return scripting::TypeOf<EnumField>(); }

    MaskField::MaskField() : DropdownField(Intern("MaskField"))
    {
        InitPopupField("helix-mask-field");
        Popup().multiple = true;
        integers_[0] = 0;
        ShowValue();
    }
    MaskField::MaskField(Atom type) : DropdownField(type)
    {
        InitPopupField("helix-mask64-field");
        Popup().multiple = true;
        integers_[0] = 0;
        ShowValue();
    }
    MaskField::MaskField(ElementWorld& world, ElementHandle e) : DropdownField(world, e)
    {
        AddToClassList("helix-mask-field");
        Popup().multiple = true;
        integers_[0] = 0;
    }
    scripting::TypeId MaskField::HelixType() const { return scripting::TypeOf<MaskField>(); }

    void MaskField::ShowValue()
    {
        // The menu: Nothing, Everything, then the choices (checked by the mask).
        u64 mask = u64(integers_[0]);
        usize n = choices_.Size();
        u64 all = n >= 64 ? ~0ull : (1ull << n) - 1;
        Array<u32>& checked = Popup().checked;
        checked.Clear();
        if ((mask & all) == 0)
            checked.Push(0);
        if (n > 0 && (mask & all) == all)
            checked.Push(1);
        u32 on = 0, last = 0;
        for (usize i = 0; i < n && i < 64; i++)
            if ((mask >> i) & 1)
            {
                checked.Push(u32(i + 2));
                on++;
                last = u32(i);
            }
        String shown = (mask & all) == 0 ? String("Nothing") : (mask & all) == all ? String("Everything") : on == 1 ? choices_[last] : String("Mixed...");
        world().tree().SetText(Popup().text, mixed_ ? StringView("-") : shown.View());
    }

    void MaskField::OnChoice(i32 index)
    {
        usize n = choices_.Size();
        u64 all = n >= 64 ? ~0ull : (1ull << n) - 1;
        u64 mask = u64(integers_[0]);
        u64 next = index == 0 ? 0 : index == 1 ? all : (mask ^ (1ull << u32(index - 2)));
        if (next == mask)
            return;
        RememberPrevious();
        integers_[0] = i64(next);
        ChangedByInput();
    }

    Span<const ControlAttribute> MaskField::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"value",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<MaskField&>(s).SetIntegerInternal(0, n), true);
             }},
        };
        return kAttributes;
    }

    Mask64Field::Mask64Field() : MaskField(Intern("Mask64Field")) {}
    Mask64Field::Mask64Field(ElementWorld& world, ElementHandle e) : MaskField(world, e) { AddToClassList("helix-mask64-field"); }
    scripting::TypeId Mask64Field::HelixType() const { return scripting::TypeOf<Mask64Field>(); }

    // ------------------------------------------------------------------------------------------- composites

    namespace
    {
        const char* const kXY[] = {"X", "Y"};
        const char* const kXYZ[] = {"X", "Y", "Z"};
        const char* const kXYZW[] = {"X", "Y", "Z", "W"};
        const char* const kRect[] = {"X", "Y", "W", "H"};
        const char* const kBoundsRows[] = {"Center", "Extent"};
        const char* const kBoundsIntRows[] = {"Position", "Size"};

        void PartChangedHook(void* user, AbstractBaseField& part) { static_cast<Vector2Field*>(user)->PartChanged(part.handle()); }
    }

    Vector2Field::Vector2Field() : AbstractBaseField(Intern("Vector2Field")) { InitComposite("helix-vector2-field", kXY, false, {}); }
    Vector2Field::Vector2Field(Atom type, StringView block, Span<const char* const> labels, bool integers, Span<const char* const> rows)
        : AbstractBaseField(type)
    {
        InitComposite(block, labels, integers, rows);
    }
    Vector2Field::Vector2Field(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e) { InitComposite("helix-vector2-field", kXY, false, {}); }
    Vector2Field::Vector2Field(ElementWorld& world, ElementHandle e, StringView block, Span<const char* const> labels, bool integers,
                               Span<const char* const> rows)
        : AbstractBaseField(world, e)
    {
        InitComposite(block, labels, integers, rows);
    }
    scripting::TypeId Vector2Field::HelixType() const { return scripting::TypeOf<Vector2Field>(); }

    void Vector2Field::InitComposite(StringView block, Span<const char* const> labels, bool integers, Span<const char* const> rows)
    {
        AddToClassList("helix-composite-field");
        AddToClassList(block);
        SetPartClass(input_, "helix-composite-field__input", true);
        settings_[1] = integers ? 1 : 0;
        u32 rowCount = rows.Empty() ? 1 : u32(rows.Size());
        settings_[0] = f64(labels.Size() * rowCount);
        for (u32 r = 0; r < rowCount; r++)
        {
            ElementHandle row = input_;
            if (!rows.Empty())
            {
                row = MakePart(input_, "helix-composite-field__row");
                ElementHandle name = MakePart(row, "helix-text-element helix-label helix-composite-field__row-label", "Label");
                world().tree().SetText(name, rows[r]);
            }
            for (usize i = 0; i < labels.Size(); i++)
            {
                AbstractBaseField* part = integers ? static_cast<AbstractBaseField*>(new IntegerField()) : new FloatField();
                part->SetLabel(labels[i]);
                part->AddToClassList("helix-composite-field__field");
                if (i == 0)
                    part->AddToClassList("helix-composite-field__field--first");
                part->SetChangeListener(&PartChangedHook, this);
                world().tree().Append(row, part->handle());
                parts_.Push(part->handle());
            }
        }
        ShowValue();
    }

    void Vector2Field::PartChanged(ElementHandle part)
    {
        for (usize i = 0; i < parts_.Size() && i < kSlots; i++)
            if (parts_[i] == part)
            {
                auto* p = static_cast<AbstractBaseField*>(Element::Of(world(), part));
                RememberPrevious();
                if (settings_[1] != 0)
                    integers_[i] = p->IntegerInternal(0);
                else
                    numbers_[i] = p->NumberInternal(0);
                ChangedByInput();
                return;
            }
    }

    void Vector2Field::ShowValue()
    {
        for (usize i = 0; i < parts_.Size() && i < kSlots; i++)
            if (auto* p = static_cast<AbstractBaseField*>(Element::Of(world(), parts_[i])))
            {
                if (settings_[1] != 0)
                {
                    if (p->IntegerInternal(0) != integers_[i])
                        p->SetIntegerInternal(0, integers_[i]);
                }
                else if (p->NumberInternal(0) != numbers_[i])
                    p->SetNumberInternal(0, numbers_[i]);
            }
    }

    Span<const ControlAttribute> Vector2Field::MarkupAttributes()
    {
        static const char* const kNames[] = {"x", "y", "z", "w", "width", "height"};
        static const ControlAttribute kAttributes[] = {
            {"x", [](Element& s, StringView v) { f64 n = 0; return ParseFloatText(v, n) && (static_cast<Vector2Field&>(s).SetNumberInternal(0, n), static_cast<Vector2Field&>(s).SetIntegerInternal(0, i64(n)), true); }},
            {"y", [](Element& s, StringView v) { f64 n = 0; return ParseFloatText(v, n) && (static_cast<Vector2Field&>(s).SetNumberInternal(1, n), static_cast<Vector2Field&>(s).SetIntegerInternal(1, i64(n)), true); }},
            {"z", [](Element& s, StringView v) { f64 n = 0; return ParseFloatText(v, n) && (static_cast<Vector2Field&>(s).SetNumberInternal(2, n), static_cast<Vector2Field&>(s).SetIntegerInternal(2, i64(n)), true); }},
            {"w", [](Element& s, StringView v) { f64 n = 0; return ParseFloatText(v, n) && (static_cast<Vector2Field&>(s).SetNumberInternal(3, n), static_cast<Vector2Field&>(s).SetIntegerInternal(3, i64(n)), true); }},
        };
        (void)kNames;
        return kAttributes;
    }

#define HX_UI_COMPOSITE_IMPL(Name, block, labels, integers, rows)                                                                       \
    Name::Name() : Vector2Field(Intern(#Name), block, labels, integers, rows) {}                                                         \
    Name::Name(ElementWorld& world, ElementHandle e) : Vector2Field(world, e, block, labels, integers, rows) {}                           \
    scripting::TypeId Name::HelixType() const { return scripting::TypeOf<Name>(); }

    HX_UI_COMPOSITE_IMPL(Vector3Field, "helix-vector3-field", kXYZ, false, {})
    HX_UI_COMPOSITE_IMPL(Vector4Field, "helix-vector4-field", kXYZW, false, {})
    HX_UI_COMPOSITE_IMPL(Vector2IntField, "helix-vector2-int-field", kXY, true, {})
    HX_UI_COMPOSITE_IMPL(Vector3IntField, "helix-vector3-int-field", kXYZ, true, {})
    HX_UI_COMPOSITE_IMPL(RectField, "helix-rect-field", kRect, false, {})
    HX_UI_COMPOSITE_IMPL(RectIntField, "helix-rect-int-field", kRect, true, {})
    HX_UI_COMPOSITE_IMPL(BoundsField, "helix-bounds-field", kXYZ, false, kBoundsRows)
    HX_UI_COMPOSITE_IMPL(BoundsIntField, "helix-bounds-int-field", kXYZ, true, kBoundsIntRows)
#undef HX_UI_COMPOSITE_IMPL

    // -------------------------------------------------------------------------------------------- text fields

    TextField::TextField() : AbstractBaseField(Intern("TextField")) { InitTextField("helix-text-field", 0); }
    TextField::TextField(Atom type, StringView block, i32 characters) : AbstractBaseField(type) { InitTextField(block, characters); }
    TextField::TextField(ElementWorld& world, ElementHandle e) : AbstractBaseField(world, e) { InitTextField("helix-text-field", 0); }
    scripting::TypeId TextField::HelixType() const { return scripting::TypeOf<TextField>(); }

    void TextField::InitTextField(StringView block, i32 characters)
    {
        AddToClassList("helix-base-text-field");
        AddToClassList(block);
        SetPartClass(input_, "helix-base-text-field__input", true);
        SetPartClass(input_, Format("{}__input", block).View(), true);
        Text().SetOption(Characters, characters);
    }

    void TextField::ShowValue()
    {
        // The value is the text as the input takes it (no longer than its max length).
        if (Text().Text() != string_)
        {
            Text().SetText(string_.View());
            string_ = Text().Text();
        }
    }

    void TextField::OnTextChanged(bool committed) { AbstractBaseField::OnTextChanged(committed); }

    Span<const ControlAttribute> TextField::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"value", [](Element& s, StringView v) { return static_cast<TextField&>(s).SetStringInternal(v), true; }},
            {"text", [](Element& s, StringView v) { return static_cast<TextField&>(s).SetStringInternal(v), true; }},
            {"multiline",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextField&>(s).SetTextOptionInternal(Multiline, b), true);
             }},
            {"password",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextField&>(s).SetTextOptionInternal(Password, b), true);
             }},
            {"readonly",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextField&>(s).SetTextOptionInternal(ReadOnly, b), true);
             }},
            {"is-delayed",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextField&>(s).SetTextOptionInternal(Delayed, b), true);
             }},
            {"max-length",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<TextField&>(s).SetTextOptionInternal(MaxLength, i32(n)), true);
             }},
            {"mask-character",
             [](Element& s, StringView v) {
                 return !v.Empty() && (static_cast<TextField&>(s).SetTextOptionInternal(MaskChar, i32(u8(v[0]))), true);
             }},
        };
        return kAttributes;
    }

    // ------------------------------------------------------------------------------------------ number fields

    namespace
    {
        const char* const kNumberBlocks[] = {"helix-integer-field", "helix-long-field", "helix-unsigned-integer-field", "helix-unsigned-long-field",
                                             "helix-float-field", "helix-double-field"};
        const i32 kNumberCharacters[] = {1, 1, 2, 2, 3, 3};
    }

    IntegerField::IntegerField() : TextField(Intern("IntegerField"), kNumberBlocks[Int], kNumberCharacters[Int]) { InitNumber(kNumberBlocks[Int], Int); }
    IntegerField::IntegerField(Atom type, StringView block, Kind kind) : TextField(type, block, kNumberCharacters[kind]) { InitNumber(block, kind); }
    IntegerField::IntegerField(ElementWorld& world, ElementHandle e) : TextField(world, e) { InitNumber(kNumberBlocks[Int], Int); }
    scripting::TypeId IntegerField::HelixType() const { return scripting::TypeOf<IntegerField>(); }

    void IntegerField::InitNumber(StringView block, Kind kind)
    {
        (void)block;
        settings_[0] = f64(kind);
        Text().SetOption(Characters, kNumberCharacters[kind]);
        SetPartClass(label_, "helix-base-field__label--with-dragger", true);
        Listen(*this, label_, Events().pointerDown, &IntegerField::LabelDown);
        Listen(*this, label_, Events().pointerMove, &IntegerField::LabelMove);
        Listen(*this, label_, Events().pointerUp, &IntegerField::LabelUp);
        Text().SetText(Format().View());
    }

    String IntegerField::Format() const
    {
        switch (kind())
        {
        case Int:
        case Long: return helix::Format("{}", integers_[0]);
        case UInt:
        case ULong: return helix::Format("{}", u64(integers_[0]));
        case Float: return helix::Format("{}", f32(numbers_[0]));
        default: return helix::Format("{}", numbers_[0]);
        }
    }

    bool IntegerField::Parse(StringView text)
    {
        switch (kind())
        {
        case Int:
        case Long:
        {
            i64 n = 0;
            if (!ParseIntText(text, n))
                return false;
            if (kind() == Int)
                n = n < std::numeric_limits<i32>::min() ? std::numeric_limits<i32>::min() : n > std::numeric_limits<i32>::max() ? std::numeric_limits<i32>::max() : n;
            SetByUser(f64(n), n);
            return true;
        }
        case UInt:
        case ULong:
        {
            u64 n = 0;
            if (!ParseUIntText(text, n))
                return false;
            if (kind() == UInt && n > 0xFFFFFFFFull)
                n = 0xFFFFFFFFull;
            SetByUser(f64(n), i64(n));
            return true;
        }
        default:
        {
            f64 n = 0;
            String t(text);
            for (usize i = 0; i < t.Size(); i++)
                if (t[i] == ',')
                    t[i] = '.';
            if (!ParseFloatText(t.View(), n))
                return false;
            if (kind() == Float)
                n = f64(f32(n));
            SetByUser(n, i64(n));
            return true;
        }
        }
    }

    void IntegerField::SetByUser(f64 number, i64 integer)
    {
        bool floating = kind() == Float || kind() == Double;
        if (floating ? number == numbers_[0] : integer == integers_[0])
            return;
        RememberPrevious();
        if (floating)
            numbers_[0] = number;
        else
            integers_[0] = integer;
        ChangedByInput();
    }

    void IntegerField::ShowValue()
    {
        // While it is being typed into, the text stays as typed.
        if (!Text().focused())
            Text().SetText(Format().View());
    }

    void IntegerField::OnTextChanged(bool committed)
    {
        if (TextOptionInternal(Delayed) && !committed)
            return;
        String text = Text().Text();
        if (!text.Empty())
            Parse(text.View());
        if (committed && Text().Text() != Format())
            Text().SetText(Format().View());
    }

    void IntegerField::LabelDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<IntegerField*>(user);
        if (e.button != 0 || !self->enabledInHierarchy())
            return;
        self->pointer_ = e.pointerId;
        self->dragStart_ = e.position.x;
        self->dragNumber_ = self->numbers_[0];
        self->dragInteger_ = self->integers_[0];
        events.CapturePointer(self->label_, e.pointerId);
        e.StopPropagation();
    }

    void IntegerField::LabelMove(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<IntegerField*>(user);
        if (e.pointerId != self->pointer_)
            return;
        f64 dx = e.position.x - self->dragStart_;
        bool floating = self->kind() == Float || self->kind() == Double;
        // The reference's sensitivity: faster for larger values.
        f64 start = floating ? self->dragNumber_ : f64(self->dragInteger_);
        f64 speed = self->settings_[1] > 0 ? self->settings_[1] : (floating ? std::fmax(1.0, std::pow(std::fabs(start), 0.5)) * 0.03 : std::fmax(1.0, std::pow(std::fabs(start), 0.5) * 0.03));
        if (floating)
            self->SetByUser(self->dragNumber_ + dx * speed, 0);
        else
        {
            i64 v = self->dragInteger_ + i64(std::round(dx * speed));
            if ((self->kind() == UInt || self->kind() == ULong) && v < 0)
                v = 0;
            self->SetByUser(f64(v), v);
        }
        self->Text().SetText(self->Format().View());
    }

    void IntegerField::LabelUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<IntegerField*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        events.ReleasePointer(self->label_, e.pointerId);
    }

    Span<const ControlAttribute> IntegerField::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"value",
             [](Element& s, StringView v) {
                 auto& f = static_cast<IntegerField&>(s);
                 String before = f.Format();
                 bool ok = f.Parse(v);
                 f.Text().SetText(f.Format().View());
                 return ok || before.Empty();
             }},
        };
        return kAttributes;
    }

#define HX_UI_NUMBER_IMPL(Name, kind)                                                                                                    \
    Name::Name() : IntegerField(Intern(#Name), kNumberBlocks[kind], kind) {}                                                             \
    Name::Name(ElementWorld& world, ElementHandle e) : IntegerField(world, e)                                                            \
    {                                                                                                                                    \
        settings_[0] = f64(kind);                                                                                                         \
        Text().SetOption(Characters, kNumberCharacters[kind]);                                                                           \
        AddToClassList(kNumberBlocks[kind]);                                                                                             \
    }                                                                                                                                    \
    scripting::TypeId Name::HelixType() const { return scripting::TypeOf<Name>(); }

    HX_UI_NUMBER_IMPL(LongField, Long)
    HX_UI_NUMBER_IMPL(UnsignedIntegerField, UInt)
    HX_UI_NUMBER_IMPL(UnsignedLongField, ULong)
    HX_UI_NUMBER_IMPL(FloatField, Float)
    HX_UI_NUMBER_IMPL(DoubleField, Double)
#undef HX_UI_NUMBER_IMPL

    // ------------------------------------------------------------------------------------------- Hash128Field

    Hash128Field::Hash128Field() : TextField(Intern("Hash128Field"), "helix-hash128-field", 4) { Text().SetOption(MaxLength, 32); ShowValue(); }
    Hash128Field::Hash128Field(ElementWorld& world, ElementHandle e) : TextField(world, e)
    {
        AddToClassList("helix-hash128-field");
        Text().SetOption(Characters, 4);
        Text().SetOption(MaxLength, 32);
    }
    scripting::TypeId Hash128Field::HelixType() const { return scripting::TypeOf<Hash128Field>(); }

    void Hash128Field::ShowValue()
    {
        if (!Text().focused())
            Text().SetText(engine::Hash128(u64(integers_[0]), u64(integers_[1])).ToString().View());
    }

    void Hash128Field::OnTextChanged(bool committed)
    {
        if (TextOptionInternal(Delayed) && !committed)
            return;
        engine::Hash128 h;
        if (engine::Hash128::TryParse(Text().Text().View(), h) && (i64(h.u64_0) != integers_[0] || i64(h.u64_1) != integers_[1]))
        {
            RememberPrevious();
            integers_[0] = i64(h.u64_0), integers_[1] = i64(h.u64_1);
            ChangedByInput();
        }
        if (committed)
            Text().SetText(engine::Hash128(u64(integers_[0]), u64(integers_[1])).ToString().View());
    }
}
