#include <Helix/UI/Engine/Implementation/GuiState.h>

#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Input/SystemClipboard.h>

#include <cmath>

// TextEditor: the text of a field with its cursor and selection (indices in UTF-16 units, as the reference's), moved
// and edited by keys; the text controls: TextField, PasswordField and TextArea over it.
namespace helix::ui::gui
{
    using namespace detail;

    namespace
    {
        Array<char16_t> Units(StringView text)
        {
            Array<char16_t> out;
            for (usize i = 0; i < text.Size();)
            {
                u8 c = u8(text[i]);
                char32_t cp = c;
                usize n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
                if (n > 1)
                {
                    cp = c & (n == 2 ? 0x1F : n == 3 ? 0x0F : 0x07);
                    for (usize k = 1; k < n && i + k < text.Size(); k++)
                        cp = (cp << 6) | (u8(text[i + k]) & 0x3F);
                }
                i += n;
                if (cp >= 0x10000)
                {
                    cp -= 0x10000;
                    out.Push(char16_t(0xD800 + (cp >> 10)));
                    out.Push(char16_t(0xDC00 + (cp & 0x3FF)));
                }
                else
                    out.Push(char16_t(cp));
            }
            return out;
        }

        String FromUnits(Span<const char16_t> units)
        {
            String out;
            for (usize i = 0; i < units.Size(); i++)
            {
                char32_t c = units[i];
                if (c >= 0xD800 && c < 0xDC00 && i + 1 < units.Size() && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000)
                {
                    c = 0x10000 + ((c - 0xD800) << 10) + (units[i + 1] - 0xDC00);
                    i++;
                }
                AppendUtf8(out, c);
            }
            return out;
        }

        bool WordUnit(char16_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c > 127; }
    }

    // ------------------------------------------------------------------------------------------------- state

    TextEditor::~TextEditor() { delete content_; }

    GuiContent* TextEditor::content()
    {
        if (!content_)
            content_ = new GuiContent();
        content_->SetText(text_.View());
        return content_;
    }

    void TextEditor::SetContent(GuiContent* v)
    {
        if (v)
            SetText(v->text().View());
    }

    i32 TextEditor::Clamp(i32 i) const
    {
        i32 n = Utf16Length(text_.View());
        return i < 0 ? 0 : i > n ? n : i;
    }

    void TextEditor::SetText(StringView v)
    {
        text_ = String(v);
        cursor_ = Clamp(cursor_);
        select_ = Clamp(select_);
    }

    void TextEditor::SetCursorIndex(i32 v) { cursor_ = Clamp(v); }
    void TextEditor::SetSelectIndex(i32 v) { select_ = Clamp(v); }

    String TextEditor::SelectedText() const
    {
        Array<char16_t> u = Units(text_.View());
        i32 a = cursor_ < select_ ? cursor_ : select_, b = cursor_ < select_ ? select_ : cursor_;
        return FromUnits(Span<const char16_t>(u.Data() + a, usize(b - a)));
    }

    String TextEditor::Shown() const
    {
        if (!password_)
            return text_;
        String out;
        i32 n = Utf16Length(text_.View());
        for (i32 i = 0; i < n; i++)
            AppendUtf8(out, mask_);
        return out;
    }

    i32 TextEditor::WordStart(i32 at) const
    {
        Array<char16_t> u = Units(text_.View());
        i32 i = Clamp(at);
        while (i > 0 && !WordUnit(u[usize(i - 1)]))
            i--;
        while (i > 0 && WordUnit(u[usize(i - 1)]))
            i--;
        return i;
    }

    i32 TextEditor::WordEnd(i32 at) const
    {
        Array<char16_t> u = Units(text_.View());
        i32 n = i32(u.Size()), i = Clamp(at);
        while (i < n && !WordUnit(u[usize(i)]))
            i++;
        while (i < n && WordUnit(u[usize(i)]))
            i++;
        return i;
    }

