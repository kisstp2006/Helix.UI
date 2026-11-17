#include <Helix/UI/Engine/Controls.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/ControlParts.h>
#include <Helix/UI/Engine/TextInput.h>
#include <Helix/UI/Styles/Properties.h>

namespace helix::ui
{
    // ------------------------------------------------------------------------------------------------ parts

    ElementTree& ControlTree() { return ElementWorld::Main().tree(); }

    ElementHandle MakePart(ElementHandle parent, StringView classes, StringView type)
    {
        ElementTree& tree = ControlTree();
        ElementHandle e = tree.Create(Intern(type));
        AddClasses(e, classes);
        if (tree.IsAlive(parent))
            tree.Append(parent, e);
        return e;
    }

    void AddClasses(ElementHandle e, StringView classes)
    {
        ElementTree& tree = ControlTree();
        usize at = 0;
        while (at < classes.Size())
        {
            usize end = classes.Find(' ', at);
            if (end == StringView::npos)
                end = classes.Size();
            if (end > at)
                tree.AddClass(e, Intern(classes.Substr(at, end - at)));
            at = end + 1;
        }
    }

    void SetPartStyle(ElementHandle e, StringView property, StringView value)
    {
        ElementWorld& world = ElementWorld::Main();
        if (world.tree().IsAlive(e))
            world.styles().SetInlineProperty(e, FindProperty(property), value);
    }

    void SetPartClass(ElementHandle e, StringView name, bool on)
    {
        ElementTree& tree = ControlTree();
        if (!tree.IsAlive(e))
            return;
        Atom a = Intern(name);
        if (on)
            tree.AddClass(e, a);
        else
            tree.RemoveClass(e, a);
    }

    void SetPartState(ElementHandle e, PseudoStates state, bool on)
    {
        ElementTree& tree = ControlTree();
        if (!tree.IsAlive(e))
            return;
        PseudoStates s = tree.States(e);
        PseudoStates next = on ? (s | state) : (s & ~state);
        if (u32(next) != u32(s))
            tree.SetStates(e, next);
    }

    void ShowPart(ElementHandle e, bool shown) { SetPartStyle(e, "display", shown ? "flex" : "none"); }

    bool PointerInside(EventSystem& events, ElementHandle e, Vector2 position)
    {
        ElementHandle under = events.Pick(position);
        return under.IsValid() && (under == e || ControlTree().IsAncestor(e, under));
    }

    Rect PartRect(const Element& owner, ElementHandle e)
    {
        Panel* p = owner.panel();
        return p ? p->layout().WorldRect(e) : Rect(0, 0, 0, 0);
    }

    void Listen(Element& self, ElementHandle on, EventTypeId type, EventCallback callback, TrickleDown phase)
    {
        self.world().callbacks().Register(on, type, callback, &self, phase);
    }

    namespace
    {
        // The elements whose text is not parsed for rich text tags.
        HashMap<u64, bool>& PlainText()
        {
            static HashMap<u64, bool>* plain = new HashMap<u64, bool>();
            return *plain;
        }

        u64 Key(ElementHandle e) { return u64(e.index) << 32 | e.generation; }

        String Unescaped(StringView text)
        {
            String out;
            for (usize i = 0; i < text.Size(); i++)
            {
                char c = text[i];
                if (c == '\\' && i + 1 < text.Size())
                {
                    char n = text[i + 1];
                    char r = n == 'n' ? '\n' : n == 't' ? '\t' : n == 'r' ? '\r' : n == '\\' ? '\\' : 0;
                    if (r)
                    {
                        out += r;
                        i++;
                        continue;
                    }
                }
                out += c;
            }
            return out;
        }
    }

    void SetPlainText(ElementHandle e, bool plain)
    {
        if (plain)
            PlainText().Set(Key(e), true);
        else
            PlainText().Remove(Key(e));
    }

    // ------------------------------------------------------------------------------------- BindableElement

    BindableElement::BindableElement() : Element(Intern("BindableElement")) {}
    BindableElement::BindableElement(Atom type) : Element(type) {}
    BindableElement::BindableElement(ElementWorld& world, ElementHandle e) : Element(world, e) {}
    scripting::TypeId BindableElement::HelixType() const { return scripting::TypeOf<BindableElement>(); }

