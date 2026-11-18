#include <Helix/UI/Engine/Implementation/GuiState.h>
#include <Helix/UI/Engine/Implementation/Overlay.h>
#include <Helix/UI/Engine/ImageAtlas.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Input/SystemClipboard.h>
#include <Helix/Math/Quaternion.h>

#include <cmath>
#include <memory>

// The immediate GUI's pass state and Gui's controls. A control asks for its id, reads the pass's event for its id
// (GetTypeForControl: a held control takes the mouse) and draws in Repaint; the clips and groups move the origin of
// the coordinates and cut what is drawn.
namespace helix::ui::gui
{
    namespace detail
    {
        State& S()
        {
            static State* s = new State();
            return *s;
        }

        i32 Hint(StringView name)
        {
            u32 h = 2166136261u;
            for (char c : name)
                h = (h ^ u8(c)) * 16777619u;
            return i32(h & 0x7fffffff);
        }

        GuiSkin& Skin()
        {
            State& s = S();
            GuiSkin* k = s.skin.Get();
            return k ? *k : *GuiSkin::Default();
        }

        EventType Type() { return S().current ? S().current->type() : EventType::Ignore; }

        Event& Current()
        {
            // Outside a pass the controls see an ignored event.
            static Event* none = new Event();
            none->SetType(EventType::Ignore);
            return S().current ? *S().current : *none;
        }
        bool Repainting() { return Type() == EventType::Repaint; }
        bool Layouting() { return Type() == EventType::Layout; }

        const Clip& TopClip() { return S().clips.Back(); }

        Vector2 ToGuiSpace(Vector2 local)
        {
            Vector2 o = TopClip().origin;
            return Vector2(local.x + o.x, local.y + o.y);
        }

        bool Contains(const Rect& r, Vector2 p) { return p.x >= r.x && p.y >= r.y && p.x < r.x + r.width && p.y < r.y + r.height; }

        Vector2 Mouse() { return S().current ? S().current->mousePosition() : Vector2(-10000, -10000); }

        bool Hover(const Rect& r, i32 id)
        {
            State& s = S();
            if (!s.enabled || (s.hotControl != 0 && s.hotControl != id))
                return false;
            // A window in front of the GUI under it takes the hover.
            if (s.target != s.window)
                return false;
            return Contains(r, Mouse());
        }

        // The mesh's map and clip from the matrix and the top clip.
        void SyncMesh()
        {
            State& s = S();
            if (s.clips.Empty())
                return;
            const Clip& c = s.clips.Back();
            s.Mesh().SetTransform(s.toScreen * Affine::Translation(c.origin));
            UIClip clip;
            clip.rect = c.visible;
            clip.toClip = s.toScreen.Inverse();
            s.Mesh().SetClip(clip);
        }

        void UpdateMouse()
        {
            State& s = S();
            if (!s.current || s.clips.Empty())
                return;
            s.mouse = s.toScreen.Inverse().Apply(s.screenMouse);
            const Clip& c = s.clips.Back();
            // Outside the clip the pointer is far away (no control under it there), unless a control holds it.
            bool inside = Contains(c.visible, s.mouse);
            if (!inside && s.hotControl == 0)
                s.current->SetMousePosition(Vector2(-10000, -10000));
            else
                s.current->SetMousePosition(Vector2(s.mouse.x - c.origin.x, s.mouse.y - c.origin.y));
        }

        void PushClip(const Rect& local, Vector2 scroll)
        {
            State& s = S();
            const Clip& parent = s.clips.Back();
            Rect r(local.x + parent.origin.x, local.y + parent.origin.y, local.width, local.height);
            const Rect& v = parent.visible;
            f32 x0 = r.x > v.x ? r.x : v.x, y0 = r.y > v.y ? r.y : v.y;
            f32 x1 = r.x + r.width < v.x + v.width ? r.x + r.width : v.x + v.width;
            f32 y1 = r.y + r.height < v.y + v.height ? r.y + r.height : v.y + v.height;
            Clip c;
            c.visible = Rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
            c.origin = Vector2(r.x + scroll.x, r.y + scroll.y);
            s.clips.Push(c);
            SyncMesh();
            UpdateMouse();
        }

        void PopClip()
        {
            State& s = S();
            if (s.clips.Size() <= 1)
            {
                HX_LOG_ERROR(LogChannel{"UI"}, "GUI: EndGroup or EndClip without a Begin.");
                return;
            }
            s.clips.Pop();
            SyncMesh();
            UpdateMouse();
        }

        i32 ControlId(i32 hint, FocusType focus, const Rect& rect)
        {
            State& s = S();
            i32 id = 0;
            IdList* list = s.ids;
            if (!list)
                id = s.nextId++;
            else if (list->at < list->ids.Size() && list->hints[list->at] == hint)
                id = list->ids[list->at++];
            else
            {
                // A call that differs from the last pass's: the list goes on from here with new ids.
                list->hints.Resize(list->at);
                list->ids.Resize(list->at);
                id = s.nextId++;
                list->hints.Push(hint);
                list->ids.Push(id);
                list->at++;
            }
            if (focus == FocusType::Keyboard)
            {
                Named n;
                n.id = id;
                n.rect = rect;
                s.keyboardControls.Push(n);
                if (!s.nextControlName.Empty())
                {
                    n.name = s.nextControlName;
                    if (s.focusByName && s.focusName == s.nextControlName.View())
                    {
                        s.keyboardControl = id;
                        s.focusByName = false;
                    }
                    s.named.Push(Move(n));
                }
            }
            s.nextControlName.Clear();
            return id;
        }

        Color BackgroundTint()
        {
            State& s = S();
            Color c(s.color.r * s.backgroundColor.r, s.color.g * s.backgroundColor.g, s.color.b * s.backgroundColor.b, s.color.a * s.backgroundColor.a);
            if (!s.enabled)
                c.a *= 0.5f;
            return c;
        }

        Color ContentTint(Color text)
        {
            State& s = S();
            Color c(s.color.r * s.contentColor.r * text.r, s.color.g * s.contentColor.g * text.g, s.color.b * s.contentColor.b * text.b,
                    s.color.a * s.contentColor.a * text.a);
            if (!s.enabled)
                c.a *= 0.5f;
            return c;
        }

        void DrawImage(const Rect& rect, engine::Texture2D* texture, const RectOffset* border, Color tint)
        {
            if (!texture || tint.a <= 0 || rect.width <= 0 || rect.height <= 0)
                return;
            UIImage image;
            overlay::SharedAtlas().Place(texture, image);
            image.tint = tint;
            if (border)
            {
                image.slices[0] = f32(border->left());
                image.slices[1] = f32(border->top());
                image.slices[2] = f32(border->right());
                image.slices[3] = f32(border->bottom());
            }
            const f32 none[4] = {0, 0, 0, 0};
            S().Mesh().Image(rect, image, Vector4(0, 0, 0, 0), none, 1);
        }

        void DrawTextureRect(const Rect& rect, engine::Texture2D* texture, const Rect& uv, Color tint)
        {
            if (!texture || tint.a <= 0 || rect.width <= 0 || rect.height <= 0)
                return;
            // Texture coordinates are the reference's (v up): the texture's rows go up, so v is as it is.
            const Vector2 corners[4] = {Vector2(rect.x, rect.y), Vector2(rect.x + rect.width, rect.y), Vector2(rect.x + rect.width, rect.y + rect.height),
                                        Vector2(rect.x, rect.y + rect.height)};
            Rect flipped(uv.x, uv.y + uv.height, uv.width, -uv.height);
            S().Mesh().TexturedQuad(corners, flipped, Color(tint.r * tint.a, tint.g * tint.a, tint.b * tint.a, tint.a), texture, UIVertexKind::Image, 0, 1);
        }

        void DrawRectangle(const Rect& rect, Color c) { S().Mesh().Rectangle(rect, Color(c.r * c.a, c.g * c.a, c.b * c.a, c.a)); }

        // ----------------------------------------------------------------------------------------------- text units

        i32 Utf16Length(StringView text) { return UnitOfByte(text, u32(text.Size())); }