    i32 TextEditor::LineStart(i32 at) const
    {
        Array<char16_t> u = Units(text_.View());
        i32 i = Clamp(at);
        while (i > 0 && u[usize(i - 1)] != '\n')
            i--;
        return i;
    }

    i32 TextEditor::LineEnd(i32 at) const
    {
        Array<char16_t> u = Units(text_.View());
        i32 n = i32(u.Size()), i = Clamp(at);
        while (i < n && u[usize(i)] != '\n')
            i++;
        return i;
    }

    void TextEditor::OnFocus()
    {
        if (multiline_)
            cursor_ = select_ = 0;
        else
            SelectAll();
    }

    void TextEditor::OnLostFocus()
    {
        scroll_ = Vector2(0, 0);
        select_ = cursor_;
    }

    // ------------------------------------------------------------------------------------------------- editing

    void TextEditor::ReplaceSelection(StringView replace)
    {
        Array<char16_t> u = Units(text_.View());
        i32 a = cursor_ < select_ ? cursor_ : select_, b = cursor_ < select_ ? select_ : cursor_;
        Array<char16_t> r = Units(replace);
        Array<char16_t> out;
        for (i32 i = 0; i < a; i++)
            out.Push(u[usize(i)]);
        for (char16_t c : r)
            if (multiline_ || c != '\n')
                out.Push(c);
        i32 at = i32(out.Size());
        for (usize i = usize(b); i < u.Size(); i++)
            out.Push(u[i]);
        text_ = FromUnits(Span<const char16_t>(out.Data(), out.Size()));
        cursor_ = select_ = at;
    }

    void TextEditor::Insert(char16_t c)
    {
        char16_t one[1] = {c};
        ReplaceSelection(FromUnits(Span<const char16_t>(one, 1)).View());
    }

    bool TextEditor::DeleteSelection()
    {
        if (cursor_ == select_)
            return false;
        ReplaceSelection(StringView());
        return true;
    }

    bool TextEditor::Delete()
    {
        if (DeleteSelection())
            return true;
        if (cursor_ >= Utf16Length(text_.View()))
            return false;
        Array<char16_t> u = Units(text_.View());
        select_ = cursor_ + ((u[usize(cursor_)] >= 0xD800 && u[usize(cursor_)] < 0xDC00) ? 2 : 1);
        return DeleteSelection();
    }

    bool TextEditor::Backspace()
    {
        if (DeleteSelection())
            return true;
        if (cursor_ <= 0)
            return false;
        Array<char16_t> u = Units(text_.View());
        select_ = cursor_ - ((cursor_ >= 2 && u[usize(cursor_ - 1)] >= 0xDC00 && u[usize(cursor_ - 1)] < 0xE000) ? 2 : 1);
        return DeleteSelection();
    }

    bool TextEditor::DeleteLineBack()
    {
        if (DeleteSelection())
            return true;
        select_ = LineStart(cursor_);
        return DeleteSelection();
    }

    bool TextEditor::DeleteWordBack()
    {
        if (DeleteSelection())
            return true;
        select_ = WordStart(cursor_);
        return DeleteSelection();
    }

    bool TextEditor::DeleteWordForward()
    {
        if (DeleteSelection())
            return true;
        select_ = WordEnd(cursor_);
        return DeleteSelection();
    }

    bool TextEditor::CanPaste() { return !engine::SystemClipboard::Text().Empty(); }

    bool TextEditor::Cut()
    {
        if (password_ || cursor_ == select_)
            return false;
        Copy();
        return DeleteSelection();
    }

    void TextEditor::Copy()
    {
        if (password_ || cursor_ == select_)
            return;
        engine::SystemClipboard::SetText(SelectedText().View());
    }

    bool TextEditor::Paste()
    {
        String clip = engine::SystemClipboard::Text();
        if (clip.Empty())
            return false;
        // A single line keeps its line feeds out.
        String text;
        for (char c : clip.View())
            if (c != '\r' && (multiline_ || c != '\n'))
                text += StringView(&c, 1);
        ReplaceSelection(text.View());
        return true;
    }

