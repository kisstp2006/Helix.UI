#pragma once

#include <Helix/UI/Engine/Fields.h>

// The text input of the text fields (not for scripts): the text as codepoints, the cursor and the selection's other
// end (codepoint indices), the options; its parts in the field's input: the text, the selection's boxes, the caret.
// Pointers place the cursor (a double click selects a word, a triple one the line), keys edit and move it, the
// clipboard is GUIUtility.systemCopyBuffer, the input method composes at the caret.
namespace helix::ui
{
    class TextInput
    {
    public:
        explicit TextInput(AbstractBaseField& field);

        void Build(ElementHandle input);
        String Text() const;
        // The text replaced (the cursor at its end, clamped to the max length).
        void SetText(StringView text);
        i32 Option(i32 option) const;
        void SetOption(i32 option, i32 value);
        void Select(i32 cursor, i32 select);
        i32 cursor() const { return cursor_; }
        bool focused() const { return focused_; }
        i32 selectIndex() const { return select_; }
        ElementHandle textElement() const { return text_; }

    private:
        static void PointerDown(void* user, EventSystem& events, UIEvent& e);
        static void PointerMove(void* user, EventSystem& events, UIEvent& e);
        static void PointerUp(void* user, EventSystem& events, UIEvent& e);
        static void KeyDown(void* user, EventSystem& events, UIEvent& e);
        static void Composition(void* user, EventSystem& events, UIEvent& e);
        static void FocusIn(void* user, EventSystem& events, UIEvent& e);
        static void FocusOut(void* user, EventSystem& events, UIEvent& e);
        static bool Tick(Element& self, f64 time);

        bool Accepts(u32 c) const;
        void Insert(Span<const u32> text);
        void Erase(i32 from, i32 to);
        void DeleteSelection();
        void Changed();
        void Show();
        void PlaceCaret();
        i32 IndexAt(Vector2 local) const;
        i32 WordStart(i32 i) const;
        i32 WordEnd(i32 i) const;
        String Selected() const;
        Vector2 Origin() const;

        AbstractBaseField& field_;
        ElementHandle input_, text_, caret_;
        Array<ElementHandle> boxes_;
        Array<u32> codepoints_;
        // The shown text's byte where each codepoint starts (a codepoint per shown character).
        Array<u32> starts_;
        Array<u32> composition_;
        i32 cursor_ = 0, select_ = 0;
        i32 options_[13] = {0, 0, '*', -1, 0, 1, 1, 1, 1, 0, 0, 0, 0};
        i32 pointer_ = -1;
        bool focused_ = false, dirty_ = false, justFocused_ = false;
        f64 blink_ = 0;
        u32 clicks_ = 0;
    };

    // The menu a popup field opens: an element over the panel's root with the choices, the picked one checked;
    // a press outside closes it.
    class PopupMenu
    {
    public:
        explicit PopupMenu(AbstractBaseField& field);
        ~PopupMenu();

        // Several may be checked (the mask fields): the checked items' indices.
        bool multiple = false;
        Array<u32> checked;
        // The field's shown text (in its input).
        ElementHandle text;
        void Open();
        void Close();
        bool open() const;

    private:
        static void ItemUp(void* user, EventSystem& events, UIEvent& e);
        static void OutsideDown(void* user, EventSystem& events, UIEvent& e);
        AbstractBaseField& field_;
        ElementHandle menu_;
        Array<ElementHandle> items_;
    };

    // The shown text of an element (a text element's text with no rich text tags).
    void SetPlainText(ElementHandle e, bool plain);
}