        namespace
        {
            // The length of the UTF-8 sequence at a byte and its UTF-16 units.
            void Step(StringView text, usize at, usize& bytes, i32& units)
            {
                u8 c = u8(text[at]);
                bytes = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
                if (at + bytes > text.Size())
                    bytes = text.Size() - at;
                units = bytes == 4 ? 2 : 1;
            }
        }

        u32 ByteOfUnit(StringView text, i32 unit)
        {
            usize at = 0;
            i32 u = 0;
            while (at < text.Size() && u < unit)
            {
                usize bytes;
                i32 units;
                Step(text, at, bytes, units);
                at += bytes;
                u += units;
            }
            return u32(at);
        }

        i32 UnitOfByte(StringView text, u32 byte)
        {
            usize at = 0;
            i32 u = 0;
            while (at < text.Size() && at < byte)
            {
                usize bytes;
                i32 units;
                Step(text, at, bytes, units);
                at += bytes;
                u += units;
            }
            return u;
        }

        void AppendUtf8(String& out, char32_t c)
        {
            char b[4];
            usize n = 0;
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

        // ----------------------------------------------------------------------------------------------- windows

        Window::~Window() { DeleteLayoutCache(cache); }
        Owner::~Owner() { DeleteLayoutCache(cache); }

        Window* FindWindow(u64 owner, i32 id)
        {
            for (Window* w : S().windows)
                if (w->owner == owner && w->id == id)
                    return w;
            return nullptr;
        }

        namespace
        {
            Window* Modal()
            {
                Window* m = nullptr;
                for (Window* w : S().windows)
                    if (w->modal && w->seen && (!m || w->order > m->order))
                        m = w;
                return m;
            }

            Window* WindowOf(i32 controlId)
            {
                for (Window* w : S().windows)
                    for (i32 id : w->ids.ids)
                        if (id == controlId)
                            return w;
                return nullptr;
            }
        }

        void ChooseTarget(Vector2 screenMouse)
        {
            State& s = S();
            s.target = nullptr;
            // A modal window takes everything; a held control's window keeps the mouse; else the front-most window
            // under the pointer.
            if (Window* m = Modal())
            {
                s.target = m;
                return;
            }
            if (s.hotControl != 0)
            {
                s.target = WindowOf(s.hotControl);
                return;
            }
            Window* best = nullptr;
            for (Window* w : s.windows)
            {
                if (!w->seen)
                    continue;
                Affine toScreen;
                toScreen.a = w->matrix.m00, toScreen.b = w->matrix.m10, toScreen.c = w->matrix.m01, toScreen.d = w->matrix.m11;
                toScreen.tx = w->matrix.m03, toScreen.ty = w->matrix.m13;
                Vector2 p = toScreen.Inverse().Apply(screenMouse);
                if (Contains(w->rect, p) && (!best || w->order > best->order))
                    best = w;
            }
            s.target = best;
        }

        void DoWindows(Owner& owner)
        {
            State& s = S();
            Array<Window*> list;
            for (Window* w : s.windows)
                if (w->owner == owner.key && w->seen)
                    list.Push(w);
            // Back to front (input front to back: only the target window takes the mouse, so the order is the same).
            for (usize i = 1; i < list.Size(); i++)
                for (usize k = i; k > 0 && list[k - 1]->order > list[k]->order; k--)
                    std::swap(list[k - 1], list[k]);
            EventType real = s.raw.type();
            bool mouse = s.raw.isMouse() || s.raw.isScrollWheel();
            Window* saved = s.window;
            IdList* savedIds = s.ids;
            for (Window* w : list)
            {
                // A click in a window brings it to the front.
                if (real == EventType::MouseDown && s.target == w)
                {
                    w->order = ++s.windowOrder;
                }
                s.event.CopyFrom(s.raw);
                if (mouse && s.target != w)
                    s.event.SetType(EventType::Ignore);
                s.current = &s.event;
                s.skin = w->skin;
                s.color = w->color, s.backgroundColor = w->backgroundColor, s.contentColor = w->contentColor;
                s.enabled = w->enabled;
                Gui::SetMatrix(w->matrix);
                s.window = w;
                s.ids = &w->ids;
                w->ids.at = 0;
                u32 clips = u32(s.clips.Size());
                // The frame, then the content in the window's space.
                if (s.event.type() == EventType::Repaint)
                {
                    bool focused = true;
                    for (Window* o : s.windows)
                        if (o->seen && o->order > w->order)
                            focused = false;
                    w->style.DrawState(w->rect, w->title, false, false, focused, false);
                }
                PushClip(w->rect, Vector2(0, 0));
                if (w->layout)
                    BeginLayout(w->cache, Rect(0, 0, w->rect.width, w->rect.height), &w->style);
                Gui::windowFunctionCalled().Invoke(w->id);
                if (w->layout)
                {
                    EndLayout(w->cache);
                    if (s.event.type() == EventType::Layout)
                    {
                        // A layout window grows to its content.
                        Vector2 min = LayoutCacheMinSize(w->cache);
                        if (min.x > w->rect.width)
                            w->rect.width = min.x;
                        if (min.y > w->rect.height)
                            w->rect.height = min.y;
                    }
                }
                while (s.clips.Size() > clips)
                    PopClip();
            }
            s.window = saved;
            s.ids = savedIds;
        }
    }

    using namespace detail;

    // ------------------------------------------------------------------------------------------------- state

    GuiSkin* Gui::skin() { return &Skin(); }
    void Gui::SetSkin(GuiSkin* value) { S().skin = engine::PPtr<GuiSkin>(value); }
    Color Gui::color() { return S().color; }
    void Gui::SetColor(Color value) { S().color = value; }
    Color Gui::backgroundColor() { return S().backgroundColor; }
    void Gui::SetBackgroundColor(Color value) { S().backgroundColor = value; }
    Color Gui::contentColor() { return S().contentColor; }
    void Gui::SetContentColor(Color value) { S().contentColor = value; }
    bool Gui::changed() { return S().changed; }
    void Gui::SetChanged(bool value) { S().changed = value; }
    bool Gui::enabled() { return S().enabled; }
    void Gui::SetEnabled(bool value) { S().enabled = value; }
    i32 Gui::depth() { return S().depth; }
    void Gui::SetDepth(i32 value) { S().depth = value; }
    Matrix4x4 Gui::matrix() { return S().matrix; }

    void Gui::SetMatrix(Matrix4x4 value)
    {
        State& s = S();
        s.matrix = value;
        Affine a;
        a.a = value.m00, a.b = value.m10, a.c = value.m01, a.d = value.m11, a.tx = value.m03, a.ty = value.m13;
        s.toScreen = a;
        SyncMesh();
        UpdateMouse();
    }

    String Gui::tooltip() { return S().tooltip; }
    void Gui::SetTooltip(StringView value) { S().tooltip = String(value); }

    namespace
    {
        // The tooltip of the control under the pointer (after it, in the pass).
        void Tip(const Rect& r, const GuiContent& c, i32 id)
        {
            if (!c.tooltip().Empty() && Hover(r, id))
                S().tooltip = c.tooltip();
        }

        bool Input() { return S().enabled; }

        GuiStyle& Pick(GuiStyle* style, GuiStyle* fallback) { return style ? *style : *fallback; }
    }

    // ------------------------------------------------------------------------------------------------- labels and boxes

    void Gui::LabelContent(Rect position, const GuiContent& content, const GuiStyle& style)
    {
        Tip(position, content, 0);
        if (Repainting())
            style.DrawState(position, content, Hover(position, 0), false, false, false);
    }

    void Gui::BoxContent(Rect position, const GuiContent& content, const GuiStyle& style)
    {
        i32 id = ControlId(Hint("Box"), FocusType::Passive, position);
        Tip(position, content, id);
        if (Repainting())
            style.DrawState(position, content, Hover(position, id), false, false, false);
    }

