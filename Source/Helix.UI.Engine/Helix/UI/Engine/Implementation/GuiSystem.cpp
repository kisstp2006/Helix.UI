#include <Helix/UI/Engine/Implementation/GuiState.h>
#include <Helix/UI/Engine/Implementation/Overlay.h>
#include <Helix/UI/Engine/ScriptPaint.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/PlayerLoop/PlayerLoop.h>
#include <Helix/Engine/Rendering/Pipeline/CommandBuffer.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Scripting/Bindings/Interop.h>

#include <bit>
#include <cmath>

// The immediate GUI's frame: the scripts with OnGui by depth, each event of the frame's input through them (a Layout
// pass before each event for the scripts that lay out), then Layout and Repaint as the frame is drawn. Input goes to
// the front-most script first and stops at the one that uses it; Repaint draws the deepest first.
namespace helix::ui::gui
{
    using namespace detail;

    namespace
    {
        LogChannel GuiLog{"UI"};

        // A script with OnGui, or a container's handler in its rectangle.
        struct Entry
        {
            engine::GameBehaviour* behaviour = nullptr;
            Owner* owner = nullptr;
            void (*call)(void*) = nullptr;
            void* user = nullptr;
            bool area = false;
            Rect rect;

            bool Alive() const { return behaviour ? behaviour->isActiveAndEnabled() : call != nullptr; }
            bool Layout() const { return behaviour ? behaviour->useGuiLayout() : true; }
            void Call() const
            {
                if (behaviour)
                    engine::CallScriptMessage(behaviour, engine::ScriptMessage::OnGui);
                else
                    call(user);
            }
        };

        struct Frame
        {
            u64 inputFrame = 0;
            // The frame's input events the GUI used.
            Array<bool> used;
            overlay::MeshUpload upload;
            Vector2 screen;
        };

        Frame& F()
        {
            static Frame* f = new Frame();
            return *f;
        }

        Owner* OwnerFor(u64 key)
        {
            State& s = S();
            for (Owner* o : s.owners)
                if (o->key == key)
                    return o;
            auto* o = new Owner();
            o->key = key;
            o->cache = NewLayoutCache();
            s.owners.Push(o);
            return o;
        }

        // The scripts with OnGui now, front-most first (the smaller depth in front; the later made among equal ones).
        Array<Entry> Collect()
        {
            State& s = S();
            for (Owner* o : s.owners)
                o->seen = false;
            Array<Entry> out;
            engine::ForEachObjectOfClass(engine::ClassOf<engine::GameBehaviour>(), [&](engine::Object* object) {
                auto* b = static_cast<engine::GameBehaviour*>(object);
                if (b->isActiveAndEnabled() && b->script() && b->script()->Has(engine::ScriptMessage::OnGui))
                {
                    Entry e;
                    e.behaviour = b;
                    e.owner = OwnerFor(b->GetEntityId().value);
                    e.owner->seen = true;
                    out.Push(e);
                }
                return true;
            });
            // Owners whose scripts went take their windows with them.
            for (usize i = 0; i < s.owners.Size();)
                if (!s.owners[i]->seen && !s.owners[i]->container)
                {
                    u64 key = s.owners[i]->key;
                    for (usize k = 0; k < s.windows.Size();)
                        if (s.windows[k]->owner == key)
                        {
                            delete s.windows[k];
                            s.windows.RemoveAt(k);
                        }
                        else
                            k++;
                    delete s.owners[i];
                    s.owners.RemoveAt(i);
                }
                else
                    i++;
            for (usize i = 1; i < out.Size(); i++)
                for (usize k = i; k > 0; k--)
                {
                    const Entry& a = out[k - 1];
                    const Entry& b = out[k];
                    bool before = b.owner->depth < a.owner->depth || (b.owner->depth == a.owner->depth && b.owner->key > a.owner->key);
                    if (!before)
                        break;
                    std::swap(out[k - 1], out[k]);
                }
            return out;
        }

        Vector2 Screen() { return Vector2(f32(engine::Screen::width()), f32(engine::Screen::height())); }

