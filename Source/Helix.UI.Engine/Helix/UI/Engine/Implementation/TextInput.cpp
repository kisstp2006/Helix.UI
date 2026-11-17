#include <Helix/UI/Engine/TextInput.h>

#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Input/GUIUtility.h>
#include <Helix/UI/Engine/ControlParts.h>
#include <Helix/UI/Engine/Panel.h>

#include <cmath>
#include <utility>

namespace helix::ui
{
    namespace
    {
        // The script API's key codes.
        constexpr u16 kKeyBackspace = 8, kKeyTab = 9, kKeyReturn = 13, kKeyEscape = 27, kKeyDelete = 127, kKeyKeypadEnter = 271;
        constexpr u16 kKeyUp = 273, kKeyDown = 274, kKeyRight = 275, kKeyLeft = 276, kKeyHome = 278, kKeyEnd = 279;
        constexpr u16 kKeyA = 97, kKeyC = 99, kKeyV = 118, kKeyX = 120;
        constexpr u8 kShift = 1, kControl = 2, kCommand = 8;

        void AppendUtf8(String& out, u32 c)
        {
            char b[4];
            u32 n = 0;
            if (c < 0x80)
                b[n++] = char(c);
            else if (c < 0x800)
                b[n++] = char(0xC0 | (c >> 6)), b[n++] = char(0x80 | (c & 0x3F));
            else if (c < 0x10000)
                b[n++] = char(0xE0 | (c >> 12)), b[n++] = char(0x80 | ((c >> 6) & 0x3F)), b[n++] = char(0x80 | (c & 0x3F));
            else
                b[n++] = char(0xF0 | (c >> 18)), b[n++] = char(0x80 | ((c >> 12) & 0x3F)), b[n++] = char(0x80 | ((c >> 6) & 0x3F)),
                b[n++] = char(0x80 | (c & 0x3F));
            out += StringView(b, n);
        }

        Array<u32> Codepoints(StringView s)
        {
            Array<u32> out;
            for (usize i = 0; i < s.Size();)
            {
                u8 c = u8(s[i]);
                u32 cp = c, extra = 0;
                if (c >= 0xF0)
                    cp = c & 0x07, extra = 3;
                else if (c >= 0xE0)
                    cp = c & 0x0F, extra = 2;
                else if (c >= 0xC0)
                    cp = c & 0x1F, extra = 1;
                i++;
                for (u32 k = 0; k < extra && i < s.Size(); k++, i++)
                    cp = (cp << 6) | (u8(s[i]) & 0x3F);
                out.Push(cp);
            }
            return out;
        }

        bool WordCharacter(u32 c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c > 127; }
    }

    TextInput::TextInput(AbstractBaseField& field) : field_(field) {}

    void TextInput::Build(ElementHandle input)
    {
        input_ = input;
        ElementTree& tree = ControlTree();
        text_ = MakePart(input, "helix-text-element helix-text-element--inner-input-field-component", "TextElement");
        SetPlainText(text_, true);
        SetPartStyle(text_, "white-space", "pre");
        // The caret and the selection beside the text (an element with children is not measured as a text).
        caret_ = MakePart(input, "helix-text-input__caret");
        tree.SetPicking(caret_, PickingMode::Ignore);
        ShowPart(caret_, false);
        field_.SetFocusable(true);
        const EventTypes& t = Events();
        EventCallbacks& callbacks = field_.world().callbacks();
        callbacks.Register(input_, t.pointerDown, &TextInput::PointerDown, this, TrickleDown::NoTrickleDown);
        callbacks.Register(input_, t.pointerMove, &TextInput::PointerMove, this, TrickleDown::NoTrickleDown);
        callbacks.Register(input_, t.pointerUp, &TextInput::PointerUp, this, TrickleDown::NoTrickleDown);
        callbacks.Register(field_.handle(), t.keyDown, &TextInput::KeyDown, this, TrickleDown::NoTrickleDown);
        callbacks.Register(field_.handle(), t.composition, &TextInput::Composition, this, TrickleDown::NoTrickleDown);
        callbacks.Register(field_.handle(), t.focusIn, &TextInput::FocusIn, this, TrickleDown::NoTrickleDown);
        callbacks.Register(field_.handle(), t.focusOut, &TextInput::FocusOut, this, TrickleDown::NoTrickleDown);
        Show();
    }

    String TextInput::Text() const
    {
        String out;
        for (u32 c : codepoints_)
            AppendUtf8(out, c);
        return out;
    }