    void TextEditor::SaveBackup()
    {
        backup_ = text_;
        backupCursor_ = cursor_;
        backupSelect_ = select_;
    }

    void TextEditor::Undo()
    {
        text_ = backup_;
        cursor_ = Clamp(backupCursor_);
        select_ = Clamp(backupSelect_);
    }

    // ------------------------------------------------------------------------------------------------- moves

    void TextEditor::SelectAll()
    {
        select_ = 0;
        cursor_ = Utf16Length(text_.View());
    }

    void TextEditor::SelectNone() { select_ = cursor_; }

    void TextEditor::MoveRight()
    {
        if (cursor_ != select_)
            cursor_ = select_ = cursor_ > select_ ? cursor_ : select_;
        else
            cursor_ = select_ = Clamp(cursor_ + 1);
    }

    void TextEditor::MoveLeft()
    {
        if (cursor_ != select_)
            cursor_ = select_ = cursor_ < select_ ? cursor_ : select_;
        else
            cursor_ = select_ = Clamp(cursor_ - 1);
    }

    namespace
    {
        // The index a line up or down from the cursor (by its pixel place).
        i32 Vertical(TextEditor& t, GuiStyle* style, String shown, i32 at, f32 lines)
        {
            if (!style)
                return at;
            GuiContent content(shown.View());
            Vector2 p = style->GetCursorPixelPosition(t.position(), &content, at);
            f32 h = style->lineHeight();
            return style->GetCursorStringIndex(t.position(), &content, Vector2(p.x, p.y + h * lines + h * 0.5f));
        }
    }

    void TextEditor::MoveUp()
    {
        i32 from = cursor_ < select_ ? cursor_ : select_;
        cursor_ = select_ = multiline_ ? Vertical(*this, style_, Shown(), from, -1) : 0;
    }

    void TextEditor::MoveDown()
    {
        i32 from = cursor_ > select_ ? cursor_ : select_;
        cursor_ = select_ = multiline_ ? Vertical(*this, style_, Shown(), from, 1) : Utf16Length(text_.View());
    }

    void TextEditor::MoveLineStart() { cursor_ = select_ = LineStart(cursor_ < select_ ? cursor_ : select_); }
    void TextEditor::MoveLineEnd() { cursor_ = select_ = LineEnd(cursor_ > select_ ? cursor_ : select_); }
    void TextEditor::MoveTextStart() { cursor_ = select_ = 0; }
    void TextEditor::MoveTextEnd() { cursor_ = select_ = Utf16Length(text_.View()); }
    void TextEditor::MoveWordRight() { cursor_ = select_ = WordEnd(cursor_ > select_ ? cursor_ : select_); }
    void TextEditor::MoveWordLeft() { cursor_ = select_ = WordStart(cursor_ < select_ ? cursor_ : select_); }
    void TextEditor::SelectLeft() { cursor_ = Clamp(cursor_ - 1); }
    void TextEditor::SelectRight() { cursor_ = Clamp(cursor_ + 1); }
    void TextEditor::SelectUp() { cursor_ = multiline_ ? Vertical(*this, style_, Shown(), cursor_, -1) : 0; }
    void TextEditor::SelectDown() { cursor_ = multiline_ ? Vertical(*this, style_, Shown(), cursor_, 1) : Utf16Length(text_.View()); }
    void TextEditor::SelectTextStart() { cursor_ = 0; }
    void TextEditor::SelectTextEnd() { cursor_ = Utf16Length(text_.View()); }
    void TextEditor::SelectToStartOfNextWord() { cursor_ = WordEnd(cursor_); }
    void TextEditor::SelectToEndOfPreviousWord() { cursor_ = WordStart(cursor_); }
    void TextEditor::SelectWordRight() { cursor_ = WordEnd(cursor_); }
    void TextEditor::SelectWordLeft() { cursor_ = WordStart(cursor_); }