    void Gui::Label(Rect position, StringView text) { LabelContent(position, GuiContent::Temp(text), *Skin().label()); }
    void Gui::Label(Rect position, engine::Texture2D* image) { LabelContent(position, GuiContent::Temp(StringView(), image), *Skin().label()); }
    void Gui::Label(Rect position, GuiContent* content) { LabelContent(position, content ? *content : *GuiContent::none(), *Skin().label()); }
    void Gui::Label(Rect position, StringView text, GuiStyle* style) { LabelContent(position, GuiContent::Temp(text), Pick(style, Skin().label())); }
    void Gui::Label(Rect position, engine::Texture2D* image, GuiStyle* style)
    {
        LabelContent(position, GuiContent::Temp(StringView(), image), Pick(style, Skin().label()));
    }
    void Gui::Label(Rect position, GuiContent* content, GuiStyle* style)
    {
        LabelContent(position, content ? *content : *GuiContent::none(), Pick(style, Skin().label()));
    }
    void Gui::Box(Rect position, StringView text) { BoxContent(position, GuiContent::Temp(text), *Skin().box()); }
    void Gui::Box(Rect position, engine::Texture2D* image) { BoxContent(position, GuiContent::Temp(StringView(), image), *Skin().box()); }
    void Gui::Box(Rect position, GuiContent* content) { BoxContent(position, content ? *content : *GuiContent::none(), *Skin().box()); }
    void Gui::Box(Rect position, StringView text, GuiStyle* style) { BoxContent(position, GuiContent::Temp(text), Pick(style, Skin().box())); }
    void Gui::Box(Rect position, engine::Texture2D* image, GuiStyle* style)
    {
        BoxContent(position, GuiContent::Temp(StringView(), image), Pick(style, Skin().box()));
    }
    void Gui::Box(Rect position, GuiContent* content, GuiStyle* style)
    {
        BoxContent(position, content ? *content : *GuiContent::none(), Pick(style, Skin().box()));
    }

    // ------------------------------------------------------------------------------------------------- buttons

    bool Gui::ButtonContent(Rect position, const GuiContent& content, const GuiStyle& style, bool repeat)
    {
        State& s = S();
        i32 id = ControlId(Hint(repeat ? "RepeatButton" : "Button"), FocusType::Passive, position);
        Tip(position, content, id);
        Event& e = Current();
        switch (e.GetTypeForControl(id))
        {
        case EventType::MouseDown:
            if (Input() && Contains(position, Mouse()))
            {
                s.hotControl = id;
                e.Use();
            }
            return false;
        case EventType::MouseDrag:
            if (s.hotControl == id)
                e.Use();
            return false;
        case EventType::MouseUp:
            if (s.hotControl == id)
            {
                s.hotControl = 0;
                e.Use();
                if (Contains(position, Mouse()))
                {
                    s.changed = true;
                    return !repeat;
                }
            }
            return false;
        case EventType::KeyDown:
            if (!repeat && s.keyboardControl == id && (e.keyCode() == platform::KeyCode::Space || e.keyCode() == platform::KeyCode::Return))
            {
                e.Use();
                s.changed = true;
                return true;
            }
            return false;
        case EventType::Repaint:
            style.DrawState(position, content, Hover(position, id), s.hotControl == id, false, s.keyboardControl == id);
            // A repeat button is pressed while it is held over.
            return repeat && s.hotControl == id && Contains(position, Mouse());
        default:
            return false;
        }
    }

    bool Gui::Button(Rect position, StringView text) { return ButtonContent(position, GuiContent::Temp(text), *Skin().button(), false); }
    bool Gui::Button(Rect position, engine::Texture2D* image) { return ButtonContent(position, GuiContent::Temp(StringView(), image), *Skin().button(), false); }
    bool Gui::Button(Rect position, GuiContent* content) { return ButtonContent(position, content ? *content : *GuiContent::none(), *Skin().button(), false); }
    bool Gui::Button(Rect position, StringView text, GuiStyle* style) { return ButtonContent(position, GuiContent::Temp(text), Pick(style, Skin().button()), false); }
    bool Gui::Button(Rect position, engine::Texture2D* image, GuiStyle* style)
    {
        return ButtonContent(position, GuiContent::Temp(StringView(), image), Pick(style, Skin().button()), false);
    }
    bool Gui::Button(Rect position, GuiContent* content, GuiStyle* style)
    {
        return ButtonContent(position, content ? *content : *GuiContent::none(), Pick(style, Skin().button()), false);
    }
    bool Gui::RepeatButton(Rect position, StringView text) { return ButtonContent(position, GuiContent::Temp(text), *Skin().button(), true); }
    bool Gui::RepeatButton(Rect position, engine::Texture2D* image)
    {
        return ButtonContent(position, GuiContent::Temp(StringView(), image), *Skin().button(), true);
    }
    bool Gui::RepeatButton(Rect position, GuiContent* content)
    {
        return ButtonContent(position, content ? *content : *GuiContent::none(), *Skin().button(), true);
    }
    bool Gui::RepeatButton(Rect position, StringView text, GuiStyle* style)
    {
        return ButtonContent(position, GuiContent::Temp(text), Pick(style, Skin().button()), true);
    }
    bool Gui::RepeatButton(Rect position, engine::Texture2D* image, GuiStyle* style)
    {
        return ButtonContent(position, GuiContent::Temp(StringView(), image), Pick(style, Skin().button()), true);
    }
    bool Gui::RepeatButton(Rect position, GuiContent* content, GuiStyle* style)
    {
        return ButtonContent(position, content ? *content : *GuiContent::none(), Pick(style, Skin().button()), true);
    }

    // ------------------------------------------------------------------------------------------------- toggles

    bool Gui::ToggleContent(Rect position, bool value, const GuiContent& content, const GuiStyle& style)
    {
        State& s = S();
        i32 id = ControlId(Hint("Toggle"), FocusType::Passive, position);
        Tip(position, content, id);
        Event& e = Current();
        switch (e.GetTypeForControl(id))
        {
        case EventType::MouseDown:
            if (Input() && Contains(position, Mouse()))
            {
                s.hotControl = id;
                e.Use();
            }
            break;
        case EventType::MouseDrag:
            if (s.hotControl == id)
                e.Use();
            break;
        case EventType::MouseUp:
            if (s.hotControl == id)
            {
                s.hotControl = 0;
                e.Use();
                if (Contains(position, Mouse()))
                {
                    s.changed = true;
                    return !value;
                }
            }
            break;
        case EventType::KeyDown:
            if (s.keyboardControl == id && e.keyCode() == platform::KeyCode::Space)
            {
                e.Use();
                s.changed = true;
                return !value;
            }
            break;
        case EventType::Repaint:
            style.DrawState(position, content, Hover(position, id), s.hotControl == id, value, s.keyboardControl == id);
            break;
        default:
            break;
        }
        return value;
    }

    bool Gui::Toggle(Rect position, bool value, StringView text) { return ToggleContent(position, value, GuiContent::Temp(text), *Skin().toggle()); }
    bool Gui::Toggle(Rect position, bool value, engine::Texture2D* image)
    {
        return ToggleContent(position, value, GuiContent::Temp(StringView(), image), *Skin().toggle());
    }
    bool Gui::Toggle(Rect position, bool value, GuiContent* content)
    {
        return ToggleContent(position, value, content ? *content : *GuiContent::none(), *Skin().toggle());
    }
    bool Gui::Toggle(Rect position, bool value, StringView text, GuiStyle* style)
    {
        return ToggleContent(position, value, GuiContent::Temp(text), Pick(style, Skin().toggle()));
    }
    bool Gui::Toggle(Rect position, bool value, engine::Texture2D* image, GuiStyle* style)
    {
        return ToggleContent(position, value, GuiContent::Temp(StringView(), image), Pick(style, Skin().toggle()));
    }
    bool Gui::Toggle(Rect position, bool value, GuiContent* content, GuiStyle* style)
    {
        return ToggleContent(position, value, content ? *content : *GuiContent::none(), Pick(style, Skin().toggle()));
    }

    // ------------------------------------------------------------------------------------------------- grids