        // One script's pass of an event: Gui's state from the start, OnGui, then its windows.
        void Pass(const Entry& e, const Event& raw)
        {
            State& s = S();
            s.owner = e.owner;
            s.window = nullptr;
            s.ids = &e.owner->ids;
            e.owner->ids.at = 0;
            s.skin = engine::PPtr<GuiSkin>();
            s.color = s.backgroundColor = s.contentColor = Color(1, 1, 1, 1);
            s.enabled = true;
            s.changed = false;
            s.depth = e.owner->depth;
            s.tooltip.Clear();
            s.scrolls.Clear();
            s.clips.Clear();
            // The screen, or a container's rectangle (its top left the origin).
            Clip root;
            root.visible = e.area ? e.rect : Rect(-1e6f, -1e6f, 2e6f, 2e6f);
            root.origin = e.area ? Vector2(e.rect.x, e.rect.y) : Vector2(0, 0);
            s.clips.Push(root);
            s.raw.CopyFrom(raw);
            s.event.CopyFrom(raw);
            // A mouse event a window takes is ignored outside it.
            if ((raw.isMouse() || raw.isScrollWheel()) && s.target)
                s.event.SetType(EventType::Ignore);
            s.current = &s.event;
            s.inPass = true;
            Gui::SetMatrix(Matrix4x4::identity);
            bool layout = e.Layout();
            Vector2 screen = e.area ? Vector2(e.rect.width, e.rect.height) : F().screen;
            if (layout)
                BeginLayout(e.owner->cache, Rect(0, 0, screen.x, screen.y), nullptr);
            e.Call();
            if (layout)
                EndLayout(e.owner->cache);
            if (s.clips.Size() > 1)
            {
                HX_LOG_ERROR(GuiLog, "GUI: a group, clip or scroll view of OnGui was not ended.");
                while (s.clips.Size() > 1)
                    PopClip();
            }
            s.scrolls.Clear();
            e.owner->depth = s.depth;
            DoWindows(*e.owner);
            s.current = nullptr;
            s.inPass = false;
            s.owner = nullptr;
            s.window = nullptr;
            s.ids = nullptr;
        }

        // An event through the scripts (front to back; Repaint back to front), each after its Layout pass.
        // True when a script used the event.
        bool Round(Array<Entry>& entries, const Event& raw)
        {
            bool used = false;
            State& s = S();
            s.named.Clear();
            s.keyboardControls.Clear();
            ChooseTarget(s.screenMouse);
            bool repaint = raw.type() == EventType::Repaint;
            Event layout;
            layout.CopyFrom(raw);
            layout.SetType(EventType::Layout);
            for (usize n = 0; n < entries.Size(); n++)
            {
                const Entry& e = repaint ? entries[entries.Size() - 1 - n] : entries[n];
                if (!e.Alive())
                    continue;
                if (e.Layout())
                {
                    Pass(e, layout);
                    // The layout may have moved the windows' targets (a window grew).
                    ChooseTarget(s.screenMouse);
                }
                Pass(e, raw);
                if (!repaint && s.event.type() == EventType::Used)
                {
                    used = true;
                    break;
                }
            }
            // Tab moves the keyboard focus through the keyboard controls (shift: back) when no control used it.
            if (raw.type() == EventType::KeyDown && raw.keyCode() == platform::KeyCode::Tab && s.event.type() == EventType::KeyDown &&
                !s.keyboardControls.Empty() && s.keyboardControl != 0)
            {
                i32 at = -1;
                for (usize i = 0; i < s.keyboardControls.Size(); i++)
                    if (s.keyboardControls[i].id == s.keyboardControl)
                        at = i32(i);
                i32 count = i32(s.keyboardControls.Size());
                i32 next = raw.shift() ? (at <= 0 ? count - 1 : at - 1) : (at + 1) % count;
                GuiUtility::SetKeyboardControl(s.keyboardControls[usize(next)].id);
                used = true;
            }
            return used;
        }

        EventModifiers Modifiers(u8 m) { return EventModifiers(i32(m) & 15); }

        bool Run(Array<Entry>& entries, Event& e) { return Round(entries, e); }

        void DrawGui(engine::CommandBuffer& cmd)
        {
            system::Repaint();
            system::Draw(cmd);
        }

        // The module's GUI: its events before Update, its drawing after the panels.
        struct Registrar
        {
            Registrar()
            {
                engine::PlayerLoop::SetSystemFunction("IMGUISendQueuedEvents", [](void*) { system::SendQueuedEvents(); });
                overlay::SetGuiDrawer(&DrawGui);
            }
        } registrar;
    }