    void TextEditor::SelectCurrentWord()
    {
        Array<char16_t> u = Units(text_.View());
        i32 n = i32(u.Size()), a = Clamp(cursor_), b = a;
        bool word = a < n ? WordUnit(u[usize(a)]) : (a > 0 && WordUnit(u[usize(a - 1)]));
        while (a > 0 && WordUnit(u[usize(a - 1)]) == word && u[usize(a - 1)] != '\n')
            a--;
        while (b < n && WordUnit(u[usize(b)]) == word && u[usize(b)] != '\n')
            b++;
        select_ = a;
        cursor_ = b;
    }

    void TextEditor::SelectCurrentParagraph()
    {
        select_ = LineStart(cursor_);
        cursor_ = LineEnd(cursor_);
    }

    void TextEditor::MoveCursorToPosition(Vector2 cursorPosition)
    {
        SelectToPosition(cursorPosition);
        select_ = cursor_;
    }

    void TextEditor::SelectToPosition(Vector2 cursorPosition)
    {
        if (!style_)
            return;
        String shown = Shown();
        GuiContent content(shown.View());
        i32 at = style_->GetCursorStringIndex(position_, &content, Vector2(cursorPosition.x + scroll_.x, cursorPosition.y + scroll_.y));
        if (snap_ == 1)
        {
            // Snapped to words: the selection grows by whole words.
            if (at < select_)
                at = WordStart(at);
            else
                at = WordEnd(at);
        }
        else if (snap_ == 2)
            at = at < select_ ? LineStart(at) : LineEnd(at);
        cursor_ = Clamp(at);
    }

    void TextEditor::DblClickSnap(DblClickSnapping snapping) { snapping_ = snapping; }

    void TextEditor::MouseDragSelectsWholeWords(bool on) { snap_ = on ? (snapping_ == DblClickSnapping::WORDS ? 1 : 2) : 0; }

    i32 TextEditor::IndexAt(Vector2 point) const
    {
        if (!style_)
            return cursor_;
        String shown = Shown();
        GuiContent content(shown.View());
        return style_->GetCursorStringIndex(position_, &content, Vector2(point.x + scroll_.x, point.y + scroll_.y));
    }

    bool TextEditor::GraphicalLine(i32 at, i32& start, i32& end) const
    {
        if (!style_)
        {
            start = LineStart(at);
            end = LineEnd(at);
            return false;
        }
        // The line the index is drawn on, from its far left to its far right.
        String shown = Shown();
        GuiContent content(shown.View());
        Vector2 p = style_->GetCursorPixelPosition(position_, &content, at);
        f32 y = p.y + style_->lineHeight() * 0.5f;
        start = style_->GetCursorStringIndex(position_, &content, Vector2(-1e6f, y));
        end = style_->GetCursorStringIndex(position_, &content, Vector2(1e6f, y));
        return true;
    }

    void TextEditor::MoveGraphicalLineStart()
    {
        i32 s, e;
        GraphicalLine(cursor_ < select_ ? cursor_ : select_, s, e);
        cursor_ = select_ = s;
    }

    void TextEditor::MoveGraphicalLineEnd()
    {
        i32 s, e;
        GraphicalLine(cursor_ > select_ ? cursor_ : select_, s, e);
        cursor_ = select_ = e;
    }

    void TextEditor::SelectGraphicalLineStart()
    {
        i32 s, e;
        GraphicalLine(cursor_, s, e);
        cursor_ = s;
    }

    void TextEditor::SelectGraphicalLineEnd()
    {
        i32 s, e;
        GraphicalLine(cursor_, s, e);
        cursor_ = e;
    }

    void TextEditor::ExpandSelectGraphicalLineStart()
    {
        i32 lo = cursor_ < select_ ? cursor_ : select_, hi = cursor_ < select_ ? select_ : cursor_, s, e;
        GraphicalLine(lo, s, e);
        select_ = hi;
        cursor_ = s;
    }