    void TextInput::SetText(StringView text)
    {
        codepoints_.Clear();
        for (u32 c : Codepoints(text))
            if (options_[AbstractBaseField::Multiline] || (c != '\n' && c != '\r'))
                codepoints_.Push(c);
        i32 max = options_[AbstractBaseField::MaxLength];
        if (max >= 0 && i32(codepoints_.Size()) > max)
            codepoints_.Resize(usize(max));
        cursor_ = select_ = i32(codepoints_.Size());
        Show();
    }

    i32 TextInput::Option(i32 option) const
    {
        if (option == AbstractBaseField::CursorIndex)
            return cursor_;
        if (option == AbstractBaseField::SelectIndex)
            return select_;
        return option >= 0 && option < 13 ? options_[option] : 0;
    }

    void TextInput::SetOption(i32 option, i32 value)
    {
        if (option == AbstractBaseField::CursorIndex || option == AbstractBaseField::SelectIndex)
        {
            Select(option == AbstractBaseField::CursorIndex ? value : cursor_, option == AbstractBaseField::SelectIndex ? value : select_);
            return;
        }
        if (option < 0 || option >= 13)
            return;
        options_[option] = value;
        if (option == AbstractBaseField::Multiline)
        {
            SetPartStyle(text_, "white-space", value ? "pre-wrap" : "pre");
            SetPartClass(field_.handle(), "helix-text-field--multiline", value != 0);
        }
        if (option == AbstractBaseField::ReadOnly)
            SetPartClass(field_.handle(), "helix-text-field--read-only", value != 0);
        Show();
    }

    void TextInput::Select(i32 cursor, i32 select)
    {
        i32 n = i32(codepoints_.Size());
        cursor_ = cursor < 0 ? 0 : cursor > n ? n : cursor;
        select_ = select < 0 ? 0 : select > n ? n : select;
        dirty_ = true;
        blink_ = 0;
    }

    bool TextInput::Accepts(u32 c) const
    {
        if (c < 32 && !(c == '\n' && options_[AbstractBaseField::Multiline]) && c != '\t')
            return false;
        if (c == '\t' && !options_[AbstractBaseField::Multiline])
            return false;
        switch (options_[AbstractBaseField::Characters])
        {
        case 1: return (c >= '0' && c <= '9') || c == '-' || c == '+';
        case 2: return (c >= '0' && c <= '9') || c == '+';
        case 3: return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E' || c == ',';
        case 4: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        default: return c != 127;
        }
    }

    void TextInput::Insert(Span<const u32> text)
    {
        DeleteSelection();
        i32 max = options_[AbstractBaseField::MaxLength];
        for (u32 c : text)
        {
            if (!Accepts(c))
                continue;
            if (max >= 0 && i32(codepoints_.Size()) >= max)
                break;
            codepoints_.Insert(usize(cursor_), c);
            cursor_++;
        }
        select_ = cursor_;
    }

    void TextInput::Erase(i32 from, i32 to)
    {
        if (from > to)
            std::swap(from, to);
        from = from < 0 ? 0 : from;
        to = to > i32(codepoints_.Size()) ? i32(codepoints_.Size()) : to;
        for (i32 i = to; i-- > from;)
            codepoints_.RemoveAt(usize(i));
        cursor_ = select_ = from;
    }

    void TextInput::DeleteSelection()
    {
        if (cursor_ != select_)
            Erase(cursor_, select_);
    }

    void TextInput::Changed()
    {
        Show();
        if (!options_[AbstractBaseField::Delayed])
            field_.OnTextChanged(false);
    }

    String TextInput::Selected() const
    {
        i32 a = cursor_ < select_ ? cursor_ : select_, b = cursor_ < select_ ? select_ : cursor_;
        String out;
        for (i32 i = a; i < b; i++)
            AppendUtf8(out, codepoints_[usize(i)]);
        return out;
    }

    void TextInput::Show()
    {
        // The shown text: the text (or its mask) with what the input method composes at the cursor.
        String shown;
        starts_.Clear();
        bool password = options_[AbstractBaseField::Password] != 0;
        u32 mask = u32(options_[AbstractBaseField::MaskChar] > 0 ? options_[AbstractBaseField::MaskChar] : '*');
        for (usize i = 0; i <= codepoints_.Size(); i++)
        {
            if (i32(i) == cursor_)
                for (u32 c : composition_)
                    AppendUtf8(shown, c);
            if (i == codepoints_.Size())
                break;
            starts_.Push(u32(shown.Size()));
            AppendUtf8(shown, password ? mask : codepoints_[i]);
        }
        ControlTree().SetText(text_, shown.View());
        dirty_ = true;
    }