    Span<const ControlAttribute> BindableElement::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"binding-path", [](Element& s, StringView v) { return static_cast<BindableElement&>(s).SetBindingPath(v), true; }},
        };
        return kAttributes;
    }

    // ----------------------------------------------------------------------------------------- TextElement

    TextElement::TextElement() : BindableElement(Intern("TextElement")) { InitText(); }
    TextElement::TextElement(Atom type) : BindableElement(type) { InitText(); }
    TextElement::TextElement(ElementWorld& world, ElementHandle e) : BindableElement(world, e) { InitText(); }
    scripting::TypeId TextElement::HelixType() const { return scripting::TypeOf<TextElement>(); }

    void TextElement::InitText()
    {
        textHandle_ = handle();
        AddToClassList("helix-text-element");
    }

    String TextElement::text() const { return String(world().tree().Text(textHandle_)); }

    void TextElement::SetText(StringView value)
    {
        if (parseEscapeSequences_)
            world().tree().SetText(textHandle_, Unescaped(value).View());
        else
            world().tree().SetText(textHandle_, value);
    }

    void TextElement::SetEnableRichText(bool value)
    {
        enableRichText_ = value;
        if (value)
            PlainText().Remove(Key(textHandle_));
        else
            PlainText().Set(Key(textHandle_), true);
        world().tree().MarkLayoutDirty(textHandle_);
    }

    bool TextElement::RichTextOf(ElementHandle e) { return !PlainText().Find(Key(e)); }

    bool TextElement::isElided() const
    {
        Panel* p = panel();
        const TextInfo* info = p ? p->TextOf(textHandle_) : nullptr;
        return info && info->overflowed;
    }

    Vector2 TextElement::MeasureTextSize(StringView textToMeasure, f32 width, f32 height)
    {
        Panel* p = panel();
        if (!p)
            return Vector2(0, 0);
        Vector2 size = p->MeasureText(textHandle_, textToMeasure, width);
        if (height >= 0 && size.y > height)
            size.y = height;
        return size;
    }

    Span<const ControlAttribute> TextElement::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"text", [](Element& s, StringView v) { return static_cast<TextElement&>(s).SetText(v), true; }},
            {"enable-rich-text",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextElement&>(s).SetEnableRichText(b), true);
             }},
            {"parse-escape-sequences",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextElement&>(s).SetParseEscapeSequences(b), true);
             }},
            {"display-tooltip-when-elided",
             [](Element& s, StringView v) {
                 bool b = false;
                 return ParseBoolText(v, b) && (static_cast<TextElement&>(s).SetDisplayTooltipWhenElided(b), true);
             }},
        };
        return kAttributes;
    }

    // ----------------------------------------------------------------------------------------------- Label

    Label::Label() : TextElement(Intern("Label")) { InitLabel(); }
    Label::Label(StringView text) : TextElement(Intern("Label"))
    {
        InitLabel();
        SetText(text);
    }
    Label::Label(Atom type) : TextElement(type) { InitLabel(); }
    Label::Label(ElementWorld& world, ElementHandle e) : TextElement(world, e) { InitLabel(); }
    scripting::TypeId Label::HelixType() const { return scripting::TypeOf<Label>(); }
    void Label::InitLabel() { AddToClassList("helix-label"); }

    // ---------------------------------------------------------------------------------------------- Button

    Button::Button() : TextElement(Intern("Button")) { InitButton(); }
    Button::Button(Atom type) : TextElement(type) { InitButton(); }
    Button::Button(ElementWorld& world, ElementHandle e) : TextElement(world, e) { InitButton(); }
    scripting::TypeId Button::HelixType() const { return scripting::TypeOf<Button>(); }

    void Button::InitButton()
    {
        AddToClassList("helix-button");
        SetFocusable(true);
        const EventTypes& t = Events();
        Listen(*this, handle(), t.pointerDown, &Button::PointerDown);
        Listen(*this, handle(), t.pointerMove, &Button::PointerMove);
        Listen(*this, handle(), t.pointerUp, &Button::PointerUp);
        Listen(*this, handle(), t.pointerCaptureOut, &Button::CaptureOut);
        Listen(*this, handle(), t.navigationSubmit, &Button::Submit);
    }

    void Button::SetIconImage(engine::Texture2D* value)
    {
        icon_ = engine::PPtr<engine::Texture2D>(value);
        ElementTree& tree = world().tree();
        if (value && !tree.IsAlive(iconElement_))
        {
            iconElement_ = MakePart(ElementHandle(), "helix-button__image", "Element");
            tree.Insert(handle(), 0, iconElement_);
            tree.SetPicking(iconElement_, PickingMode::Ignore);
        }
        if (tree.IsAlive(iconElement_))
        {
            if (Element* icon = Element::Of(world(), iconElement_))
                icon->SetInlineStyleTexture("background-image", value, Vector2(0, 0));
            ShowPart(iconElement_, value != nullptr);
        }
        SetPartClass(handle(), "helix-button--with-icon", value != nullptr);
    }

    void Button::OnPress() {}

    void Button::OnRelease(bool inside)
    {
        if (inside)
            Click();
    }

    void Button::Click()
    {
        clicks_++;
        if (listener_)
            listener_(listenerUser_, *this);
        NotifyControl(handle(), ControlNotify::Clicked);
        clicked_.Invoke();
    }

    void Button::PointerDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Button*>(user);
        if (e.button != 0 || self->pointer_ >= 0 || !self->enabledInHierarchy())
            return;
        self->pointer_ = e.pointerId;
        events.CapturePointer(self->handle(), e.pointerId);
        SetPartState(self->handle(), PseudoStates::Active, true);
        self->OnPress();
        e.StopPropagation();
    }

    void Button::PointerMove(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Button*>(user);
        if (e.pointerId != self->pointer_)
            return;
        SetPartState(self->handle(), PseudoStates::Active, PointerInside(events, self->handle(), e.position));
    }

    void Button::PointerUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<Button*>(user);
        if (e.pointerId != self->pointer_ || e.button != 0)
            return;
        bool inside = PointerInside(events, self->handle(), e.position);
        self->pointer_ = -1;
        SetPartState(self->handle(), PseudoStates::Active, false);
        events.ReleasePointer(self->handle(), e.pointerId);
        self->OnRelease(inside && self->enabledInHierarchy());
        e.StopPropagation();
    }

    void Button::CaptureOut(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<Button*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        SetPartState(self->handle(), PseudoStates::Active, false);
        self->OnRelease(false);
    }

    void Button::Submit(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<Button*>(user);
        if (!self->enabledInHierarchy())
            return;
        self->Click();
        e.StopPropagation();
    }

    // ---------------------------------------------------------------------------------------- RepeatButton

    RepeatButton::RepeatButton() : Button(Intern("RepeatButton")) { InitRepeat(); }
    RepeatButton::RepeatButton(ElementWorld& world, ElementHandle e) : Button(world, e) { InitRepeat(); }
    scripting::TypeId RepeatButton::HelixType() const { return scripting::TypeOf<RepeatButton>(); }
    void RepeatButton::InitRepeat() { AddToClassList("helix-repeat-button"); }

    void RepeatButton::OnPress()
    {
        // Clicked at once, then again after the delay and every interval.
        Click();
        held_ = true;
        next_ = -1;
        if (interval_ > 0)
            StartControlTick(*this, &RepeatButton::Repeat);
    }

    void RepeatButton::OnRelease(bool) { held_ = false; }

    bool RepeatButton::Repeat(Element& self, f64 time)
    {
        auto& b = static_cast<RepeatButton&>(self);
        if (!b.held_ || b.interval_ <= 0)
            return false;
        if (b.next_ < 0)
            b.next_ = time + f64(b.delay_) / 1000.0;
        while (time >= b.next_ && b.held_)
        {
            if (Any(b.world().tree().States(b.handle()) & PseudoStates::Active))
                b.Click();
            b.next_ += f64(b.interval_) / 1000.0;
        }
        return true;
    }

    Span<const ControlAttribute> RepeatButton::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"delay",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<RepeatButton&>(s).SetDelay(n), true);
             }},
            {"interval",
             [](Element& s, StringView v) {
                 i64 n = 0;
                 return ParseIntText(v, n) && (static_cast<RepeatButton&>(s).SetInterval(n), true);
             }},
        };
        return kAttributes;
    }

    // ----------------------------------------------------------------------------------------------- Image

    Image::Image() : Element(Intern("Image")) { InitImage(); }
    Image::Image(ElementWorld& world, ElementHandle e) : Element(world, e) { InitImage(); }
    scripting::TypeId Image::HelixType() const { return scripting::TypeOf<Image>(); }
    void Image::InitImage() { AddToClassList("helix-image"); }

    void Image::SetImage(engine::Texture2D* value)
    {
        image_ = engine::PPtr<engine::Texture2D>(value);
        Apply();
    }

    void Image::SetScaleMode(ScaleMode value)
    {
        scaleMode_ = value;
        Apply();
    }

    void Image::SetTintColor(Color value)
    {
        tint_ = value;
        Apply();
    }

    void Image::SetUv(Rect value)
    {
        uv_ = value;
        Apply();
    }

    Rect Image::sourceRect() const
    {
        engine::Texture2D* t = image_.Get();
        if (!t)
            return Rect(0, 0, 0, 0);
        f32 w = f32(t->width()), h = f32(t->height());
        return Rect(uv_.x * w, (1 - uv_.y - uv_.height) * h, uv_.width * w, uv_.height * h);
    }

    void Image::SetSourceRect(Rect value)
    {
        engine::Texture2D* t = image_.Get();
        if (!t || t->width() <= 0 || t->height() <= 0)
            return;
        f32 w = f32(t->width()), h = f32(t->height());
        SetUv(Rect(value.x / w, 1 - (value.y + value.height) / h, value.width / w, value.height / h));
    }

    void Image::Apply()
    {
        // The texture as the element's background in its scale mode, tinted; a part of it by the background's
        // size and position (the whole texture scaled so the part fills the box).
        SetInlineStyleTexture("background-image", image_.Get(), Vector2(0, 0));
        static const char* kModes[] = {"stretch-to-fill", "scale-and-crop", "scale-to-fit"};
        SetInlineStyleText("-helix-background-scale-mode", kModes[u32(scaleMode_) % 3]);
        SetInlineStyleText("-helix-background-image-tint-color",
                           Format("rgba({}, {}, {}, {})", i32(tint_.r * 255 + 0.5f), i32(tint_.g * 255 + 0.5f), i32(tint_.b * 255 + 0.5f), tint_.a).View());
        bool whole = uv_.x == 0 && uv_.y == 0 && uv_.width == 1 && uv_.height == 1;
        if (!whole && uv_.width > 0 && uv_.height > 0)
        {
            SetInlineStyleText("background-size", Format("{}% {}%", 100 / uv_.width, 100 / uv_.height).View());
            f32 px = uv_.width < 1 ? uv_.x / (1 - uv_.width) * 100 : 0;
            f32 top = 1 - uv_.y - uv_.height;
            f32 py = uv_.height < 1 ? top / (1 - uv_.height) * 100 : 0;
            SetInlineStyleText("background-position-x", Format("{}%", px).View());
            SetInlineStyleText("background-position-y", Format("{}%", py).View());
        }
    }

    // ------------------------------------------------------------------------------------------------- Box

    Box::Box() : Element(Intern("Box")) { AddToClassList("helix-box"); }
    Box::Box(ElementWorld& world, ElementHandle e) : Element(world, e) { AddToClassList("helix-box"); }
    scripting::TypeId Box::HelixType() const { return scripting::TypeOf<Box>(); }

    // -------------------------------------------------------------------------------------------- GroupBox

    GroupBox::GroupBox() : BindableElement(Intern("GroupBox")) { InitGroup(); }
    GroupBox::GroupBox(StringView text) : BindableElement(Intern("GroupBox"))
    {
        InitGroup();
        SetText(text);
    }
    GroupBox::GroupBox(ElementWorld& world, ElementHandle e) : BindableElement(world, e) { InitGroup(); }
    scripting::TypeId GroupBox::HelixType() const { return scripting::TypeOf<GroupBox>(); }

    void GroupBox::InitGroup()
    {
        AddToClassList("helix-group-box");
        label_ = MakePart(handle(), "helix-text-element helix-label helix-group-box__label", "Label");
        ShowPart(label_, false);
    }

    void GroupBox::SetText(StringView value)
    {
        text_ = String(value);
        world().tree().SetText(label_, value);
        ShowPart(label_, !value.Empty());
    }

    Span<const ControlAttribute> GroupBox::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"text", [](Element& s, StringView v) { return static_cast<GroupBox&>(s).SetText(v), true; }},
        };
        return kAttributes;
    }

    // --------------------------------------------------------------------------------------------- HelpBox

    HelpBox::HelpBox() : Element(Intern("HelpBox")) { InitHelp(); }
    HelpBox::HelpBox(StringView text, HelpBoxMessageType messageType) : Element(Intern("HelpBox"))
    {
        InitHelp();
        SetText(text);
        SetMessageType(messageType);
    }
    HelpBox::HelpBox(ElementWorld& world, ElementHandle e) : Element(world, e) { InitHelp(); }
    scripting::TypeId HelpBox::HelixType() const { return scripting::TypeOf<HelpBox>(); }

    void HelpBox::InitHelp()
    {
        AddToClassList("helix-help-box");
        icon_ = MakePart(handle(), "helix-help-box__icon", "Element");
        label_ = MakePart(handle(), "helix-text-element helix-label helix-help-box__label", "Label");
        SetMessageType(type_);
    }

    String HelpBox::text() const { return String(world().tree().Text(label_)); }
    void HelpBox::SetText(StringView value) { world().tree().SetText(label_, value); }

    void HelpBox::SetMessageType(HelpBoxMessageType value)
    {
        type_ = value;
        static const char* kIcons[] = {"helix-icon-info", "helix-icon-warning", "helix-icon-error"};
        for (u32 i = 0; i < 3; i++)
            SetPartClass(icon_, kIcons[i], u32(value) == i + 1);
        SetPartClass(icon_, "helix-help-box__icon--empty", value == HelpBoxMessageType::None);
        ShowPart(icon_, value != HelpBoxMessageType::None);
    }

    Span<const ControlAttribute> HelpBox::MarkupAttributes()
    {
        static const StringView kTypes[] = {"None", "Info", "Warning", "Error"};
        static const ControlAttribute kAttributes[] = {
            {"text", [](Element& s, StringView v) { return static_cast<HelpBox&>(s).SetText(v), true; }},
            {"message-type",
             [](Element& s, StringView v) {
                 i32 k = 0;
                 return ParseEnumText(v, kTypes, k) && (static_cast<HelpBox&>(s).SetMessageType(HelpBoxMessageType(k)), true);
             }},
        };
        return kAttributes;
    }

    // ----------------------------------------------------------------------------------------- PopupWindow

    PopupWindow::PopupWindow() : TextElement(Intern("PopupWindow")) { InitPopup(); }
    PopupWindow::PopupWindow(ElementWorld& world, ElementHandle e) : TextElement(world, e) { InitPopup(); }
    scripting::TypeId PopupWindow::HelixType() const { return scripting::TypeOf<PopupWindow>(); }

    void PopupWindow::InitPopup()
    {
        RemoveFromClassList("helix-text-element");
        AddToClassList("helix-popup-window");
        // The title in its own part, the content under it.
        textHandle_ = MakePart(handle(), "helix-text-element helix-label helix-popup-window__title", "Label");
        content_ = MakePart(handle(), "helix-popup-window__content-container", "Element");
    }

    Element* PopupWindow::contentContainer() { return Element::Of(world(), content_); }

    // ----------------------------------------------------------------------------------------- ProgressBar

    ProgressBar::ProgressBar() : BindableElement(Intern("ProgressBar")) { InitProgress(); }
    ProgressBar::ProgressBar(ElementWorld& world, ElementHandle e) : BindableElement(world, e) { InitProgress(); }
    scripting::TypeId ProgressBar::HelixType() const { return scripting::TypeOf<ProgressBar>(); }

    void ProgressBar::InitProgress()
    {
        AddToClassList("helix-progress-bar");
        ElementHandle container = MakePart(handle(), "helix-progress-bar__container", "Element");
        ElementHandle background = MakePart(container, "helix-progress-bar__background", "Element");
        progress_ = MakePart(background, "helix-progress-bar__progress", "Element");
        ElementHandle titles = MakePart(background, "helix-progress-bar__title-container", "Element");
        title_ = MakePart(titles, "helix-text-element helix-label helix-progress-bar__title", "Label");
        Fill();
    }

    void ProgressBar::SetValue(f32 v)
    {
        value_ = v;
        Fill();
    }

    void ProgressBar::SetLowValue(f32 v)
    {
        low_ = v;
        Fill();
    }

    void ProgressBar::SetHighValue(f32 v)
    {
        high_ = v;
        Fill();
    }

    String ProgressBar::title() const { return String(world().tree().Text(title_)); }
    void ProgressBar::SetTitle(StringView value) { world().tree().SetText(title_, value); }

    void ProgressBar::Fill()
    {
        f32 range = high_ - low_;
        f32 k = range != 0 ? (value_ - low_) / range : 0;
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        SetPartStyle(progress_, "width", Format("{}%", k * 100).View());
    }

    Span<const ControlAttribute> ProgressBar::MarkupAttributes()
    {
        static const ControlAttribute kAttributes[] = {
            {"value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<ProgressBar&>(s).SetValue(f32(n)), true);
             }},
            {"low-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<ProgressBar&>(s).SetLowValue(f32(n)), true);
             }},
            {"high-value",
             [](Element& s, StringView v) {
                 f64 n = 0;
                 return ParseFloatText(v, n) && (static_cast<ProgressBar&>(s).SetHighValue(f32(n)), true);
             }},
            {"title", [](Element& s, StringView v) { return static_cast<ProgressBar&>(s).SetTitle(v), true; }},
        };
        return kAttributes;
    }
}