    void TextEditor::ExpandSelectGraphicalLineEnd()
    {
        i32 lo = cursor_ < select_ ? cursor_ : select_, hi = cursor_ < select_ ? select_ : cursor_, s, e;
        GraphicalLine(hi, s, e);
        select_ = lo;
        cursor_ = e;
    }

    void TextEditor::MoveParagraphForward()
    {
        i32 at = cursor_ > select_ ? cursor_ : select_;
        if (at < Utf16Length(text_.View()))
            at = LineEnd(at + 1);
        cursor_ = select_ = at;
    }

    void TextEditor::MoveParagraphBackward()
    {
        i32 at = cursor_ < select_ ? cursor_ : select_;
        cursor_ = select_ = at > 1 ? LineStart(at - 1) : 0;
    }

    void TextEditor::SelectParagraphForward()
    {
        if (cursor_ < Utf16Length(text_.View()))
            cursor_ = LineEnd(cursor_ + 1);
    }

    void TextEditor::SelectParagraphBackward() { cursor_ = cursor_ > 1 ? LineStart(cursor_ - 1) : 0; }

    i32 TextEditor::FindStartOfNextWord(i32 p)
    {
        Array<char16_t> u = Units(text_.View());
        i32 n = i32(u.Size()), i = Clamp(p);
        while (i < n && WordUnit(u[usize(i)]))
            i++;
        while (i < n && !WordUnit(u[usize(i)]))
            i++;
        return i;
    }

    void TextEditor::MoveToStartOfNextWord() { cursor_ = select_ = FindStartOfNextWord(cursor_ > select_ ? cursor_ : select_); }

    void TextEditor::MoveToEndOfPreviousWord()
    {
        Array<char16_t> u = Units(text_.View());
        i32 i = Clamp(cursor_ < select_ ? cursor_ : select_);
        while (i > 0 && WordUnit(u[usize(i - 1)]))
            i--;
        while (i > 0 && !WordUnit(u[usize(i - 1)]))
            i--;
        cursor_ = select_ = i;
    }

    void TextEditor::MoveAltCursorToPosition(Vector2 cursorPosition) { alt_ = Clamp(IndexAt(cursorPosition)); }

    void TextEditor::MoveSelectionToAltCursor()
    {
        i32 lo = cursor_ < select_ ? cursor_ : select_, hi = cursor_ < select_ ? select_ : cursor_;
        if (alt_ < 0 || lo == hi || (alt_ >= lo && alt_ <= hi))
            return;
        // The selected text taken out and put in at the other cursor, selected there.
        String moved = SelectedText();
        i32 length = hi - lo, at = alt_ > hi ? alt_ - length : alt_;
        DeleteSelection();
        cursor_ = select_ = Clamp(at);
        ReplaceSelection(moved.View());
        select_ = at;
        cursor_ = at + length;
        alt_ = -1;
    }

    bool TextEditor::IsOverSelection(Vector2 cursorPosition)
    {
        i32 lo = cursor_ < select_ ? cursor_ : select_, hi = cursor_ < select_ ? select_ : cursor_;
        if (lo == hi)
            return false;
        i32 at = IndexAt(cursorPosition);
        return at >= lo && at < hi;
    }

    bool TextEditor::LinkAt(Vector2 point, bool href, String& out) const
    {
        out.Clear();
        if (!style_)
            return false;
        GuiContent content(text_.View());
        return style_->LinkAtPoint(position_, content, Vector2(point.x + scroll_.x, point.y + scroll_.y), href, out);
    }

    bool TextEditor::HasClickedOnLink(Vector2 mousePosition, String& linkData) { return LinkAt(mousePosition, false, linkData); }

    bool TextEditor::HasClickedOnHREF(Vector2 mousePosition, String& href) { return LinkAt(mousePosition, true, href); }

    void TextEditor::DetectFocusChange()
    {
        bool now = controlID_ != 0 && GuiUtility::keyboardControl() == controlID_;
        if (now == hasFocus_)
            return;
        hasFocus_ = now;
        if (now)
            OnFocus();
        else
            OnLostFocus();
    }