    namespace
    {
        // The cells of a grid: xCount columns, the rows it takes, the style's margins between them (none on a toolbar).
        // A toolbar fitting its contents: each cell as wide as its content's share of the whole.
        struct Fit
        {
            Array<f32> x, w;
        };

        Rect CellRect(const Rect& r, i32 index, i32 count, i32 xCount, f32 spacing, const Fit* fit = nullptr)
        {
            if (fit && usize(index) < fit->x.Size())
                return Rect(std::floor(r.x + fit->x[usize(index)]), r.y, std::floor(fit->w[usize(index)]), r.height);
            i32 columns = xCount > 0 ? xCount : 1;
            i32 rows = (count + columns - 1) / columns;
            rows = rows > 0 ? rows : 1;
            f32 w = (r.width - spacing * f32(columns - 1)) / f32(columns), h = (r.height - spacing * f32(rows - 1)) / f32(rows);
            i32 cx = index % columns, cy = index / columns;
            return Rect(std::floor(r.x + f32(cx) * (w + spacing)), std::floor(r.y + f32(cy) * (h + spacing)), std::floor(w), std::floor(h));
        }

        i32 CellAt(const Rect& r, Vector2 p, i32 count, i32 xCount, f32 spacing, const Fit* fit = nullptr)
        {
            for (i32 i = 0; i < count; i++)
                if (Contains(CellRect(r, i, count, xCount, spacing, fit), p))
                    return i;
            return -1;
        }

        template <class F>
        i32 WithContents(i32 count, F&& f)
        {
            Array<GuiContent*> contents;
            std::unique_ptr<GuiContent[]> made(new GuiContent[usize(count > 0 ? count : 1)]);
            for (i32 i = 0; i < count; i++)
                contents.Push(&made[usize(i)]);
            return f(made.get(), Span<GuiContent* const>(contents.Data(), contents.Size()));
        }
    }

    i32 Gui::GridContent(Rect position, i32 selected, Span<GuiContent* const> contents, i32 xCount, const GuiStyle* style, bool toolbar,
                         ToolbarButtonSize buttonSize, Span<const bool> enabled)
    {
        State& s = S();
        const GuiStyle& st = style ? *style : *Skin().button();
        i32 count = i32(contents.Size());
        i32 id = ControlId(Hint(toolbar ? "Toolbar" : "SelectionGrid"), FocusType::Passive, position);
        f32 spacing = toolbar ? 0.0f : f32(st.margin()->left() > st.margin()->right() ? st.margin()->left() : st.margin()->right());
        i32 columns = toolbar ? count : xCount;
        if (count == 0)
            return selected;
        Fit fitted;
        const Fit* fit = nullptr;
        if (toolbar && buttonSize == ToolbarButtonSize::FitToContents)
        {
            f32 total = 0;
            for (GuiContent* c : contents)
            {
                fitted.w.Push(st.CalcSize(c).x);
                total += fitted.w.Back();
            }
            f32 k = total > 0 ? position.width / total : 0, at = 0;
            for (f32& w : fitted.w)
            {
                fitted.x.Push(at);
                w *= k;
                at += w;
            }
            fit = &fitted;
        }
        auto on = [&](i32 i) { return usize(i) >= enabled.Size() || enabled[usize(i)]; };
        Event& e = Current();
        switch (e.GetTypeForControl(id))
        {
        case EventType::MouseDown:
            if (Input() && Contains(position, Mouse()) && CellAt(position, Mouse(), count, columns, spacing, fit) >= 0 &&
                on(CellAt(position, Mouse(), count, columns, spacing, fit)))
            {
                s.hotControl = id;
                e.Use();
            }
            break;
        case EventType::MouseDrag:
            if (s.hotControl == id)
                e.Use();
            break;
        case EventType::MouseUp:
            if (s.hotControl == id)
            {
                s.hotControl = 0;
                e.Use();
                i32 cell = CellAt(position, Mouse(), count, columns, spacing, fit);
                if (cell >= 0 && on(cell))
                {
                    s.changed = true;
                    return cell;
                }
            }
            break;
        case EventType::Repaint:
        {
            i32 over = CellAt(position, Mouse(), count, columns, spacing, fit);
            bool was = s.enabled;
            for (i32 i = 0; i < count; i++)
            {
                Rect cell = CellRect(position, i, count, columns, spacing, fit);
                const GuiContent& c = contents[usize(i)] ? *contents[usize(i)] : *GuiContent::none();
                s.enabled = was && on(i);
                bool hover = over == i && Hover(position, id);
                st.DrawState(cell, c, hover, s.hotControl == id && over == i, i == selected, false);
            }
            s.enabled = was;
            break;
        }
        default:
            break;
        }
        for (i32 i = 0; i < count; i++)
            if (contents[usize(i)])
                Tip(CellRect(position, i, count, columns, spacing, fit), *contents[usize(i)], id);
        return selected;
    }

