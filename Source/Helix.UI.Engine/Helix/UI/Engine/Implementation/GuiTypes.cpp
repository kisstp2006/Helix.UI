#include <Helix/UI/Engine/Gui.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Core/Strings/Format.h>

#include <cmath>

namespace helix::ui::gui
{
    namespace
    {
        struct Registry
        {
            HashMap<u64, GuiObjectBase*> byId;
            u64 next = 0;
        };

        Registry& R()
        {
            static Registry* r = new Registry();
            return *r;
        }
    }

    // ------------------------------------------------------------------------------------------------- objects

    GuiObjectBase::GuiObjectBase() : id_(++R().next) { R().byId.Set(id_, this); }

    GuiObjectBase::~GuiObjectBase()
    {
        R().byId.Remove(id_);
        scripting::ReleaseWrapper(wrapper_);
    }

    GuiObjectBase* GuiObjectBase::Find(u64 id)
    {
        GuiObjectBase** found = R().byId.Find(id);
        return found ? *found : nullptr;
    }

    scripting::TypeId RectOffset::HelixType() const { return scripting::TypeOf<RectOffset>(); }
    scripting::TypeId GuiContent::HelixType() const { return scripting::TypeOf<GuiContent>(); }
    scripting::TypeId GuiStyleState::HelixType() const { return scripting::TypeOf<GuiStyleState>(); }
    scripting::TypeId GuiStyle::HelixType() const { return scripting::TypeOf<GuiStyle>(); }
    scripting::TypeId GuiSettings::HelixType() const { return scripting::TypeOf<GuiSettings>(); }
    scripting::TypeId Event::HelixType() const { return scripting::TypeOf<Event>(); }
    scripting::TypeId GuiLayoutOption::HelixType() const { return scripting::TypeOf<GuiLayoutOption>(); }
    scripting::TypeId TextEditor::HelixType() const { return scripting::TypeOf<TextEditor>(); }

    String RectOffset::ToString() const { return Format("RectOffset (l:{} r:{} t:{} b:{})", left_, right_, top_, bottom_); }

    // ------------------------------------------------------------------------------------------------- content

    GuiContent::GuiContent(GuiContent* source)
    {
        if (source)
        {
            text_ = source->text_;
            tooltip_ = source->tooltip_;
            image_ = source->image_;
        }
    }

    GuiContent* GuiContent::none()
    {
        static GuiContent* none = new GuiContent();
        none->text_.Clear();
        none->tooltip_.Clear();
        none->image_ = {};
        return none;
    }

    GuiContent& GuiContent::Temp(StringView text, engine::Texture2D* image, StringView tooltip)
    {
        // A few slots: a control may take two temporaries at once (a label and its field).
        static GuiContent* slots[4] = {new GuiContent(), new GuiContent(), new GuiContent(), new GuiContent()};
        static u32 next = 0;
        GuiContent& c = *slots[next++ & 3];
        c.text_ = String(text);
        c.tooltip_ = String(tooltip);
        c.image_ = engine::PPtr<engine::Texture2D>(image);
        return c;
    }

    // ------------------------------------------------------------------------------------------------- states

    Array<engine::Texture2D*> GuiStyleState::scaledBackgrounds() const
    {
        Array<engine::Texture2D*> out;
        for (const auto& t : scaled_)
            out.Push(t.Get());
        return out;
    }

    void GuiStyleState::SetScaledBackgrounds(Span<engine::Texture2D* const> v)
    {
        scaled_.Clear();
        for (engine::Texture2D* t : v)
            scaled_.Push(engine::PPtr<engine::Texture2D>(t));
    }

    void GuiStyleState::CopyFrom(const GuiStyleState& o)
    {
        background_ = o.background_;
        scaled_ = o.scaled_;
        textColor_ = o.textColor_;
    }

    // ------------------------------------------------------------------------------------------------- styles

    GuiStyle::GuiStyle()
    {
        for (GuiStyleState*& s : states_)
            s = new GuiStyleState();
        for (RectOffset*& o : offsets_)
            o = new RectOffset();
    }

    GuiStyle::GuiStyle(GuiStyle* other) : GuiStyle()
    {
        if (other)
            CopyFrom(*other);
    }

    GuiStyle::~GuiStyle()
    {
        // Its states and offsets go with it (a C# object of one then finds none).
        for (GuiStyleState* s : states_)
            delete s;
        for (RectOffset* o : offsets_)
            delete o;
    }