    void TextEditor::DrawCursor(StringView newText)
    {
        String saved = text_;
        text_ = String(newText);
        if (style_)
        {
            String shown = Shown();
            GuiContent content(shown.View());
            graphicalCursor_ = style_->GetCursorPixelPosition(position_, &content, Clamp(cursor_));
        }
        text_ = saved;
    }

    void TextEditor::UpdateScrollOffsetIfNeeded(Event* evt)
    {
        (void)evt;
        if (!style_)
            return;
        String shown = Shown();
        GuiContent content(shown.View());
        Vector2 cursor = style_->GetCursorPixelPosition(position_, &content, cursor_);
        graphicalCursor_ = cursor;
        graphicalSelect_ = style_->GetCursorPixelPosition(position_, &content, select_);
        Rect inner = style_->padding()->Remove(position_);
        // The cursor kept inside the content rect (a little room on the right for it).
        f32 x = cursor.x - scroll_.x;
        if (!multiline_ || !style_->wordWrap())
        {
            if (x > inner.x + inner.width - 1)
                scroll_.x = cursor.x - (inner.x + inner.width - 1);
            else if (x < inner.x)
                scroll_.x = cursor.x - inner.x;
            Vector2 size = style_->CalcSize(&content);
            f32 maxScroll = size.x - position_.width;
            if (scroll_.x > maxScroll + 1)
                scroll_.x = maxScroll + 1 > 0 ? maxScroll + 1 : 0;
            if (scroll_.x < 0)
                scroll_.x = 0;
        }
        if (multiline_)
        {
            f32 h = style_->lineHeight();
            f32 y = cursor.y - scroll_.y;
            if (y + h > inner.y + inner.height)
                scroll_.y = cursor.y + h - (inner.y + inner.height);
            else if (y < inner.y)
                scroll_.y = cursor.y - inner.y;
            if (scroll_.y < 0)
                scroll_.y = 0;
        }
    }

    bool TextEditor::HandleKeyEvent(Event* e)
    {
        if (!e)
            return false;
        platform::KeyCode key = e->keyCode();
        bool shift = e->shift(), word = e->control() || e->alt(), command = e->control() || e->command();
        using K = platform::KeyCode;
        switch (key)
        {
        case K::LeftArrow:
            shift ? (word ? SelectWordLeft() : SelectLeft()) : (word ? MoveWordLeft() : MoveLeft());
            return true;
        case K::RightArrow:
            shift ? (word ? SelectWordRight() : SelectRight()) : (word ? MoveWordRight() : MoveRight());
            return true;
        case K::UpArrow:
            shift ? SelectUp() : MoveUp();
            return true;
        case K::DownArrow:
            shift ? SelectDown() : MoveDown();
            return true;
        case K::Home:
            if (shift)
                cursor_ = command ? 0 : LineStart(cursor_);
            else
                command ? MoveTextStart() : MoveLineStart();
            return true;
        case K::End:
            if (shift)
                cursor_ = command ? Utf16Length(text_.View()) : LineEnd(cursor_);
            else
                command ? MoveTextEnd() : MoveLineEnd();
            return true;
        case K::Backspace:
            word ? DeleteWordBack() : Backspace();
            return true;
        case K::Delete:
            word ? DeleteWordForward() : Delete();
            return true;
        default:
            break;
        }
        if (command && e->character() == 0)
        {
            switch (key)
            {
            case K::A:
                SelectAll();
                return true;
            case K::C:
                Copy();
                return true;
            case K::X:
                Cut();
                return true;
            case K::V:
                Paste();
                return true;
            case K::Z:
                Undo();
                return true;
            default:
                break;
            }
        }
        return false;
    }

    // ------------------------------------------------------------------------------------------------- the text controls

    struct TextControl
    {
        static void Configure(TextEditor& t, const Rect& position, const GuiStyle& style, bool multiline, char16_t mask)
        {
            t.position_ = position;
            t.style_ = const_cast<GuiStyle*>(&style);
            t.multiline_ = multiline;
            t.password_ = mask != 0;
            t.mask_ = mask ? mask : u'*';
        }

    };