    i32 Gui::Toolbar(Rect position, i32 selected, Span<const String> texts) { return Toolbar(position, selected, texts, Skin().button()); }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<engine::Texture2D* const> images)
    {
        return WithContents(i32(images.Size()), [&](GuiContent* made, Span<GuiContent* const> c) {
            for (usize i = 0; i < images.Size(); i++)
                made[i].SetImage(images[i]);
            return GridContent(position, selected, c, 0, Skin().button(), true);
        });
    }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<GuiContent* const> contents) { return GridContent(position, selected, contents, 0, Skin().button(), true); }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<const String> texts, GuiStyle* style)
    {
        return WithContents(i32(texts.Size()), [&](GuiContent* made, Span<GuiContent* const> c) {
            for (usize i = 0; i < texts.Size(); i++)
                made[i].SetText(texts[i].View());
            return GridContent(position, selected, c, 0, style, true);
        });
    }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<GuiContent* const> contents, GuiStyle* style)
    {
        return GridContent(position, selected, contents, 0, style, true);
    }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<engine::Texture2D* const> images, GuiStyle* style)
    {
        return Toolbar(position, selected, images, style, ToolbarButtonSize::Fixed);
    }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<const String> texts, GuiStyle* style, ToolbarButtonSize buttonSize)
    {
        return WithContents(i32(texts.Size()), [&](GuiContent* made, Span<GuiContent* const> c) {
            for (usize i = 0; i < texts.Size(); i++)
                made[i].SetText(texts[i].View());
            return GridContent(position, selected, c, 0, style, true, buttonSize);
        });
    }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<engine::Texture2D* const> images, GuiStyle* style, ToolbarButtonSize buttonSize)
    {
        return WithContents(i32(images.Size()), [&](GuiContent* made, Span<GuiContent* const> c) {
            for (usize i = 0; i < images.Size(); i++)
                made[i].SetImage(images[i]);
            return GridContent(position, selected, c, 0, style, true, buttonSize);
        });
    }
    i32 Gui::Toolbar(Rect position, i32 selected, Span<GuiContent* const> contents, GuiStyle* style, ToolbarButtonSize buttonSize)
    {
        return GridContent(position, selected, contents, 0, style, true, buttonSize);
    }
    i32 Gui::SelectionGrid(Rect position, i32 selected, Span<const String> texts, i32 xCount)
    {
        return SelectionGrid(position, selected, texts, xCount, Skin().button());
    }
    i32 Gui::SelectionGrid(Rect position, i32 selected, Span<engine::Texture2D* const> images, i32 xCount)
    {
        return WithContents(i32(images.Size()), [&](GuiContent* made, Span<GuiContent* const> c) {
            for (usize i = 0; i < images.Size(); i++)
                made[i].SetImage(images[i]);
            return GridContent(position, selected, c, xCount, Skin().button(), false);
        });
    }
    i32 Gui::SelectionGrid(Rect position, i32 selected, Span<GuiContent* const> contents, i32 xCount)
    {
        return GridContent(position, selected, contents, xCount, Skin().button(), false);
    }
    i32 Gui::SelectionGrid(Rect position, i32 selected, Span<const String> texts, i32 xCount, GuiStyle* style)
    {
        return WithContents(i32(texts.Size()), [&](GuiContent* made, Span<GuiContent* const> c) {
            for (usize i = 0; i < texts.Size(); i++)
                made[i].SetText(texts[i].View());
            return GridContent(position, selected, c, xCount, style, false);
        });
    }
    i32 Gui::SelectionGrid(Rect position, i32 selected, Span<GuiContent* const> contents, i32 xCount, GuiStyle* style)
    {
        return GridContent(position, selected, contents, xCount, style, false);
    }

    // ------------------------------------------------------------------------------------------------- sliders and scrollbars

    namespace
    {
        struct SliderDrag
        {
            i32 id = 0;
            f32 value = 0, mouse = 0;
        };

        SliderDrag& Drag()
        {
            static SliderDrag d;
            return d;
        }

        f32 Clamp(f32 v, f32 a, f32 b)
        {
            f32 lo = a < b ? a : b, hi = a < b ? b : a;
            return v < lo ? lo : v > hi ? hi : v;
        }

        struct SliderGeometry
        {
            Rect track, thumb;
            f32 length = 0, thumbSize = 0;
        };

        SliderGeometry Geometry(const Rect& position, f32 value, f32 size, f32 start, f32 end, const GuiStyle& slider, const GuiStyle& thumb, bool horiz)
        {
            SliderGeometry g;
            g.track = slider.padding()->Remove(position);
            g.length = horiz ? g.track.width : g.track.height;
            f32 own = horiz ? (thumb.fixedWidth() != 0 ? thumb.fixedWidth() : f32(thumb.padding()->horizontal()))
                            : (thumb.fixedHeight() != 0 ? thumb.fixedHeight() : f32(thumb.padding()->vertical()));
            f32 range = std::fabs(end - start);
            f32 bySize = range > 0 && size > 0 ? g.length * size / range : 0;
            g.thumbSize = Clamp(own > bySize ? own : bySize, 0, g.length);
            f32 span = (end - start) - (end >= start ? size : -size);
            f32 t = span != 0 ? (Clamp(value, start, end - (end >= start ? size : -size)) - start) / span : 0;
            f32 along = (horiz ? g.track.x : g.track.y) + t * (g.length - g.thumbSize);
            if (horiz)
            {
                f32 h = thumb.fixedHeight() != 0 ? thumb.fixedHeight() : g.track.height;
                g.thumb = Rect(along, position.y + (position.height - h) * 0.5f, g.thumbSize, h);
            }
            else
            {
                f32 w = thumb.fixedWidth() != 0 ? thumb.fixedWidth() : g.track.width;
                g.thumb = Rect(position.x + (position.width - w) * 0.5f, along, w, g.thumbSize);
            }
            return g;
        }

        f32 ValueAt(const SliderGeometry& g, f32 along, f32 size, f32 start, f32 end, bool horiz)
        {
            f32 travel = g.length - g.thumbSize;
            f32 span = (end - start) - (end >= start ? size : -size);
            f32 t = travel > 0 ? (along - (horiz ? g.track.x : g.track.y) - g.thumbSize * 0.5f) / travel : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            return start + t * span;
        }
    }

    f32 Gui::Slider(Rect position, f32 value, f32 size, f32 start, f32 end, GuiStyle* slider, GuiStyle* thumb, bool horiz, i32 id)
    {
        State& s = S();
        GuiStyle& sl = Pick(slider, horiz ? Skin().horizontalSlider() : Skin().verticalSlider());
        GuiStyle& th = Pick(thumb, horiz ? Skin().horizontalSliderThumb() : Skin().verticalSliderThumb());
        if (id == 0)
            id = ControlId(Hint("Slider"), FocusType::Passive, position);
        SliderGeometry g = Geometry(position, value, size, start, end, sl, th, horiz);
        f32 maxStart = end - (end >= start ? size : -size);
        Event& e = Current();
        Vector2 m = Mouse();
        f32 along = horiz ? m.x : m.y;
        switch (e.GetTypeForControl(id))
        {
        case EventType::MouseDown:
            if (!Input() || !Contains(position, m))
                break;
            s.hotControl = id;
            e.Use();
            if (!Contains(g.thumb, m))
            {
                if (size == 0)
                {
                    // A slider jumps to the pointer.
                    f32 v = ValueAt(g, along, size, start, end, horiz);
                    if (v != value)
                        s.changed = true;
                    value = v;
                }
                else
                {
                    // A scrollbar pages toward it.
                    f32 thumbAt = horiz ? g.thumb.x : g.thumb.y;
                    f32 step = (end >= start ? size : -size) * (along < thumbAt ? -1.0f : 1.0f);
                    value = Clamp(value + step, start, maxStart);
                    s.changed = true;
                    s.hotControl = 0;
                    break;
                }
            }
            Drag().id = id;
            Drag().value = value;
            Drag().mouse = along;
            break;
        case EventType::MouseDrag:
            if (s.hotControl == id && Drag().id == id)
            {
                f32 travel = g.length - g.thumbSize;
                f32 span = maxStart - start;
                f32 v = travel > 0 ? Drag().value + (along - Drag().mouse) / travel * span : Drag().value;
                v = Clamp(v, start, maxStart);
                if (v != value)
                    s.changed = true;
                value = v;
                e.Use();
            }
            break;
        case EventType::MouseUp:
            if (s.hotControl == id)
            {
                s.hotControl = 0;
                Drag().id = 0;
                e.Use();
            }
            break;
        case EventType::Repaint:
        {
            sl.DrawState(position, *GuiContent::none(), Hover(position, id), s.hotControl == id, false, false);
            SliderGeometry now = Geometry(position, value, size, start, end, sl, th, horiz);
            th.DrawState(now.thumb, *GuiContent::none(), Hover(now.thumb, id) || s.hotControl == id, s.hotControl == id, false, false);
            break;
        }
        default:
            break;
        }
        return value;
    }

    f32 Gui::HorizontalSlider(Rect position, f32 value, f32 leftValue, f32 rightValue)
    {
        return Slider(position, value, 0, leftValue, rightValue, nullptr, nullptr, true, 0);
    }
    f32 Gui::HorizontalSlider(Rect position, f32 value, f32 leftValue, f32 rightValue, GuiStyle* slider, GuiStyle* thumb)
    {
        return Slider(position, value, 0, leftValue, rightValue, slider, thumb, true, 0);
    }
    f32 Gui::VerticalSlider(Rect position, f32 value, f32 topValue, f32 bottomValue)
    {
        return Slider(position, value, 0, topValue, bottomValue, nullptr, nullptr, false, 0);
    }
    f32 Gui::VerticalSlider(Rect position, f32 value, f32 topValue, f32 bottomValue, GuiStyle* slider, GuiStyle* thumb)
    {
        return Slider(position, value, 0, topValue, bottomValue, slider, thumb, false, 0);
    }

    namespace
    {
        // A scrollbar: its buttons (the skin's styles named after it, none when they have no size) and the slider.
        f32 Scrollbar(Rect position, f32 value, f32 size, f32 start, f32 end, GuiStyle* style, bool horiz)
        {
            GuiSkin& k = Skin();
            GuiStyle& bar = Pick(style, horiz ? k.horizontalScrollbar() : k.verticalScrollbar());
            String name = bar.name();
            auto named = [&](StringView suffix) {
                String n = name;
                n += suffix;
                return k.FindStyle(n.View());
            };
            GuiStyle* thumb = named("thumb");
            GuiStyle* first = named(horiz ? "leftbutton" : "upbutton");
            GuiStyle* second = named(horiz ? "rightbutton" : "downbutton");
            if (!thumb)
                thumb = horiz ? k.horizontalScrollbarThumb() : k.verticalScrollbarThumb();
            f32 a = first ? (horiz ? first->fixedWidth() : first->fixedHeight()) : 0;
            f32 b = second ? (horiz ? second->fixedWidth() : second->fixedHeight()) : 0;
            Rect track = position;
            f32 step = size * 0.1f > 0 ? size * 0.1f : std::fabs(end - start) * 0.1f;
            f32 maxStart = end - (end >= start ? size : -size);
            if (a > 0)
            {
                Rect r = horiz ? Rect(position.x, position.y, a, position.height) : Rect(position.x, position.y, position.width, a);
                if (Gui::ButtonContent(r, *GuiContent::none(), *first, true))
                    value = Clamp(value - (end >= start ? step : -step), start, maxStart);
                if (horiz)
                    track.x += a, track.width -= a;
                else
                    track.y += a, track.height -= a;
            }
            if (b > 0)
            {
                Rect r = horiz ? Rect(position.x + position.width - b, position.y, b, position.height)
                               : Rect(position.x, position.y + position.height - b, position.width, b);
                if (Gui::ButtonContent(r, *GuiContent::none(), *second, true))
                    value = Clamp(value + (end >= start ? step : -step), start, maxStart);
                if (horiz)
                    track.width -= b;
                else
                    track.height -= b;
            }
            return Gui::Slider(track, value, size, start, end, &bar, thumb, horiz, 0);
        }
    }

    f32 Gui::HorizontalScrollbar(Rect position, f32 value, f32 size, f32 leftValue, f32 rightValue)
    {
        return Scrollbar(position, value, size, leftValue, rightValue, nullptr, true);
    }
    f32 Gui::HorizontalScrollbar(Rect position, f32 value, f32 size, f32 leftValue, f32 rightValue, GuiStyle* style)
    {
        return Scrollbar(position, value, size, leftValue, rightValue, style, true);
    }
    f32 Gui::VerticalScrollbar(Rect position, f32 value, f32 size, f32 topValue, f32 bottomValue)
    {
        return Scrollbar(position, value, size, topValue, bottomValue, nullptr, false);
    }
    f32 Gui::VerticalScrollbar(Rect position, f32 value, f32 size, f32 topValue, f32 bottomValue, GuiStyle* style)
    {
        return Scrollbar(position, value, size, topValue, bottomValue, style, false);
    }

    // ------------------------------------------------------------------------------------------------- textures

    void Gui::DrawTexture(Rect position, engine::Texture2D* image) { DrawTexture(position, image, ScaleMode::StretchToFill, true, 0); }
    void Gui::DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode) { DrawTexture(position, image, scaleMode, true, 0); }
    void Gui::DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode, bool alphaBlend) { DrawTexture(position, image, scaleMode, alphaBlend, 0); }

    void Gui::DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode, bool alphaBlend, f32 imageAspect)
    {
        DrawTexture(position, image, scaleMode, alphaBlend, imageAspect, S().color, 0, 0);
    }

    void Gui::DrawTexture(Rect position, engine::Texture2D* image, ScaleMode scaleMode, bool alphaBlend, f32 imageAspect, Color color, f32 borderWidth,
                          f32 borderRadius)
    {
        if (!Repainting() || !image)
            return;
        (void)alphaBlend;
        f32 aspect = imageAspect != 0 ? imageAspect : f32(image->width()) / f32(image->height() > 0 ? image->height() : 1);
        Rect r = position;
        Rect uv(0, 0, 1, 1);
        f32 boxAspect = position.height > 0 ? position.width / position.height : 1;
        if (scaleMode == ScaleMode::ScaleToFit)
        {
            if (boxAspect > aspect)
            {
                f32 w = position.height * aspect;
                r = Rect(position.x + (position.width - w) * 0.5f, position.y, w, position.height);
            }
            else
            {
                f32 h = position.width / aspect;
                r = Rect(position.x, position.y + (position.height - h) * 0.5f, position.width, h);
            }
        }
        else if (scaleMode == ScaleMode::ScaleAndCrop)
        {
            if (boxAspect > aspect)
            {
                f32 k = aspect / boxAspect;
                uv = Rect(0, (1 - k) * 0.5f, 1, k);
            }
            else
            {
                f32 k = boxAspect / aspect;
                uv = Rect((1 - k) * 0.5f, 0, k, 1);
            }
        }
        Color tint = color;
        if (!S().enabled)
            tint.a *= 0.5f;
        if (borderWidth > 0 || borderRadius > 0)
        {
            // A rounded or bordered texture: an image in a rounded box.
            UIImage img;
            img.texture = ExternalTextureHandle(image);
            img.rowsUp = true;
            img.textureWidth = f32(image->width());
            img.textureHeight = f32(image->height());
            img.tint = tint;
            img.uv = uv;
            const f32 borders[4] = {borderWidth, borderWidth, borderWidth, borderWidth};
            S().Mesh().Image(r, img, Vector4(borderRadius, borderRadius, borderRadius, borderRadius), borders, 1);
            return;
        }
        DrawTextureRect(r, image, uv, tint);
    }

    void Gui::DrawTextureWithTexCoords(Rect position, engine::Texture2D* image, Rect texCoords) { DrawTextureWithTexCoords(position, image, texCoords, true); }

    void Gui::DrawTextureWithTexCoords(Rect position, engine::Texture2D* image, Rect texCoords, bool alphaBlend)
    {
        (void)alphaBlend;
        if (!Repainting() || !image)
            return;
        Color tint = S().color;
        if (!S().enabled)
            tint.a *= 0.5f;
        DrawTextureRect(position, image, texCoords, tint);
    }

    // ------------------------------------------------------------------------------------------------- groups and clips

    void Gui::BeginGroup(Rect position) { BeginGroup(position, GuiContent::none(), GuiStyle::none()); }
    void Gui::BeginGroup(Rect position, StringView text) { BeginGroup(position, &GuiContent::Temp(text), GuiStyle::none()); }
    void Gui::BeginGroup(Rect position, GuiStyle* style) { BeginGroup(position, GuiContent::none(), style); }

    void Gui::BeginGroup(Rect position, GuiContent* content, GuiStyle* style)
    {
        i32 id = ControlId(Hint("BeginGroup"), FocusType::Passive, position);
        const GuiStyle& st = style ? *style : *GuiStyle::none();
        const GuiContent& c = content ? *content : *GuiContent::none();
        if (Repainting() && (&st != GuiStyle::none() || !c.text().Empty() || c.image()))
            st.DrawState(position, c, Hover(position, id), false, false, false);
        PushClip(position, Vector2(0, 0));
    }

    void Gui::EndGroup() { PopClip(); }
    void Gui::BeginClip(Rect position) { PushClip(position, Vector2(0, 0)); }

    void Gui::BeginClip(Rect position, Vector2 scrollOffset, Vector2 renderOffset, bool resetOffset)
    {
        (void)resetOffset;
        PushClip(position, Vector2(scrollOffset.x + renderOffset.x, scrollOffset.y + renderOffset.y));
    }

    void Gui::EndClip() { PopClip(); }

    // ------------------------------------------------------------------------------------------------- scroll views

    Vector2 Gui::BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect)
    {
        return BeginScrollView(position, scrollPosition, viewRect, false, false, nullptr, nullptr);
    }
    Vector2 Gui::BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect, bool alwaysShowHorizontal, bool alwaysShowVertical)
    {
        return BeginScrollView(position, scrollPosition, viewRect, alwaysShowHorizontal, alwaysShowVertical, nullptr, nullptr);
    }
    Vector2 Gui::BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect, GuiStyle* horizontalScrollbar, GuiStyle* verticalScrollbar)
    {
        return BeginScrollView(position, scrollPosition, viewRect, false, false, horizontalScrollbar, verticalScrollbar);
    }

    Vector2 Gui::BeginScrollView(Rect position, Vector2 scrollPosition, Rect viewRect, bool alwaysShowHorizontal, bool alwaysShowVertical,
                                 GuiStyle* horizontalScrollbar, GuiStyle* verticalScrollbar)
    {
        State& s = S();
        GuiSkin& k = Skin();
        GuiStyle& hs = Pick(horizontalScrollbar, k.horizontalScrollbar());
        GuiStyle& vs = Pick(verticalScrollbar, k.verticalScrollbar());
        i32 id = ControlId(Hint("ScrollView"), FocusType::Passive, position);
        ScrollState& state = s.scrollStates[id];
        if (state.apply)
        {
            scrollPosition = state.scroll;
            state.apply = false;
        }
        f32 hh = hs.fixedHeight() + f32(hs.margin()->top()), vw = vs.fixedWidth() + f32(vs.margin()->left());
        bool needH = alwaysShowHorizontal || viewRect.width > position.width;
        bool needV = alwaysShowVertical || viewRect.height > position.height;
        // A bar takes room from the other axis, which may then need its bar too.
        if (needH && !needV && viewRect.height > position.height - hh)
            needV = true;
        if (needV && !needH && viewRect.width > position.width - vw)
            needH = true;
        Rect clip(position.x, position.y, position.width - (needV ? vw : 0), position.height - (needH ? hh : 0));
        if (Repainting())
            k.scrollView()->DrawState(position, *GuiContent::none(), false, false, false, false);
        f32 maxX = viewRect.width - clip.width > 0 ? viewRect.width - clip.width : 0, maxY = viewRect.height - clip.height > 0 ? viewRect.height - clip.height : 0;
        // Both bars every pass (the same control ids whether they show or not); one not shown has no size.
        f32 hx = Gui::HorizontalScrollbar(needH ? Rect(position.x, position.y + position.height - hs.fixedHeight(), clip.width, hs.fixedHeight()) : Rect(0, 0, 0, 0),
                                          scrollPosition.x, clip.width < viewRect.width ? clip.width : viewRect.width, 0, viewRect.width, &hs);
        f32 vy = Gui::VerticalScrollbar(needV ? Rect(position.x + position.width - vs.fixedWidth(), position.y, vs.fixedWidth(), clip.height) : Rect(0, 0, 0, 0),
                                        scrollPosition.y, clip.height < viewRect.height ? clip.height : viewRect.height, 0, viewRect.height, &vs);
        scrollPosition.x = needH ? hx : 0;
        scrollPosition.y = needV ? vy : 0;
        scrollPosition.x = Clamp(scrollPosition.x, 0, maxX);
        scrollPosition.y = Clamp(scrollPosition.y, 0, maxY);
        state.scroll = scrollPosition;
        state.position = position;
        state.visible = clip;
        state.view = viewRect;
        state.horizontal = needH;
        state.vertical = needV;
        OpenScroll open;
        open.id = id;
        open.position = position;
        open.clip = clip;
        open.view = viewRect;
        s.scrolls.Push(open);
        PushClip(clip, Vector2(std::round(-scrollPosition.x - viewRect.x), std::round(-scrollPosition.y - viewRect.y)));
        return scrollPosition;
    }

    void Gui::EndScrollView() { EndScrollView(true); }

    void Gui::EndScrollView(bool handleScrollWheel)
    {
        State& s = S();
        if (s.scrolls.Empty())
        {
            HX_LOG_ERROR(LogChannel{"UI"}, "GUI: EndScrollView without a BeginScrollView.");
            return;
        }
        OpenScroll open = s.scrolls.Back();
        s.scrolls.Pop();
        PopClip();
        Event& e = Current();
        if (handleScrollWheel && e.type() == EventType::ScrollWheel && Contains(open.position, Mouse()))
        {
            ScrollState& state = s.scrollStates[open.id];
            Vector2 d = e.delta();
            f32 maxX = open.view.width - open.clip.width > 0 ? open.view.width - open.clip.width : 0;
            f32 maxY = open.view.height - open.clip.height > 0 ? open.view.height - open.clip.height : 0;
            Vector2 next(Clamp(state.scroll.x + d.x * 20, 0, maxX), Clamp(state.scroll.y + d.y * 20, 0, maxY));
            if (next.x != state.scroll.x || next.y != state.scroll.y)
            {
                state.scroll = next;
                state.apply = true;
                e.Use();
            }
        }
    }

    void Gui::ScrollTo(Rect position)
    {
        State& s = S();
        // The innermost scroll view moves so the rect (in its content) is seen.
        if (s.scrolls.Empty())
            return;
        const OpenScroll& open = s.scrolls.Back();
        ScrollState& state = s.scrollStates[open.id];
        Vector2 scroll = state.scroll;
        Rect r(position.x - open.view.x, position.y - open.view.y, position.width, position.height);
        if (r.x < scroll.x)
            scroll.x = r.x;
        else if (r.x + r.width > scroll.x + open.clip.width)
            scroll.x = r.x + r.width - open.clip.width;
        if (r.y < scroll.y)
            scroll.y = r.y;
        else if (r.y + r.height > scroll.y + open.clip.height)
            scroll.y = r.y + r.height - open.clip.height;
        if (scroll.x != state.scroll.x || scroll.y != state.scroll.y)
        {
            state.scroll = scroll;
            state.apply = true;
        }
    }

    bool Gui::ScrollTowards(Rect position, f32 maxDelta)
    {
        State& s = S();
        if (s.scrolls.Empty())
            return false;
        const OpenScroll& open = s.scrolls.Back();
        ScrollState& state = s.scrollStates[open.id];
        Vector2 target = state.scroll;
        Rect r(position.x - open.view.x, position.y - open.view.y, position.width, position.height);
        if (r.x < target.x)
            target.x = r.x;
        else if (r.x + r.width > target.x + open.clip.width)
            target.x = r.x + r.width - open.clip.width;
        if (r.y < target.y)
            target.y = r.y;
        else if (r.y + r.height > target.y + open.clip.height)
            target.y = r.y + r.height - open.clip.height;
        Vector2 d(target.x - state.scroll.x, target.y - state.scroll.y);
        f32 length = std::sqrt(d.x * d.x + d.y * d.y);
        if (length <= 0)
            return false;
        // At most maxDelta along the way.
        if (maxDelta >= 0 && length > maxDelta)
            d = Vector2(d.x * maxDelta / length, d.y * maxDelta / length);
        state.scroll = Vector2(state.scroll.x + d.x, state.scroll.y + d.y);
        state.apply = true;
        return true;
    }

    // ------------------------------------------------------------------------------------------------- windows

    Rect Gui::WindowInternal(i32 id, Rect clientRect, GuiContent* title, GuiStyle* style, bool modal)
    {
        State& s = S();
        u64 owner = s.owner ? s.owner->key : 0;
        Window* w = FindWindow(owner, id);
        if (!w)
        {
            w = new Window();
            w->owner = owner;
            w->id = id;
            w->order = ++s.windowOrder;
            s.windows.Push(w);
        }
        // The script's rect, unless it is the one returned last (then the window's own, which a drag moved).
        if (!w->hasReturned || clientRect.x != w->returned.x || clientRect.y != w->returned.y || clientRect.width != w->returned.width ||
            clientRect.height != w->returned.height)
            w->rect = clientRect;
        w->modal = modal;
        w->seen = true;
        w->called = true;
        w->layout = false;
        if (title)
        {
            w->title.SetText(title->text().View());
            w->title.SetImage(title->image());
            w->title.SetTooltip(title->tooltip().View());
        }
        w->style.CopyFrom(style ? *style : *Skin().window());
        w->skin = s.skin;
        w->color = s.color, w->backgroundColor = s.backgroundColor, w->contentColor = s.contentColor;
        w->enabled = s.enabled;
        w->matrix = s.matrix;
        w->returned = w->rect;
        w->hasReturned = true;
        return w->rect;
    }

    engine::Event<i32>& Gui::windowFunctionCalled()
    {
        static engine::Event<i32>* e = new engine::Event<i32>();
        return *e;
    }

    void Gui::DragWindow() { DragWindow(Rect(0, 0, 10000, 10000)); }

    void Gui::DragWindow(Rect position)
    {
        State& s = S();
        Window* w = s.window;
        if (!w)
        {
            HX_LOG_ERROR(LogChannel{"UI"}, "GUI: DragWindow outside a window function.");
            return;
        }
        i32 id = ControlId(Hint("DragWindow"), FocusType::Passive, position);
        Event& e = Current();
        Vector2 m = Mouse();
        switch (e.GetTypeForControl(id))
        {
        case EventType::MouseDown:
            if (Contains(position, m))
            {
                s.hotControl = id;
                w->dragMouse = s.mouse;
                w->dragFrom = Vector2(w->rect.x, w->rect.y);
                e.Use();
            }
            break;
        case EventType::MouseDrag:
            if (s.hotControl == id)
            {
                w->rect.x = w->dragFrom.x + (s.mouse.x - w->dragMouse.x);
                w->rect.y = w->dragFrom.y + (s.mouse.y - w->dragMouse.y);
                e.Use();
            }
            break;
        case EventType::MouseUp:
            if (s.hotControl == id)
            {
                s.hotControl = 0;
                e.Use();
            }
            break;
        default:
            break;
        }
    }

    void Gui::BringWindowToFront(i32 windowID)
    {
        State& s = S();
        if (Window* w = FindWindow(s.owner ? s.owner->key : 0, windowID))
            w->order = ++s.windowOrder;
    }

    void Gui::BringWindowToBack(i32 windowID)
    {
        State& s = S();
        Window* w = FindWindow(s.owner ? s.owner->key : 0, windowID);
        if (!w)
            return;
        u64 low = w->order;
        for (Window* o : s.windows)
            low = o->order < low ? o->order : low;
        // Behind all: the others move up one.
        for (Window* o : s.windows)
            o->order += 1;
        w->order = low;
    }

    void Gui::FocusWindow(i32 windowID) { BringWindowToFront(windowID); }

    void Gui::UnfocusWindow()
    {
        // No window is focused: the GUI outside them takes the keys.
        S().keyboardControl = 0;
    }

    // ------------------------------------------------------------------------------------------------- focus by name

    void Gui::SetNextControlName(StringView name) { S().nextControlName = String(name); }

    String Gui::GetNameOfFocusedControl()
    {
        State& s = S();
        for (const Named& n : s.named)
            if (n.id == s.keyboardControl && s.keyboardControl != 0)
                return n.name;
        return String();
    }

    void Gui::FocusControl(StringView name)
    {
        State& s = S();
        if (name.Empty())
        {
            s.keyboardControl = 0;
            return;
        }
        for (const Named& n : s.named)
            if (n.name == name)
            {
                s.keyboardControl = n.id;
                return;
            }
        // Not seen yet in this pass: focused when the control asks for its id.
        s.focusName = String(name);
        s.focusByName = true;
    }

    // ------------------------------------------------------------------------------------------------- GuiUtility

    i32 GuiUtility::hotControl() { return S().hotControl; }
    void GuiUtility::SetHotControl(i32 value) { S().hotControl = value; }
    i32 GuiUtility::keyboardControl() { return S().keyboardControl; }

    void GuiUtility::SetKeyboardControl(i32 value)
    {
        State& s = S();
        if (s.keyboardControl != value)
            ClearEditor();
        s.keyboardControl = value;
    }

    String GuiUtility::systemCopyBuffer() { return engine::SystemClipboard::Text(); }
    void GuiUtility::SetSystemCopyBuffer(StringView value) { engine::SystemClipboard::SetText(value); }

    bool GuiUtility::hasModalWindow()
    {
        for (Window* w : S().windows)
            if (w->modal && w->seen)
                return true;
        return false;
    }

    i32 GuiUtility::GetControlID(FocusType focus) { return ControlId(0, focus, Rect()); }
    i32 GuiUtility::GetControlID(i32 hint, FocusType focus) { return ControlId(hint, focus, Rect()); }
    i32 GuiUtility::GetControlID(FocusType focus, Rect position) { return ControlId(0, focus, position); }
    i32 GuiUtility::GetControlID(i32 hint, FocusType focus, Rect position) { return ControlId(hint, focus, position); }
    i32 GuiUtility::GetControlID(GuiContent* contents, FocusType focus) { return ControlId(contents ? Hint(contents->text().View()) : 0, focus, Rect()); }

    i32 GuiUtility::GetControlID(GuiContent* contents, FocusType focus, Rect position)
    {
        return ControlId(contents ? Hint(contents->text().View()) : 0, focus, position);
    }

    void GuiUtility::ExitGui()
    {
        // The rest of the pass sees a used event (its controls do nothing).
        if (Event* e = S().current)
            e->SetType(EventType::Used);
    }

    Vector2 GuiUtility::GuiToScreenPoint(Vector2 guiPoint) { return S().toScreen.Apply(ToGuiSpace(guiPoint)); }

    Rect GuiUtility::GuiToScreenRect(Rect guiRect)
    {
        Vector2 a = GuiToScreenPoint(Vector2(guiRect.x, guiRect.y)), b = GuiToScreenPoint(Vector2(guiRect.x + guiRect.width, guiRect.y + guiRect.height));
        return Rect(a.x, a.y, b.x - a.x, b.y - a.y);
    }

    Vector2 GuiUtility::ScreenToGuiPoint(Vector2 screenPoint)
    {
        Vector2 g = S().toScreen.Inverse().Apply(screenPoint);
        Vector2 o = TopClip().origin;
        return Vector2(g.x - o.x, g.y - o.y);
    }

    Rect GuiUtility::ScreenToGuiRect(Rect screenRect)
    {
        Vector2 a = ScreenToGuiPoint(Vector2(screenRect.x, screenRect.y)), b = ScreenToGuiPoint(Vector2(screenRect.x + screenRect.width, screenRect.y + screenRect.height));
        return Rect(a.x, a.y, b.x - a.x, b.y - a.y);
    }

    Rect GuiUtility::AlignRectToDevice(Rect rect)
    {
        i32 w = 0, h = 0;
        return AlignRectToDevice(rect, w, h);
    }

    Rect GuiUtility::AlignRectToDevice(Rect rect, i32& widthInPixels, i32& heightInPixels)
    {
        // The corners on the screen's pixels, back in the GUI's space.
        Rect screen = GuiToScreenRect(rect);
        f32 x0 = std::round(screen.x), y0 = std::round(screen.y);
        f32 x1 = std::round(screen.x + screen.width), y1 = std::round(screen.y + screen.height);
        widthInPixels = i32(x1 - x0);
        heightInPixels = i32(y1 - y0);
        return ScreenToGuiRect(Rect(x0, y0, x1 - x0, y1 - y0));
    }

    void GuiUtility::RotateAroundPivot(f32 angle, Vector2 pivotPoint)
    {
        Vector2 p = GuiToScreenPoint(pivotPoint);
        Matrix4x4 m = Matrix4x4::Translate(Vector3(p.x, p.y, 0)) * Matrix4x4::Rotate(Quaternion::Euler(0, 0, angle)) * Matrix4x4::Translate(Vector3(-p.x, -p.y, 0));
        Gui::SetMatrix(m * S().matrix);
    }

    void GuiUtility::ScaleAroundPivot(Vector2 scale, Vector2 pivotPoint)
    {
        Vector2 p = GuiToScreenPoint(pivotPoint);
        Matrix4x4 m = Matrix4x4::Translate(Vector3(p.x, p.y, 0)) * Matrix4x4::Scale(Vector3(scale.x, scale.y, 1)) * Matrix4x4::Translate(Vector3(-p.x, -p.y, 0));
        Gui::SetMatrix(m * S().matrix);
    }

    // ------------------------------------------------------------------------------------------------- Event

    Event* Event::current() { return S().current; }
    void Event::SetCurrent(Event* e) { S().current = e; }

    EventType Event::GetTypeForControl(i32 controlID) const
    {
        State& s = S();
        // A held control takes the mouse; the others see it ignored.
        if (s.hotControl != 0 && s.hotControl != controlID &&
            (type_ == EventType::MouseDown || type_ == EventType::MouseUp || type_ == EventType::MouseDrag || type_ == EventType::MouseMove))
            return EventType::Ignore;
        return type_;
    }

    i32 Event::GetEventCount() { return i32(S().queue.Size()) - i32(S().popped); }

    bool Event::PopEvent(Event* outEvent)
    {
        State& s = S();
        if (!outEvent || s.popped >= s.queue.Size())
            return false;
        outEvent->CopyFrom(*s.queue[s.popped++]);
        return true;
    }
}