    Vector2 TextInput::Origin() const
    {
        Panel* p = field_.panel();
        if (!p)
            return Vector2(0, 0);
        Rect r = p->layout().WorldRect(text_);
        return Vector2(r.x, r.y);
    }

    i32 TextInput::IndexAt(Vector2 local) const
    {
        Panel* p = field_.panel();
        const TextInfo* info = p ? p->TextOf(text_) : nullptr;
        if (!info || info->characterCount == 0)
            return 0;
        // The line under the point, then the nearest boundary of its characters.
        u32 line = 0;
        for (u32 l = 0; l < info->lineCount && l < info->lineInfo.Size(); l++)
        {
            const CharacterInfo& first = info->characterInfo[info->lineInfo[l].firstCharacterIndex];
            if (local.y >= first.ascender - 1 || l == 0)
                line = l;
        }
        const LineInfo& li = info->lineInfo[line];
        i32 best = -1;
        for (u32 c = li.firstCharacterIndex; c <= li.lastCharacterIndex && c < info->characterInfo.Size(); c++)
        {
            const CharacterInfo& ci = info->characterInfo[c];
            usize k = 0;
            while (k + 1 < starts_.Size() && starts_[k + 1] <= ci.index)
                k++;
            if (local.x < ci.origin + ci.xAdvance * 0.5f)
                return i32(k);
            best = i32(k) + 1;
        }
        return best < 0 ? 0 : best;
    }

    i32 TextInput::WordStart(i32 i) const
    {
        while (i > 0 && !WordCharacter(codepoints_[usize(i - 1)]))
            i--;
        while (i > 0 && WordCharacter(codepoints_[usize(i - 1)]))
            i--;
        return i;
    }

    i32 TextInput::WordEnd(i32 i) const
    {
        i32 n = i32(codepoints_.Size());
        while (i < n && !WordCharacter(codepoints_[usize(i)]))
            i++;
        while (i < n && WordCharacter(codepoints_[usize(i)]))
            i++;
        return i;
    }

    void TextInput::PlaceCaret()
    {
        Panel* p = field_.panel();
        if (!p)
            return;
        const TextInfo* info = p->TextOf(text_);
        f32 size = p->styles().Computed(text_).fontSize.value;
        size = size > 0 ? size : 13;
        // Where an index's caret goes: x, the top and the height (in the text element).
        auto at = [&](i32 index, f32& x, f32& top, f32& height) {
            x = 0, top = 0, height = size * 1.2f;
            if (!info || info->characterInfo.Empty())
                return;
            usize shownIndex = usize(index) < starts_.Size() ? starts_[usize(index)] : ~0u;
            const CharacterInfo* last = nullptr;
            for (const CharacterInfo& ci : info->characterInfo)
            {
                if (ci.index == shownIndex)
                {
                    x = ci.origin, top = ci.ascender < ci.descender ? ci.ascender : ci.descender;
                    height = ci.descender > ci.ascender ? ci.descender - ci.ascender : ci.ascender - ci.descender;
                    return;
                }
                last = &ci;
            }
            if (last)
            {
                x = last->origin + last->xAdvance;
                top = last->ascender < last->descender ? last->ascender : last->descender;
                height = last->descender > last->ascender ? last->descender - last->ascender : last->ascender - last->descender;
            }
        };
        // From the text's box to the input's padding box (where the caret and the selection are placed).
        Rect textRect = p->layout().WorldRect(text_), inputRect = p->layout().WorldRect(input_);
        const LayoutBox& inputBox = p->layout().Box(input_);
        f32 ox = textRect.x - inputRect.x - inputBox.border[0], oy = textRect.y - inputRect.y - inputBox.border[1];
        f32 x = 0, top = 0, height = 0;
        at(cursor_, x, top, height);
        SetPartStyle(caret_, "left", Format("{}px", x + ox).View());
        SetPartStyle(caret_, "top", Format("{}px", top + oy).View());
        SetPartStyle(caret_, "height", Format("{}px", height).View());
        // The selection: a box per line it spans.
        i32 a = cursor_ < select_ ? cursor_ : select_, b = cursor_ < select_ ? select_ : cursor_;
        usize used = 0;
        if (a != b && focused_)
        {
            i32 start = a;
            while (start < b)
            {
                f32 x0 = 0, t0 = 0, h0 = 0;
                at(start, x0, t0, h0);
                i32 end = start;
                f32 x1 = x0, t1 = t0, h1 = h0;
                while (end < b)
                {
                    f32 nx = 0, nt = 0, nh = 0;
                    at(end + 1, nx, nt, nh);
                    if (nt != t0 && end + 1 < b)
                        break;
                    end++;
                    x1 = nx, t1 = nt, h1 = nh;
                    if (nt != t0)
                        break;
                }
                (void)t1, (void)h1;
                if (used == boxes_.Size())
                {
                    ElementHandle box = MakePart(ElementHandle(), "helix-text-input__selection");
                    ControlTree().Insert(input_, 0, box);
                    ControlTree().SetPicking(box, PickingMode::Ignore);
                    boxes_.Push(box);
                }
                ElementHandle box = boxes_[used++];
                ShowPart(box, true);
                SetPartStyle(box, "left", Format("{}px", x0 + ox).View());
                SetPartStyle(box, "top", Format("{}px", t0 + oy).View());
                SetPartStyle(box, "width", Format("{}px", x1 > x0 ? x1 - x0 : 2).View());
                SetPartStyle(box, "height", Format("{}px", h0).View());
                start = end > start ? end : start + 1;
            }
        }
        for (usize i = used; i < boxes_.Size(); i++)
            ShowPart(boxes_[i], false);
        if (focused_)
            if (EventSystem* events = field_.events())
            {
                Vector2 o = Origin();
                events->SetTextInput(field_.handle(), Rect(o.x + x, o.y + top, 1, height));
            }
    }