    namespace system
    {
        void SendQueuedEvents()
        {
            State& s = S();
            Frame& f = F();
            u64 frame = engine::InputFeed::FrameNumber();
            if (frame == f.inputFrame)
                return;
            f.inputFrame = frame;
            f.screen = Screen();
            Span<const engine::InputEvent> input = engine::InputFeed::Frame();
            for (Event* q : s.queue)
                delete q;
            s.queue.Clear();
            s.popped = 0;
            if (input.Empty())
                return;
            Array<Entry> entries = Collect();
            f.used.Clear();
            f.used.Resize(input.Size());
            for (usize index = 0; index < input.Size(); index++)
            {
                const engine::InputEvent& in = input[index];
                f.used[index] = false;
                // The immediate GUI is the main display's.
                if (in.display != 0)
                    continue;
                const platform::Event& p = in.event;
                Event e;
                e.SetModifiers(Modifiers(p.modifiers));
                e.SetMousePosition(s.screenMouse);
                switch (p.type)
                {
                case platform::EventType::MouseMove:
                {
                    Vector2 at(p.mouse.x, p.mouse.y);
                    e.SetType(s.heldButtons ? EventType::MouseDrag : EventType::MouseMove);
                    e.SetDelta(Vector2(at.x - s.screenMouse.x, at.y - s.screenMouse.y));
                    s.screenMouse = at;
                    e.SetMousePosition(at);
                    for (i32 b = 0; b < 3; b++)
                        if (s.heldButtons & (1 << b))
                        {
                            e.SetButton(b);
                            break;
                        }
                    break;
                }
                case platform::EventType::MouseDown:
                case platform::EventType::MouseUp:
                {
                    Vector2 at(p.mouse.x, p.mouse.y);
                    s.screenMouse = at;
                    e.SetMousePosition(at);
                    i32 button = i32(p.mouse.button);
                    e.SetButton(button);
                    if (p.type == platform::EventType::MouseDown)
                    {
                        f64 now = in.time;
                        f32 dx = at.x - s.lastClickAt.x, dy = at.y - s.lastClickAt.y;
                        s.clickCount = (now - s.lastClickTime < 0.4 && dx * dx + dy * dy < 16) ? s.clickCount + 1 : 1;
                        s.lastClickTime = now;
                        s.lastClickAt = at;
                        s.heldButtons |= 1 << button;
                        e.SetType(EventType::MouseDown);
                    }
                    else
                    {
                        s.heldButtons &= ~(1 << button);
                        e.SetType(EventType::MouseUp);
                    }
                    e.SetClickCount(s.clickCount);
                    break;
                }
                case platform::EventType::MouseWheel:
                    e.SetType(EventType::ScrollWheel);
                    // Lines down are positive (three to a notch).
                    e.SetDelta(Vector2(p.wheel.dx * 3, -p.wheel.dy * 3));
                    break;
                case platform::EventType::KeyDown:
                case platform::EventType::KeyUp:
                    e.SetType(p.type == platform::EventType::KeyDown ? EventType::KeyDown : EventType::KeyUp);
                    e.SetKeyCode(p.key.key);
                    if (p.key.key >= platform::KeyCode::F1 && p.key.key <= platform::KeyCode::F15)
                        e.SetModifiers(EventModifiers(i32(e.modifiers()) | i32(EventModifiers::FunctionKey)));
                    break;
                case platform::EventType::Text:
                {
                    // A character comes as a key down of no key (a pair of them beyond the 16 bit range).
                    u32 c = p.text.codepoint;
                    if (c < 32 || c == 127)
                        continue;
                    e.SetType(EventType::KeyDown);
                    if (c >= 0x10000)
                    {
                        u32 v = c - 0x10000;
                        Event high(&e);
                        high.SetCharacter(char16_t(0xD800 + (v >> 10)));
                        s.queue.Push(new Event(&high));
                        if (!entries.Empty())
                            Run(entries, high);
                        e.SetCharacter(char16_t(0xDC00 + (v & 0x3FF)));
                    }
                    else
                        e.SetCharacter(char16_t(c));
                    break;
                }
                default:
                    continue;
                }
                e.SetRawType(e.type());
                s.queue.Push(new Event(&e));
                if (!entries.Empty() && Run(entries, e))
                    f.used[index] = true;
                // A right click asks for the context menu after its up.
                if (p.type == platform::EventType::MouseUp && p.mouse.button == platform::MouseButton::Right)
                {
                    Event c(&e);
                    c.SetType(EventType::ContextClick);
                    c.SetRawType(EventType::ContextClick);
                    s.queue.Push(new Event(&c));
                    if (!entries.Empty())
                        Run(entries, c);
                }
            }
        }