    String Gui::TextContent(Rect position, StringView text, i32 maxLength, const GuiStyle& style, bool multiline, char16_t mask)
    {
        State& s = S();
        i32 id = ControlId(Hint(multiline ? "TextArea" : mask ? "PasswordField" : "TextField"), FocusType::Keyboard, position);
        Event& e = Current();
        String result(text);
        // The editor of the focused field: one that gets the focus by the keys (tab, FocusControl) selects its text.
        auto editor = [&]() -> TextEditor* {
            bool made = !s.editor || s.editor->controlID() != id;
            TextEditor* t = EditorFor(id);
            TextControl::Configure(*t, position, style, multiline, mask);
            if (t->text() != text)
                t->SetText(text);
            if (made && e.type() != EventType::MouseDown)
                t->OnFocus();
            return t;
        };
        auto limit = [&](String value) {
            if (maxLength >= 0 && Utf16Length(value.View()) > maxLength)
                return String(value.View().Substr(0, ByteOfUnit(value.View(), maxLength)));
            return value;
        };
        switch (e.GetTypeForControl(id))
        {
        case EventType::MouseDown:
            if (s.enabled && Contains(position, Mouse()))
            {
                GuiUtility::SetKeyboardControl(id);
                TextEditor* t = editor();
                s.hotControl = id;
                GuiSettings* settings = Skin().settings();
                if (e.clickCount() == 2 && settings->doubleClickSelectsWord())
                {
                    t->MoveCursorToPosition(Mouse());
                    t->SelectCurrentWord();
                    t->DblClickSnap(TextEditor::DblClickSnapping::WORDS);
                    t->MouseDragSelectsWholeWords(true);
                }
                else if (e.clickCount() >= 3 && settings->tripleClickSelectsLine())
                {
                    t->MoveCursorToPosition(Mouse());
                    t->SelectCurrentParagraph();
                    t->DblClickSnap(TextEditor::DblClickSnapping::PARAGRAPHS);
                    t->MouseDragSelectsWholeWords(true);
                }
                else
                {
                    t->MouseDragSelectsWholeWords(false);
                    if (e.shift())
                        t->SelectToPosition(Mouse());
                    else
                        t->MoveCursorToPosition(Mouse());
                }
                e.Use();
            }
            break;
        case EventType::MouseDrag:
            if (s.hotControl == id)
            {
                editor()->SelectToPosition(Mouse());
                e.Use();
            }
            break;
        case EventType::MouseUp:
            if (s.hotControl == id)
            {
                editor()->MouseDragSelectsWholeWords(false);
                s.hotControl = 0;
                e.Use();
            }
            break;
        case EventType::KeyDown:
        {
            if (s.keyboardControl != id || !s.enabled)
                break;
            TextEditor* t = editor();
            String before = t->text();
            bool used = false;
            char16_t c = e.character();
            if (c == 0 && t->HandleKeyEvent(&e))
                used = true;
            else if (c == '\t' || (c == 0 && e.keyCode() == platform::KeyCode::Tab))
                break;
            else if (c == '\n' || c == '\r' || (c == 0 && (e.keyCode() == platform::KeyCode::Return || e.keyCode() == platform::KeyCode::KeypadEnter)))
            {
                // The key makes a line feed in an area (a line feed character is the same key); a field leaves it to
                // the script.
                if (multiline)
                {
                    if (c == 0)
                        t->Insert(u'\n');
                    used = true;
                }
            }
            else if (c >= 32)
            {
                if (maxLength < 0 || Utf16Length(t->text().View()) - (t->hasSelection() ? Utf16Length(t->SelectedText().View()) : 0) < maxLength)
                    t->Insert(c);
                used = true;
            }
            if (used)
            {
                e.Use();
                result = limit(t->text());
                if (result != before.View())
                {
                    t->SetText(result.View());
                    s.changed = true;
                }
            }
            break;
        }
        case EventType::Repaint:
        {
            String shown(text);
            if (mask)
            {
                shown.Clear();
                i32 n = Utf16Length(text);
                for (i32 i = 0; i < n; i++)
                    AppendUtf8(shown, mask);
            }
            GuiContent& content = GuiContent::Temp(shown.View());
            if (s.keyboardControl == id && s.enabled)
            {
                TextEditor* t = editor();
                t->UpdateScrollOffsetIfNeeded(&e);
                style.DrawWithTextSelection(position, &content, id, t->selectIndex(), t->cursorIndex());
                // The input method's caret where the cursor is.
                Vector2 p = t->graphicalCursorPos();
                Vector2 screen = GuiUtility::GuiToScreenPoint(Vector2(p.x - t->scrollOffset().x, p.y - t->scrollOffset().y));
                engine::InputFeed::RequestTextInput(i32(screen.x), i32(screen.y), i32(style.lineHeight()));
            }
            else
                style.Draw(position, &content, id, false);
            break;
        }
        default:
            break;
        }
        return result;
    }