    // ------------------------------------------------------------------------------------------------ events

    void TextInput::PointerDown(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<TextInput*>(user);
        if (e.button != 0 || !self->field_.enabledInHierarchy())
            return;
        events.Focus(self->field_.handle());
        Vector2 o = self->Origin();
        i32 i = self->IndexAt(Vector2(e.position.x - o.x, e.position.y - o.y));
        self->clicks_ = u32(e.clickCount);
        if (e.clickCount == 2 && self->options_[AbstractBaseField::DoubleClickSelectsWord])
            self->Select(self->WordEnd(i), self->WordStart(i));
        else if (e.clickCount >= 3 && self->options_[AbstractBaseField::TripleClickSelectsLine])
            self->Select(i32(self->codepoints_.Size()), 0);
        else
            self->Select(i, (e.modifiers & kShift) ? self->select_ : i);
        self->pointer_ = e.pointerId;
        events.CapturePointer(self->input_, e.pointerId);
        e.StopPropagation();
    }

    void TextInput::PointerMove(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<TextInput*>(user);
        if (e.pointerId != self->pointer_ || self->clicks_ > 1)
            return;
        Vector2 o = self->Origin();
        self->Select(self->IndexAt(Vector2(e.position.x - o.x, e.position.y - o.y)), self->select_);
    }

    void TextInput::PointerUp(void* user, EventSystem& events, UIEvent& e)
    {
        auto* self = static_cast<TextInput*>(user);
        if (e.pointerId != self->pointer_)
            return;
        self->pointer_ = -1;
        events.ReleasePointer(self->input_, e.pointerId);
        if (self->options_[AbstractBaseField::SelectAllOnMouseUp] && self->cursor_ == self->select_ && self->clicks_ == 1 && self->justFocused_)
            self->Select(i32(self->codepoints_.Size()), 0);
        self->justFocused_ = false;
    }