    void GuiStyle::CopyFrom(const GuiStyle& o)
    {
        name_ = o.name_;
        for (u32 i = 0; i < 8; i++)
            states_[i]->CopyFrom(*o.states_[i]);
        for (u32 i = 0; i < 4; i++)
            offsets_[i]->CopyFrom(*o.offsets_[i]);
        font_ = o.font_;
        fontSize_ = o.fontSize_;
        fontStyle_ = o.fontStyle_;
        alignment_ = o.alignment_;
        wordWrap_ = o.wordWrap_;
        richText_ = o.richText_;
        clipping_ = o.clipping_;
        imagePosition_ = o.imagePosition_;
        contentOffset_ = o.contentOffset_;
        fixedWidth_ = o.fixedWidth_;
        fixedHeight_ = o.fixedHeight_;
        stretchWidth_ = o.stretchWidth_;
        stretchHeight_ = o.stretchHeight_;
    }

    void GuiStyle::SetState(u32 i, GuiStyleState* v)
    {
        // The reference's states are values set by copy.
        if (v)
            states_[i]->CopyFrom(*v);
    }

    void GuiStyle::SetOffset(u32 i, RectOffset* v)
    {
        if (v)
            offsets_[i]->CopyFrom(*v);
    }

    GuiStyle* GuiStyle::none()
    {
        static GuiStyle* none = [] {
            auto* s = new GuiStyle();
            s->SetName("none");
            return s;
        }();
        return none;
    }

    String GuiStyle::ToString() const { return Format("GUIStyle '{}'", name_.View()); }

    // ------------------------------------------------------------------------------------------------- skin

    GuiSkin::GuiSkin()
    {
        static const char* const kNames[kStyles] = {"box", "label", "textfield", "textarea", "button", "toggle", "window", "horizontalslider",
                                                    "horizontalsliderthumb", "verticalslider", "verticalsliderthumb", "horizontalscrollbar",
                                                    "horizontalscrollbarthumb", "horizontalscrollbarleftbutton", "horizontalscrollbarrightbutton",
                                                    "verticalscrollbar", "verticalscrollbarthumb", "verticalscrollbarupbutton",
                                                    "verticalscrollbardownbutton", "scrollview"};
        for (u32 i = 0; i < kStyles; i++)
        {
            styles_[i] = new GuiStyle();
            styles_[i]->SetName(kNames[i]);
        }
        settings_ = new GuiSettings();
    }

    GuiSkin::~GuiSkin()
    {
        for (GuiStyle* s : styles_)
            delete s;
        for (GuiStyle* s : custom_)
            delete s;
        delete settings_;
    }

    void GuiSkin::SetStyle(u32 i, GuiStyle* v)
    {
        if (v)
            styles_[i]->CopyFrom(*v);
    }

    Array<GuiStyle*> GuiSkin::customStyles() const { return custom_; }

    void GuiSkin::SetCustomStyles(Span<GuiStyle* const> v)
    {
        // The skin keeps copies (a style a script made may be collected with its C# object).
        for (GuiStyle* s : custom_)
            delete s;
        custom_.Clear();
        for (GuiStyle* s : v)
            if (s)
                custom_.Push(new GuiStyle(s));
    }