    String Gui::TextField(Rect position, StringView text) { return TextContent(position, text, -1, *Skin().textField(), false, 0); }
    String Gui::TextField(Rect position, StringView text, i32 maxLength) { return TextContent(position, text, maxLength, *Skin().textField(), false, 0); }
    String Gui::TextField(Rect position, StringView text, GuiStyle* style)
    {
        return TextContent(position, text, -1, style ? *style : *Skin().textField(), false, 0);
    }
    String Gui::TextField(Rect position, StringView text, i32 maxLength, GuiStyle* style)
    {
        return TextContent(position, text, maxLength, style ? *style : *Skin().textField(), false, 0);
    }
    String Gui::PasswordField(Rect position, StringView password, char16_t maskChar)
    {
        return TextContent(position, password, -1, *Skin().textField(), false, maskChar ? maskChar : u'*');
    }
    String Gui::PasswordField(Rect position, StringView password, char16_t maskChar, i32 maxLength)
    {
        return TextContent(position, password, maxLength, *Skin().textField(), false, maskChar ? maskChar : u'*');
    }
    String Gui::PasswordField(Rect position, StringView password, char16_t maskChar, GuiStyle* style)
    {
        return TextContent(position, password, -1, style ? *style : *Skin().textField(), false, maskChar ? maskChar : u'*');
    }
    String Gui::PasswordField(Rect position, StringView password, char16_t maskChar, i32 maxLength, GuiStyle* style)
    {
        return TextContent(position, password, maxLength, style ? *style : *Skin().textField(), false, maskChar ? maskChar : u'*');
    }
    String Gui::TextArea(Rect position, StringView text) { return TextContent(position, text, -1, *Skin().textArea(), true, 0); }
    String Gui::TextArea(Rect position, StringView text, i32 maxLength) { return TextContent(position, text, maxLength, *Skin().textArea(), true, 0); }
    String Gui::TextArea(Rect position, StringView text, GuiStyle* style)
    {
        return TextContent(position, text, -1, style ? *style : *Skin().textArea(), true, 0);
    }
    String Gui::TextArea(Rect position, StringView text, i32 maxLength, GuiStyle* style)
    {
        return TextContent(position, text, maxLength, style ? *style : *Skin().textArea(), true, 0);
    }

    namespace detail
    {
        TextEditor* EditorFor(i32 id)
        {
            State& s = S();
            if (s.editor && s.editor->controlID() == id)
                return s.editor;
            if (s.editor)
                s.editor->OnLostFocus();
            delete s.editor;
            s.editor = new TextEditor();
            s.editor->SetControlID(id);
            return s.editor;
        }

        void ClearEditor()
        {
            State& s = S();
            if (s.editor)
                s.editor->OnLostFocus();
            delete s.editor;
            s.editor = nullptr;
        }
    }
}