        bool UsedInput(u64 inputFrame, usize index)
        {
            Frame& f = F();
            return inputFrame == f.inputFrame && index < f.used.Size() && f.used[index];
        }

        void Repaint()
        {
            State& s = S();
            F().screen = Screen();
            s.Mesh().Clear();
            Array<Entry> entries = Collect();
            for (Window* w : s.windows)
                w->called = false;
            Event e;
            e.SetType(EventType::Repaint);
            e.SetRawType(EventType::Repaint);
            e.SetMousePosition(s.screenMouse);
            if (!entries.Empty())
                Round(entries, e);
            // Windows not called this frame are gone.
            for (usize k = 0; k < s.windows.Size();)
                if (!s.windows[k]->called)
                {
                    delete s.windows[k];
                    s.windows.RemoveAt(k);
                }
                else
                    k++;
        }

        const UIMesh& Mesh() { return S().Mesh(); }

        void Draw(engine::CommandBuffer& cmd)
        {
            State& s = S();
            if (s.Mesh().Vertices().Empty())
                return;
            Frame& f = F();
            f.upload.Upload(s.Mesh());
            Vector2 screen = f.screen;
            cmd.SetViewProjectionMatrices(Matrix4x4::identity, Matrix4x4::Ortho(0, screen.x, screen.y, 0, -100000, 100000));
            f.upload.Draw(s.Mesh(), cmd);
        }

        void ResetForTests()
        {
            State& s = S();
            for (Window* w : s.windows)
                delete w;
            s.windows.Clear();
            for (Owner* o : s.owners)
                delete o;
            s.owners.Clear();
            s.scrollStates.Clear();
            ClearEditor();
            s.hotControl = s.keyboardControl = 0;
            s.heldButtons = 0;
            s.clickCount = 0;
            s.lastClickTime = -1;
            s.screenMouse = Vector2(0, 0);
            s.focusByName = false;
            s.Mesh().Clear();
            for (Event* q : s.queue)
                delete q;
            s.queue.Clear();
            s.popped = 0;
            F().inputFrame = engine::InputFeed::FrameNumber();
            ResetLayoutForTests();
        }
    }

    // ------------------------------------------------------------------------------------------------ containers

    namespace container
    {
        namespace
        {
            // A container's owner: its key apart from the scripts' entity ids.
            constexpr u64 kContainerBit = u64(1) << 63;

            Owner* OwnerOf(u64 key)
            {
                State& s = S();
                u64 k = key | kContainerBit;
                for (Owner* o : s.owners)
                    if (o->key == k)
                        return o;
                auto* o = new Owner();
                o->key = k;
                o->container = true;
                o->cache = NewLayoutCache();
                s.owners.Push(o);
                return o;
            }
        }

        bool Run(u64 key, const Rect& rect, const Event& e, void (*handler)(void* user), void* user, UIMesh* mesh)
        {
            State& s = S();
            // Not inside another pass (a handler that draws a container's panel).
            if (s.inPass || !handler)
                return false;
            Entry entry;
            entry.owner = OwnerOf(key);
            entry.call = handler;
            entry.user = user;
            entry.area = true;
            entry.rect = rect;
            // The pointer and the buttons the event tells of.
            EventType type = e.type();
            if (type == EventType::MouseDown || type == EventType::MouseUp || type == EventType::MouseMove || type == EventType::MouseDrag ||
                type == EventType::ScrollWheel || type == EventType::ContextClick)
                s.screenMouse = e.mousePosition();
            if (type == EventType::MouseDown)
                s.heldButtons |= 1 << e.button();
            else if (type == EventType::MouseUp)
                s.heldButtons &= ~(1 << e.button());
            // Repaint draws into the panel's mesh; its clip and map are the panel's again after.
            UIMesh* saved = s.meshTarget;
            UIClip clip;
            Affine map;
            if (mesh)
            {
                clip = mesh->Clip();
                map = mesh->Transform();
                s.meshTarget = mesh;
            }
            Array<Entry> entries;
            entries.Push(entry);
            bool used = Round(entries, e);
            if (mesh)
            {
                mesh->SetClip(clip);
                mesh->SetTransform(map);
            }
            s.meshTarget = saved;
            // Windows of the container not called in its Repaint are gone.
            if (type == EventType::Repaint)
            {
                for (usize k = 0; k < s.windows.Size();)
                {
                    if (s.windows[k]->owner == entry.owner->key && !s.windows[k]->called)
                    {
                        delete s.windows[k];
                        s.windows.RemoveAt(k);
                    }
                    else
                        k++;
                }
            }
            return used;
        }