    GuiStyle* GuiSkin::FindStyle(StringView styleName) const
    {
        auto same = [](StringView a, StringView b) {
            if (a.Size() != b.Size())
                return false;
            for (usize i = 0; i < a.Size(); i++)
            {
                char x = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] + 32) : a[i];
                char y = b[i] >= 'A' && b[i] <= 'Z' ? char(b[i] + 32) : b[i];
                if (x != y)
                    return false;
            }
            return true;
        };
        for (GuiStyle* s : styles_)
            if (same(s->name().View(), styleName))
                return s;
        for (GuiStyle* s : custom_)
            if (same(s->name().View(), styleName))
                return s;
        return nullptr;
    }

    Array<GuiStyle*> GuiSkin::AllStyles() const
    {
        Array<GuiStyle*> all;
        for (GuiStyle* s : styles_)
            all.Push(s);
        for (GuiStyle* s : custom_)
            all.Push(s);
        return all;
    }

    GuiStyle* GuiSkin::GetStyle(StringView styleName) const
    {
        GuiStyle* s = FindStyle(styleName);
        return s ? s : GuiStyle::none();
    }

    namespace
    {
        // A rounded box image: fill inside, a border ring, transparent outside (straight alpha, rows up).
        engine::Texture2D* RoundedImage(u32 size, f32 radius, Color fill, Color border, f32 borderWidth)
        {
            engine::Texture2D* t = engine::Texture2D::Create(i32(size), i32(size), engine::TextureFormat::RGBA32, false, true);
            t->hideFlags = engine::HideFlags::DontSave;
            Array<Color32> pixels;
            pixels.Resize(usize(size) * size);
            f32 half = f32(size) * 0.5f;
            for (u32 y = 0; y < size; y++)
                for (u32 x = 0; x < size; x++)
                {
                    // The distance outside the rounded box (negative inside).
                    f32 px = std::fabs(f32(x) + 0.5f - half) - (half - radius), py = std::fabs(f32(y) + 0.5f - half) - (half - radius);
                    f32 ox = px > 0 ? px : 0, oy = py > 0 ? py : 0;
                    f32 d = std::sqrt(ox * ox + oy * oy) + (px > py ? (px < 0 ? px : 0) : (py < 0 ? py : 0)) - radius;
                    f32 cover = d < -0.5f ? 1 : d > 0.5f ? 0 : 0.5f - d;
                    f32 ring = borderWidth > 0 ? (d > -borderWidth - 0.5f ? (d > -borderWidth + 0.5f ? 1 : d + borderWidth + 0.5f) : 0) : 0;
                    Color c = Color(fill.r + (border.r - fill.r) * ring, fill.g + (border.g - fill.g) * ring, fill.b + (border.b - fill.b) * ring,
                                    (fill.a + (border.a - fill.a) * ring) * cover);
                    pixels[usize(y) * size + x] = Color32(u8(std::lround(c.r * 255)), u8(std::lround(c.g * 255)), u8(std::lround(c.b * 255)),
                                                          u8(std::lround(c.a * 255)));
                }
            t->SetPixels32(Span<const Color32>(pixels.Data(), pixels.Size()));
            t->Apply(false);
            return t;
        }

        // An image in the top left corner of a larger transparent one (a toggle's box: the 9-slice stretches the
        // transparent part, the box keeps its size).
        engine::Texture2D* Corner(engine::Texture2D* box, u32 size)
        {
            u32 bw = u32(box->width()), bh = u32(box->height());
            engine::Texture2D* t = engine::Texture2D::Create(i32(size), i32(size), engine::TextureFormat::RGBA32, false, true);
            t->hideFlags = engine::HideFlags::DontSave;
            Array<Color32> from = box->GetPixels32(0);
            Array<Color32> pixels;
            pixels.Resize(usize(size) * size);
            for (Color32& c : pixels)
                c = Color32(0, 0, 0, 0);
            // Rows go up: the box's rows are the top ones.
            for (u32 y = 0; y < bh; y++)
                for (u32 x = 0; x < bw; x++)
                    pixels[usize(size - bh + y) * size + x] = from[usize(y) * bw + x];
            t->SetPixels32(Span<const Color32>(pixels.Data(), pixels.Size()));
            t->Apply(false);
            engine::Object::DestroyImmediate(box);
            return t;
        }

        // A check mark over a box (the toggle's on states).
        engine::Texture2D* CheckImage(Color box, Color mark)
        {
            constexpr u32 n = 14;
            engine::Texture2D* t = RoundedImage(n, 3, box, Color(0, 0, 0, 0.35f), 1);
            Array<Color32> pixels = t->GetPixels32(0);
            auto put = [&](f32 x, f32 y) {
                for (i32 dy = -1; dy <= 1; dy++)
                    for (i32 dx = -1; dx <= 1; dx++)
                    {
                        i32 px = i32(std::lround(x)) + dx, py = i32(std::lround(y)) + dy;
                        if (px < 0 || py < 0 || px >= i32(n) || py >= i32(n))
                            continue;
                        f32 a = (dx == 0 && dy == 0) ? 1.0f : 0.45f;
                        Color32& p = pixels[usize(py) * n + usize(px)];
                        p = Color32(u8(p.r + (mark.r * 255 - p.r) * a), u8(p.g + (mark.g * 255 - p.g) * a), u8(p.b + (mark.b * 255 - p.b) * a), 255);
                    }
            };
            // Rows go up: the mark's short stroke low on the left, the long one up to the right.
            for (f32 k = 0; k <= 1; k += 0.1f)
                put(3 + 2.2f * k, 7.5f - 2.6f * k);
            for (f32 k = 0; k <= 1; k += 0.07f)
                put(5.2f + 5.6f * k, 4.9f + 5.6f * k);
            t->SetPixels32(Span<const Color32>(pixels.Data(), pixels.Size()));
            t->Apply(false);
            return t;
        }

        void Look(GuiStyle& s, engine::Texture2D* normal, engine::Texture2D* hover, engine::Texture2D* active, Color text, Color hoverText)
        {
            s.normal()->SetBackground(normal);
            s.normal()->SetTextColor(text);
            s.hover()->SetBackground(hover ? hover : normal);
            s.hover()->SetTextColor(hoverText);
            s.active()->SetBackground(active ? active : normal);
            s.active()->SetTextColor(hoverText);
            s.focused()->SetBackground(hover ? hover : normal);
            s.focused()->SetTextColor(hoverText);
            s.onNormal()->SetBackground(active ? active : normal);
            s.onNormal()->SetTextColor(hoverText);
            s.onHover()->SetBackground(active ? active : normal);
            s.onHover()->SetTextColor(hoverText);
            s.onActive()->SetBackground(active ? active : normal);
            s.onActive()->SetTextColor(hoverText);
            s.onFocused()->SetBackground(active ? active : normal);
            s.onFocused()->SetTextColor(hoverText);
        }
    }

    GuiSkin* GuiSkin::Default()
    {
        // Made again when it (or its images) went with the objects (a scene's teardown destroys every object).
        static engine::PPtr<GuiSkin> skin;
        static engine::PPtr<engine::Texture2D> probe;
        static engine::PPtr<FontAsset> font;
        if (GuiSkin* k = skin.Get(); k && probe.Get())
            return k;
        if (GuiSkin* old = skin.Get())
        {
            font = engine::PPtr<FontAsset>(old->font());
            engine::Object::DestroyImmediate(old);
        }
        GuiSkin* made = [] {
            auto* k = static_cast<GuiSkin*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<GuiSkin>()));
            k->hideFlags = engine::HideFlags::DontSave;
            k->SetName("GameSkin");
            const Color text(0.86f, 0.86f, 0.86f, 1), bright(1, 1, 1, 1);
            engine::Texture2D* box = RoundedImage(16, 4, Color(0, 0, 0, 0.45f), Color(1, 1, 1, 0.06f), 1);
            engine::Texture2D* button = RoundedImage(16, 4, Color(0.26f, 0.26f, 0.27f, 0.92f), Color(0.07f, 0.07f, 0.08f, 0.95f), 1);
            engine::Texture2D* buttonHover = RoundedImage(16, 4, Color(0.33f, 0.33f, 0.35f, 0.95f), Color(0.07f, 0.07f, 0.08f, 0.95f), 1);
            engine::Texture2D* buttonActive = RoundedImage(16, 4, Color(0.18f, 0.18f, 0.19f, 0.95f), Color(0.07f, 0.07f, 0.08f, 0.95f), 1);
            engine::Texture2D* field = RoundedImage(16, 3, Color(0.12f, 0.12f, 0.13f, 0.92f), Color(0.05f, 0.05f, 0.06f, 0.95f), 1);
            engine::Texture2D* fieldFocus = RoundedImage(16, 3, Color(0.12f, 0.12f, 0.13f, 0.95f), Color(0.27f, 0.55f, 0.92f, 1), 1);
            engine::Texture2D* window = RoundedImage(32, 6, Color(0.17f, 0.17f, 0.18f, 0.96f), Color(0.04f, 0.04f, 0.05f, 1), 1);
            engine::Texture2D* track = RoundedImage(8, 2, Color(0.08f, 0.08f, 0.09f, 0.9f), Color(0, 0, 0, 0), 0);
            engine::Texture2D* thumb = RoundedImage(12, 4, Color(0.62f, 0.62f, 0.64f, 1), Color(0.1f, 0.1f, 0.1f, 1), 1);
            engine::Texture2D* thumbHover = RoundedImage(12, 4, Color(0.78f, 0.78f, 0.8f, 1), Color(0.1f, 0.1f, 0.1f, 1), 1);
            engine::Texture2D* toggleOff = Corner(RoundedImage(14, 3, Color(0.12f, 0.12f, 0.13f, 0.95f), Color(0.04f, 0.04f, 0.05f, 1), 1), 18);
            engine::Texture2D* toggleOffHover = Corner(RoundedImage(14, 3, Color(0.18f, 0.18f, 0.19f, 0.95f), Color(0.04f, 0.04f, 0.05f, 1), 1), 18);
            engine::Texture2D* toggleOn = Corner(CheckImage(Color(0.27f, 0.55f, 0.92f, 1), Color(1, 1, 1, 1)), 18);

            GuiStyle& b = *k->box();
            Look(b, box, nullptr, nullptr, text, text);
            b.border()->Set(6, 6, 6, 6);
            b.padding()->Set(4, 4, 4, 4);
            b.margin()->Set(4, 4, 4, 4);
            b.SetAlignment(TextAnchor::UpperCenter);

            GuiStyle& l = *k->label();
            Look(l, nullptr, nullptr, nullptr, text, text);
            l.padding()->Set(2, 2, 1, 2);
            l.margin()->Set(4, 4, 2, 2);
            l.SetWordWrap(true);

            GuiStyle& bt = *k->button();
            Look(bt, button, buttonHover, buttonActive, text, bright);
            bt.border()->Set(6, 6, 6, 6);
            bt.padding()->Set(6, 6, 3, 3);
            bt.margin()->Set(4, 4, 2, 2);
            bt.SetAlignment(TextAnchor::MiddleCenter);

            GuiStyle& tf = *k->textField();
            Look(tf, field, field, fieldFocus, text, bright);
            tf.focused()->SetBackground(fieldFocus);
            tf.onNormal()->SetBackground(fieldFocus);
            tf.border()->Set(4, 4, 4, 4);
            tf.padding()->Set(4, 4, 3, 3);
            tf.margin()->Set(4, 4, 2, 2);
            tf.SetClipping(TextClipping::Clip);
            tf.SetWordWrap(false);
            GuiStyle& ta = *k->textArea();
            ta.CopyFrom(tf);
            ta.SetName("textarea");
            ta.SetWordWrap(true);

            GuiStyle& tg = *k->toggle();
            Look(tg, toggleOff, toggleOffHover, toggleOffHover, text, bright);
            tg.onNormal()->SetBackground(toggleOn);
            tg.onHover()->SetBackground(toggleOn);
            tg.onActive()->SetBackground(toggleOn);
            tg.onFocused()->SetBackground(toggleOn);
            tg.border()->Set(16, 0, 16, 0);
            tg.padding()->Set(18, 0, 2, 2);
            tg.margin()->Set(4, 4, 2, 2);
            // The box down to the middle of the text line (the background drawn lower, as the reference skin does).
            tg.overflow()->Set(0, 0, -3, 3);
            tg.SetFixedHeight(0);
            tg.SetImagePosition(ImagePosition::ImageLeft);

            GuiStyle& w = *k->window();
            Look(w, window, nullptr, nullptr, text, bright);
            w.onNormal()->SetBackground(window);
            w.border()->Set(8, 8, 20, 8);
            w.padding()->Set(8, 8, 22, 8);
            w.margin()->Set(0, 0, 0, 0);
            w.SetAlignment(TextAnchor::UpperCenter);
            w.SetContentOffset(Vector2(0, -18));
            w.SetFontStyle(FontStyle::Bold);

            GuiStyle& hs = *k->horizontalSlider();
            Look(hs, track, nullptr, nullptr, text, text);
            hs.border()->Set(3, 3, 0, 0);
            hs.padding()->Set(-1, -1, 0, 0);
            hs.margin()->Set(4, 4, 2, 2);
            hs.SetFixedHeight(16);
            hs.overflow()->Set(0, 0, -6, -6);
            GuiStyle& ht = *k->horizontalSliderThumb();
            Look(ht, thumb, thumbHover, thumbHover, text, text);
            ht.border()->Set(4, 4, 4, 4);
            ht.SetFixedWidth(12);
            ht.SetFixedHeight(12);
            ht.overflow()->Set(0, 0, 0, 0);
            GuiStyle& vs = *k->verticalSlider();
            Look(vs, track, nullptr, nullptr, text, text);
            vs.border()->Set(0, 0, 3, 3);
            vs.padding()->Set(0, 0, -1, -1);
            vs.margin()->Set(2, 2, 4, 4);
            vs.SetFixedWidth(16);
            vs.SetStretchHeight(true);
            vs.SetStretchWidth(false);
            vs.overflow()->Set(-6, -6, 0, 0);
            GuiStyle& vt = *k->verticalSliderThumb();
            Look(vt, thumb, thumbHover, thumbHover, text, text);
            vt.border()->Set(4, 4, 4, 4);
            vt.SetFixedWidth(12);
            vt.SetFixedHeight(12);
            vt.overflow()->Set(0, 0, 0, 0);

            for (u32 i : {11u, 15u})
            {
                GuiStyle& sb = *k->styles_[i];
                Look(sb, track, nullptr, nullptr, text, text);
                sb.border()->Set(2, 2, 2, 2);
                sb.margin()->Set(1, 1, 1, 1);
                if (i == 11)
                    sb.SetFixedHeight(12);
                else
                {
                    sb.SetFixedWidth(12);
                    sb.SetStretchHeight(true);
                    sb.SetStretchWidth(false);
                }
            }
            for (u32 i : {12u, 16u})
            {
                GuiStyle& st = *k->styles_[i];
                Look(st, thumb, thumbHover, thumbHover, text, text);
                st.border()->Set(4, 4, 4, 4);
                st.padding()->Set(5, 5, 5, 5);
                if (i == 12)
                    st.SetFixedHeight(10);
                else
                    st.SetFixedWidth(10);
            }
            for (u32 i : {13u, 14u, 17u, 18u})
                k->styles_[i]->SetFixedWidth(0), k->styles_[i]->SetFixedHeight(0);
            GuiStyle& sv = *k->scrollView();
            sv.padding()->Set(0, 0, 0, 0);
            return k;
        }();
        made->SetFont(font.Get());
        skin = engine::PPtr<GuiSkin>(made);
        probe = engine::PPtr<engine::Texture2D>(made->button()->normal()->background());
        return made;
    }

    // ------------------------------------------------------------------------------------------------- event

    Event::Event(Event* other)
    {
        if (other)
            CopyFrom(*other);
    }

    void Event::CopyFrom(const Event& o)
    {
        type_ = o.type_;
        rawType_ = o.rawType_;
        mousePosition_ = o.mousePosition_;
        delta_ = o.delta_;
        pointerType_ = o.pointerType_;
        button_ = o.button_;
        modifiers_ = o.modifiers_;
        pressure_ = o.pressure_;
        clickCount_ = o.clickCount_;
        character_ = o.character_;
        commandName_ = o.commandName_;
        keyCode_ = o.keyCode_;
        displayIndex_ = o.displayIndex_;
    }

    bool Event::isMouse() const
    {
        return type_ == EventType::MouseDown || type_ == EventType::MouseUp || type_ == EventType::MouseMove || type_ == EventType::MouseDrag ||
               type_ == EventType::ContextClick || type_ == EventType::MouseEnterWindow || type_ == EventType::MouseLeaveWindow;
    }

    void Event::Use()
    {
        if (type_ == EventType::Repaint || type_ == EventType::Layout)
            return;
        type_ = EventType::Used;
    }

    Event* Event::KeyboardEvent(StringView key)
    {
        // "a", "#a" (shift), "^a" (control), "%a" (command), "&a" (alt), and named keys ("return", "escape", "up").
        auto* e = new Event();
        e->type_ = EventType::KeyDown;
        usize at = 0;
        i32 mods = 0;
        while (at < key.Size() && (key[at] == '#' || key[at] == '^' || key[at] == '%' || key[at] == '&'))
        {
            mods |= key[at] == '#' ? i32(EventModifiers::Shift) : key[at] == '^' ? i32(EventModifiers::Control) : key[at] == '%' ? i32(EventModifiers::Command)
                                                                                                                     : i32(EventModifiers::Alt);
            at++;
        }
        e->modifiers_ = EventModifiers(mods);
        StringView name = key.Substr(at);
        struct Named
        {
            StringView name;
            i32 code;
        };
        static const Named kNamed[] = {{"backspace", 8}, {"tab", 9}, {"return", 13}, {"enter", 271}, {"escape", 27}, {"space", 32}, {"delete", 127},
                                       {"up", 273},      {"down", 274}, {"right", 275}, {"left", 276},   {"insert", 277}, {"home", 278}, {"end", 279},
                                       {"pageup", 280},  {"pagedown", 281}, {"f1", 282}, {"f2", 283},   {"f3", 284},     {"f4", 285},  {"f5", 286}};
        for (const Named& n : kNamed)
            if (n.name == name)
                e->keyCode_ = platform::KeyCode(n.code);
        if (e->keyCode_ == platform::KeyCode::None && name.Size() == 1)
        {
            char c = name[0];
            e->keyCode_ = platform::KeyCode(c >= 'A' && c <= 'Z' ? c + 32 : c);
            e->character_ = char16_t(c);
        }
        return e;
    }

    String Event::ToString() const
    {
        return Format("Event:{} Character:{} Modifiers:{} KeyCode:{}", i32(type_), i32(character_), i32(modifiers_), i32(keyCode_));
    }
}