    void TextInput::KeyDown(void* user, EventSystem&, UIEvent& e)
    {
        auto* self = static_cast<TextInput*>(user);
        if (!self->focused_ || !self->field_.enabledInHierarchy())
            return;
        bool readOnly = self->options_[AbstractBaseField::ReadOnly] != 0;
        bool shift = (e.modifiers & kShift) != 0, command = (e.modifiers & (kControl | kCommand)) != 0;
        i32 n = i32(self->codepoints_.Size());
        auto move = [&](i32 to) {
            if (!shift && self->cursor_ != self->select_ && (e.keyCode == kKeyLeft || e.keyCode == kKeyRight) && !command)
                to = e.keyCode == kKeyLeft ? (self->cursor_ < self->select_ ? self->cursor_ : self->select_)
                                        : (self->cursor_ > self->select_ ? self->cursor_ : self->select_);
            self->Select(to, shift ? self->select_ : to);
        };
        bool handled = true;
        if (e.keyCode == 0 && e.character)
        {
            if (readOnly || command)
                return;
            u32 c = e.character == '\r' ? '\n' : e.character;
            if (c == '\n')
                return;
            self->Insert(Span<const u32>(&c, 1));
            self->Changed();
        }
        else if (e.keyCode == kKeyLeft)
            move(command ? self->WordStart(self->cursor_) : self->cursor_ - 1);
        else if (e.keyCode == kKeyRight)
            move(command ? self->WordEnd(self->cursor_) : self->cursor_ + 1);
        else if (e.keyCode == kKeyHome || e.keyCode == kKeyUp)
            move(0);
        else if (e.keyCode == kKeyEnd || e.keyCode == kKeyDown)
            move(n);
        else if (e.keyCode == kKeyBackspace || e.keyCode == kKeyDelete)
        {
            if (readOnly)
                return;
            if (self->cursor_ != self->select_)
                self->DeleteSelection();
            else if (e.keyCode == kKeyBackspace && self->cursor_ > 0)
                self->Erase(command ? self->WordStart(self->cursor_) : self->cursor_ - 1, self->cursor_);
            else if (e.keyCode == kKeyDelete && self->cursor_ < n)
                self->Erase(self->cursor_, command ? self->WordEnd(self->cursor_) : self->cursor_ + 1);
            self->Changed();
        }
        else if (e.keyCode == kKeyReturn || e.keyCode == kKeyKeypadEnter)
        {
            if (self->options_[AbstractBaseField::Multiline] && !readOnly && !command)
            {
                u32 c = '\n';
                self->Insert(Span<const u32>(&c, 1));
                self->Changed();
            }
            else
                self->field_.OnTextChanged(true);
        }
        else if (e.keyCode == kKeyEscape)
            self->field_.OnTextChanged(true);
        else if (command && e.keyCode == kKeyA)
            self->Select(n, 0);
        else if (command && (e.keyCode == kKeyC || e.keyCode == kKeyX))
        {
            if (self->cursor_ == self->select_ || self->options_[AbstractBaseField::Password])
                return;
            engine::GUIUtility::SetSystemCopyBuffer(self->Selected().View());
            if (e.keyCode == kKeyX && !readOnly)
            {
                self->DeleteSelection();
                self->Changed();
            }
        }
        else if (command && e.keyCode == kKeyV)
        {
            if (readOnly)
                return;
            Array<u32> pasted = Codepoints(engine::GUIUtility::systemCopyBuffer().View());
            self->Insert(Span<const u32>(pasted.Data(), pasted.Size()));
            self->Changed();
        }
        else if (e.keyCode == kKeyTab)
            handled = false;
        else
            handled = false;
        if (handled)
        {
            self->blink_ = 0;
            e.StopPropagation();
        }
    }

    void TextInput::Composition(void* user, EventSystem& events, UIEvent&)
    {
        auto* self = static_cast<TextInput*>(user);
        Span<const u32> c = events.Composition();
        self->composition_.Clear();
        for (u32 x : c)
            self->composition_.Push(x);
        self->Show();
    }

    void TextInput::FocusIn(void* user, EventSystem&, UIEvent&)
    {
        auto* self = static_cast<TextInput*>(user);
        self->focused_ = true;
        self->justFocused_ = true;
        self->blink_ = 0;
        self->dirty_ = true;
        SetPartClass(self->field_.handle(), "helix-text-field--focused", true);
        if (self->options_[AbstractBaseField::SelectAllOnFocus] && self->pointer_ < 0)
            self->Select(i32(self->codepoints_.Size()), 0);
        StartControlTick(self->field_, &TextInput::Tick);
    }

    void TextInput::FocusOut(void* user, EventSystem&, UIEvent&)
    {
        auto* self = static_cast<TextInput*>(user);
        self->focused_ = false;
        self->composition_.Clear();
        ShowPart(self->caret_, false);
        for (ElementHandle b : self->boxes_)
            ShowPart(b, false);
        SetPartClass(self->field_.handle(), "helix-text-field--focused", false);
        self->Show();
        self->field_.OnTextChanged(true);
    }

    bool TextInput::Tick(Element& self, f64 time)
    {
        auto& field = static_cast<AbstractBaseField&>(self);
        TextInput& t = field.Text();
        if (!t.focused_)
            return false;
        if (t.blink_ <= 0)
            t.blink_ = time;
        // On for half a second, off for half (on again after each edit).
        bool on = std::fmod(time - t.blink_, 1.0) < 0.5 && !t.options_[AbstractBaseField::ReadOnly];
        ShowPart(t.caret_, on);
        t.PlaceCaret();
        t.dirty_ = false;
        return true;
    }
}