        void Remove(u64 key)
        {
            State& s = S();
            u64 k = key | kContainerBit;
            for (usize i = 0; i < s.windows.Size();)
                if (s.windows[i]->owner == k)
                {
                    delete s.windows[i];
                    s.windows.RemoveAt(i);
                }
                else
                    i++;
            for (usize i = 0; i < s.owners.Size(); i++)
                if (s.owners[i]->key == k)
                {
                    delete s.owners[i];
                    s.owners.RemoveAt(i);
                    break;
                }
        }

        // ---------------------------------------------------------------------------- C#'s ImguiContainer

        namespace
        {
            // C#'s: runs the onGuiHandler of the container with the key.
            using ManagedHandler = void (*)(u64 key);
            ManagedHandler managedHandler = nullptr;

            void CallManaged(void* user)
            {
                if (managedHandler)
                    managedHandler(u64(uintptr_t(user)));
            }

            void SetCallback(void* function) { managedHandler = std::bit_cast<ManagedHandler>(function); }

            // An event of the container's element: its type, the pointer (the screen's), its move, button, clicks,
            // modifiers, key and character. 1 when a control used it.
            i32 SendEvent(u64 key, const f32* rect, i32 type, const f32* position, const f32* delta, i32 button, i32 clicks, i32 modifiers,
                          i32 keyCode, i32 character)
            {
                Event e;
                e.SetType(EventType(type));
                e.SetRawType(EventType(type));
                e.SetMousePosition(Vector2(position[0], position[1]));
                e.SetDelta(Vector2(delta[0], delta[1]));
                e.SetButton(button);
                e.SetClickCount(clicks);
                e.SetModifiers(EventModifiers(modifiers));
                e.SetKeyCode(platform::KeyCode(keyCode));
                e.SetCharacter(char16_t(character));
                return Run(key, Rect(rect[0], rect[1], rect[2], rect[3]), e, &CallManaged, reinterpret_cast<void*>(uintptr_t(key)), nullptr) ? 1 : 0;
            }

            // Its Repaint (from its generateVisualContent: the paint context's mesh, its box of a size at the context's
            // origin).
            void Repaint(u64 key, u64 paintContext, f32 width, f32 height)
            {
                auto* c = reinterpret_cast<ScriptPaintContext*>(uintptr_t(paintContext));
                if (!c || !c->mesh)
                    return;
                Event e;
                e.SetType(EventType::Repaint);
                e.SetRawType(EventType::Repaint);
                e.SetMousePosition(S().screenMouse);
                Run(key, Rect(c->origin.x, c->origin.y, width, height), e, &CallManaged, reinterpret_cast<void*>(uintptr_t(key)), c->mesh);
            }

            void RemoveExport(u64 key) { Remove(key); }

            struct ExportRegistrar
            {
                ExportRegistrar()
                {
                    scripting::RegisterNativeExport("HelixUI.SetGuiContainerCallback", std::bit_cast<const void*>(&SetCallback));
                    scripting::RegisterNativeExport("HelixUI.GuiContainerEvent", std::bit_cast<const void*>(&SendEvent));
                    scripting::RegisterNativeExport("HelixUI.GuiContainerRepaint", std::bit_cast<const void*>(&Repaint));
                    scripting::RegisterNativeExport("HelixUI.GuiContainerRemove", std::bit_cast<const void*>(&RemoveExport));
                }
            } registrar;
        }
    }
}
