#include <Helix/UI/Engine/UIView.h>
#include <Helix/UI/Engine/PanelInput.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Transform/Transform.h>
#include <Helix/UI/Engine/PanelInputConfiguration.h>
#include <Helix/Engine/Input/Cursor.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/Pipeline/CommandBuffer.h>
#include <Helix/Engine/Rendering/Pipeline/RenderTargets.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Texture/RenderTexture.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Platform/Time/Time.h>
#include <Helix/UI/Engine/Control.h>
#include <Helix/UI/Engine/ControlParts.h>
#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Engine/FontAsset.h>
#include <Helix/UI/Engine/Gui.h>
#include <Helix/UI/Engine/ImageAtlas.h>
#include <Helix/UI/Engine/Implementation/Overlay.h>
#include <Helix/Engine/Screen/Display.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace helix::ui
{
    namespace
    {
        LogChannel UILog{"UI"};

        // How often watched files are read again (seconds).
        constexpr f64 kWatchInterval = 0.5;

        // The fonts every panel falls back to after its styles' fonts: families of the platform (the first one the
        // machine has is the default font).
#if defined(_WIN32)
        constexpr const char* kDefaultFamilies[] = {"Segoe UI", "Arial", "Segoe UI Emoji", "Segoe UI Symbol", "Microsoft YaHei", "Yu Gothic", "Malgun Gothic",
                                                    "Nirmala UI"};
#elif defined(__ANDROID__)
        constexpr const char* kDefaultFamilies[] = {"Roboto", "Noto Sans", "Noto Color Emoji", "Noto Sans CJK SC"};
#else
        constexpr const char* kDefaultFamilies[] = {"DejaVu Sans", "Liberation Sans", "Noto Sans", "Noto Color Emoji", "Noto Sans CJK SC"};
#endif

        struct MaterialEntry
        {
            bool world = false;
            const void* textures[UIBatch::kSlots] = {};
            engine::PPtr<engine::Texture> alive[UIBatch::kSlots];
            engine::PPtr<engine::Material> material;
        };

        // The cursor the panels last set (none: the game's own), so it is set again only when it changes.
        struct CursorKey
        {
            bool ui = false, set = false;
            Atom image = kNoAtom;
            CursorShape shape = CursorShape::Arrow;
            f32 hotspot[2] = {0, 0};
            bool operator==(const CursorKey& o) const
            {
                return ui == o.ui && set == o.set && image == o.image && shape == o.shape && hotspot[0] == o.hotspot[0] && hotspot[1] == o.hotspot[1];
            }
        };

        bool ReadSource(StringView path, Array<u8>& out)
        {
            const engine::AssetEntry* e = engine::FindAssetAtPath(path);
            String error;
            return e && e->source && e->source->Read(e->path.View(), out, error);
        }
    }

    // A panel of the views of one settings asset (or of one view without settings): the elements, their fonts and
    // textures, the geometry of the last update.
    class RuntimePanel
    {
    public:
        class TextHost;

        RuntimePanel(UIPanelSettings* settings, u64 serial, bool world);
        ~RuntimePanel();

        UIPanelSettings* settings() const { return ScriptObjectAs<UIPanelSettings>(settings_.Get()); }
        bool Owns(UIPanelSettings* s) const { return shared_ && settings() == s; }
        bool shared() const { return shared_; }
        // In the scene, placed by its first view's GameObject.
        bool world() const { return world_; }
        const Matrix4x4& toWorld() const { return toWorld_; }
        Panel& panel() { return *panel_; }
        Span<UIView* const> views() const { return Span<UIView* const>(views_.Data(), views_.Size()); }
        // Draw order among the panels: the settings' sorting order (a lone view's own), then the order of creation.
        f32 order() const { return shared_ ? (settings() ? settings()->sortingOrder() : 0.0f) : (views_.Empty() ? 0.0f : views_[0]->sortingOrder()); }
        u64 serial() const { return serial_; }
        engine::RenderTexture* target() const { return !world_ && settings() ? settings()->targetTexture() : nullptr; }
        // The GameObject of its first view (whose content it is: the editor's or the game's).
        engine::GameObject* owner() const { return views_.Empty() ? nullptr : views_[0]->gameObject(); }
        // The game's content while the editor keeps it apart (drawn into its display's texture).
        bool game() const { return engine::Graphics::SeparateEditorContent() && !engine::Graphics::IsEditorObject(owner()); }
        // The display a screen panel draws to and takes input from: its settings' targetDisplay while that display is
        // active, else the main one (the game's: its own, into that display's texture).
        i32 display() const
        {
            i32 d = !world_ && settings() ? settings()->targetDisplay() : 0;
            if (game())
                return d > 0 ? d : 0;
            return d > 0 && engine::Displays::Get(d).active ? d : 0;
        }
        f32 scale() const { return scale_; }
        Vector2 size() const { return size_; }
        Vector2 ToPanel(Vector2 screen) const { return Vector2(screen.x / scale_, screen.y / scale_); }
        // Where a screen point (pixels from the top left) is on the panel and how far (0 on the screen; world space
        // panels: along the event cameras' rays); false when it is not on it.
        bool ScreenToPanel(Vector2 screen, Vector2& at, f32& distance);
        // Takes the screen's pointers at all (texture panels only through their settings' function).
        bool TakesInput();
        engine::Mesh* mesh() const { return upload_.mesh(); }
        const UIMesh& uiMesh() const { return uiMesh_; }
        const overlay::MeshUpload& upload() const { return upload_; }
        TextHost& host() { return *host_; }

        void Add(UIView* view);
        void Remove(UIView* view);
        // The view's root among the others' by their sorting orders.
        void Place(UIView* view);
        // Styles, layout and geometry as of now, in the target's size divided by the scale.
        void Update();

    private:
        void ApplyTheme();
        void Upload();
        void UpdateWorld();
        void UpdateTooltip();
        // The tooltip: the element hovered with one, since when, the label shown.
        ElementHandle tipFor_, tipLabel_;
        f64 tipSince_ = 0;
        Vector2 tipAt_ = Vector2(0, 0);

        engine::PPtr<engine::ScriptableObject> settings_;
        bool shared_;
        bool world_;
        Matrix4x4 toWorld_ = Matrix4x4::identity;
        u64 serial_;
        std::unique_ptr<TextHost> host_;
        std::unique_ptr<Panel> panel_;
        Array<UIView*> views_;
        const StyleSheetData* theme_ = nullptr;
        bool themed_ = false;
        UIMesh uiMesh_;
        overlay::MeshUpload upload_;
        f32 scale_ = 1;
        Vector2 size_ = Vector2(0, 0);
    };

    namespace
    {
        struct Views
        {
            Array<UIView*> list;
            Array<RuntimePanel*> panels;
            u64 serial = 0;
            u32 drawer = 0;
            // The input frame the panels read last, and the panel the keys go to (the last one pressed in).
            u64 inputFrame = 0;
            i64 tickFrame = -1;
            RuntimePanel* keyboard = nullptr;
            // An event system outside the UI takes the input (PanelInput).
            bool eventSystem = false;
            CursorKey cursor;
            // One material per set of textures in the slots.
            Array<MaterialEntry> materials;
            bool missingShader = false;
            ImageAtlas* atlas = nullptr;
            // The immediate GUI's drawing after the panels.
            void (*gui)(engine::CommandBuffer&) = nullptr;
        };

        Views& V()
        {
            static Views* v = new Views();
            return *v;
        }

        engine::Material* MaterialFor(const UIBatch& batch, bool world = false)
        {
            Views& v = V();
            // Entries whose textures went make room.
            for (usize i = 0; i < v.materials.Size();)
            {
                bool gone = !v.materials[i].material.Get();
                for (u32 k = 0; k < UIBatch::kSlots; k++)
                    gone = gone || (v.materials[i].textures[k] && !v.materials[i].alive[k].Get());
                if (gone)
                {
                    if (engine::Material* m = v.materials[i].material.Get())
                        engine::Object::DestroyImmediate(m);
                    v.materials.RemoveAt(i);
                }
                else
                    i++;
            }
            for (const MaterialEntry& e : v.materials)
            {
                bool same = e.world == world;
                for (u32 k = 0; k < UIBatch::kSlots; k++)
                    same = same && e.textures[k] == batch.textures[k];
                if (same)
                    return e.material.Get();
            }
            engine::Shader* shader = engine::Shader::Find("Hidden/UI/Default");
            if (!shader)
            {
                if (!v.missingShader)
                    HX_LOG_WARNING(UILog, "The shader Hidden/UI/Default is not loaded: the UI is not drawn.");
                v.missingShader = true;
                return nullptr;
            }
            engine::Material* m = engine::Material::Create(shader);
            m->hideFlags = engine::HideFlags::DontSave;
            static const char* kNames[UIBatch::kSlots] = {"_Tex0", "_Tex1", "_Tex2", "_Tex3"};
            MaterialEntry entry;
            entry.world = world;
            if (world)
            {
                // Depth tested against the scene, drawn with its transparent objects.
                m->SetFloat("_UIZTest", 4.0f);
                m->SetRenderQueue(3000);
            }
            for (u32 k = 0; k < UIBatch::kSlots; k++)
            {
                auto* t = static_cast<engine::Texture*>(const_cast<void*>(batch.textures[k]));
                entry.textures[k] = batch.textures[k];
                entry.alive[k] = engine::PPtr<engine::Texture>(t);
                if (t)
                    m->SetTexture(kNames[k], t);
            }
            entry.material = engine::PPtr<engine::Material>(m);
            v.materials.Push(entry);
            return m;
        }

        ImageAtlas& Atlas()
        {
            Views& v = V();
            if (!v.atlas)
                v.atlas = new ImageAtlas();
            return *v.atlas;
        }

        // The panels in drawing order: by sorting order, the earlier made under the later among equal ones.
        Array<RuntimePanel*> DrawOrder()
        {
            Array<RuntimePanel*> order(V().panels);
            for (usize i = 1; i < order.Size(); i++)
                for (usize j = i; j > 0 && (order[j - 1]->order() > order[j]->order() ||
                                            (order[j - 1]->order() == order[j]->order() && order[j - 1]->serial() > order[j]->serial()));
                     j--)
                    std::swap(order[j - 1], order[j]);
            return order;
        }

        void DrawPanel(RuntimePanel& p, engine::CommandBuffer& cmd) { p.upload().Draw(p.uiMesh(), cmd); }

        // The panels, once a frame after the cameras: over the screen (pixels from the top left mapped to the target)
        // or into their target textures.
        void Draw(void*, engine::CommandBuffer& cmd)
        {
            const engine::RenderTarget& screen = engine::Graphics::Target();
            bool onScreen = true;
            // The display drawn now: its screen panels; panels into textures and the immediate GUI with the main one.
            i32 display = engine::Displays::Current();
            for (RuntimePanel* p : DrawOrder())
            {
                if (p->world())
                    continue;
                if (p->target() ? display != 0 || engine::Graphics::DrawingContent() == engine::DrawnContent::Game
                                : !engine::Graphics::DrawsNow(p->owner(), p->display()))
                    continue;
                engine::Mesh* mesh = p->mesh();
                Vector2 size = p->size();
                if (!mesh || size.x <= 0 || size.y <= 0)
                    continue;
                UIPanelSettings* s = p->settings();
                engine::RenderTexture* target = p->target();
                bool clearColor = s && s->clearColor(), clearDepth = s && s->clearDepthStencil();
                if (target)
                {
                    cmd.SetRenderTarget(engine::RenderTargetIdentifier(target));
                    onScreen = false;
                    cmd.SetViewport(Rect(0, 0, f32(target->width()), f32(target->height())));
                    if (clearColor || clearDepth)
                        cmd.ClearRenderTarget(clearDepth, clearColor, s->colorClearValue(), s->depthClearValue());
                }
                else
                {
                    if (!onScreen)
                        cmd.SetRenderTarget(engine::RenderTargetIdentifier(engine::BuiltinRenderTextureType::CameraTarget));
                    onScreen = true;
                    cmd.SetViewport(Rect(0, 0, f32(screen.width), f32(screen.height)));
                    // The screen's color holds what the cameras drew: only an asked clear takes it.
                    if (clearColor)
                        cmd.ClearRenderTarget(clearDepth, true, s->colorClearValue(), s->depthClearValue());
                }
                if (mesh->vertexCount() == 0)
                    continue;
                // Depth for elements turned in 3D (seen along z on the screen).
                cmd.SetViewProjectionMatrices(Matrix4x4::identity, Matrix4x4::Ortho(0, size.x, size.y, 0, -100000, 100000));
                DrawPanel(*p, cmd);
            }
            if (!onScreen)
                cmd.SetRenderTarget(engine::RenderTargetIdentifier(engine::BuiltinRenderTextureType::CameraTarget));
            // The immediate GUI over everything (it sets its own view).
            if (V().gui && display == 0)
            {
                cmd.SetViewport(Rect(0, 0, f32(screen.width), f32(screen.height)));
                V().gui(cmd);
            }
        }

        // The overlay is there while there are panels or a GUI.
        void UpdateDrawer()
        {
            Views& v = V();
            bool wanted = !v.panels.Empty() || v.gui;
            if (wanted && !v.drawer)
                v.drawer = engine::Graphics::screenOverlayRendering().Add(&Draw);
            else if (!wanted && v.drawer)
            {
                engine::Graphics::screenOverlayRendering().Remove(v.drawer);
                v.drawer = 0;
            }
        }

        // ----------------------------------------------------------------------------------------------- input

        // The panels that take the screen's input top-most first: by sorting order, the later made over the earlier
        // among equal ones (world space panels after the screen's, ordered for each pointer by distance).
        Array<RuntimePanel*> InputOrder()
        {
            Array<RuntimePanel*> draw = DrawOrder();
            Array<RuntimePanel*> order;
            for (usize i = draw.Size(); i-- > 0;)
                if (draw[i]->TakesInput())
                    order.Push(draw[i]);
            return order;
        }

        using PanelOrder = Span<RuntimePanel* const>;

        // Where a pointer is on each panel: the screen's first, then the hit ones nearest first, then the missed.
        struct PointerTarget
        {
            RuntimePanel* panel;
            Vector2 at;
            f32 distance;
            bool on;
        };

        // A place no element is at (a panel the pointer is not on).
        constexpr f32 kOff = -1.0e9f;

        Array<PointerTarget> Targets(PanelOrder order, Vector2 screen)
        {
            Array<PointerTarget> out, far;
            usize screenCount = 0;
            for (RuntimePanel* p : order)
            {
                PointerTarget t{p, Vector2(kOff, kOff), 0, false};
                t.on = p->ScreenToPanel(screen, t.at, t.distance);
                if (!t.on)
                {
                    t.at = Vector2(kOff, kOff);
                    far.Push(t);
                    continue;
                }
                if (t.distance <= 0)
                {
                    out.Insert(screenCount++, t);
                    continue;
                }
                usize at = screenCount;
                while (at < out.Size() && out[at].distance <= t.distance)
                    at++;
                out.Insert(at, t);
            }
            for (const PointerTarget& t : far)
                out.Push(t);
            return out;
        }

        // A pointer's input to every panel, the top-most (or nearest) first: the first one with an element under the
        // point has it, the panels behind are covered there (their capture still gets it). Positions are the panel's
        // pixels.
        template <class F>
        void ToPointer(PanelOrder order, Vector2 screen, bool press, F&& send)
        {
            bool covered = false;
            for (const PointerTarget& t : Targets(order, screen))
            {
                EventSystem& events = t.panel->panel().events();
                bool hit = !covered && t.on && events.Pick(t.at).IsValid();
                send(events, t.at, covered || !t.on);
                if (hit)
                {
                    covered = true;
                    if (press)
                        V().keyboard = t.panel;
                }
            }
        }

        // The panel keys go to: the last one pressed in while it is there, else the top-most.
        EventSystem* KeyboardPanel(PanelOrder order)
        {
            for (RuntimePanel* p : order)
                if (p == V().keyboard)
                    return &p->panel().events();
            return order.Empty() ? nullptr : &order[0]->panel().events();
        }

        constexpr i32 TouchId(i32 platformId) { return PointerId::touchBase + i32(u32(platformId) % u32(PointerId::touchCount)); }

        Vector2 Where(const platform::MouseData& m) { return Vector2(m.x, m.y); }

        // How each kind of the window's input reaches the panels.
        struct InputRoute
        {
            platform::EventType type;
            void (*route)(PanelOrder order, const engine::InputEvent& e);
        };

        const InputRoute kInputRoutes[] = {
            {platform::EventType::MouseMove,
             [](PanelOrder order, const engine::InputEvent& e) {
                 ToPointer(order, Where(e.event.mouse), false, [&](EventSystem& events, Vector2 at, bool covered) {
                     events.PointerMove(PointerId::mouse, PointerKind::Mouse, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::MouseDown,
             [](PanelOrder order, const engine::InputEvent& e) {
                 ToPointer(order, Where(e.event.mouse), true, [&](EventSystem& events, Vector2 at, bool covered) {
                     events.PointerDown(PointerId::mouse, PointerKind::Mouse, i32(e.event.mouse.button), at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::MouseUp,
             [](PanelOrder order, const engine::InputEvent& e) {
                 ToPointer(order, Where(e.event.mouse), false, [&](EventSystem& events, Vector2 at, bool covered) {
                     events.PointerUp(PointerId::mouse, PointerKind::Mouse, i32(e.event.mouse.button), at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::MouseWheel,
             [](PanelOrder order, const engine::InputEvent& e) {
                 for (RuntimePanel* p : order)
                 {
                     EventSystem& events = p->panel().events();
                     Vector2 at = events.PointerPosition(PointerId::mouse);
                     bool hit = events.Pick(at).IsValid();
                     events.Wheel(at, Vector2(e.event.wheel.dx, e.event.wheel.dy), e.event.modifiers, e.time);
                     if (hit)
                         break;
                 }
             }},
            {platform::EventType::TouchBegin,
             [](PanelOrder order, const engine::InputEvent& e) {
                 ToPointer(order, Vector2(e.event.touch.x, e.event.touch.y), true, [&](EventSystem& events, Vector2 at, bool covered) {
                     events.PointerDown(TouchId(e.event.touch.id), PointerKind::Touch, 0, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::TouchMove,
             [](PanelOrder order, const engine::InputEvent& e) {
                 ToPointer(order, Vector2(e.event.touch.x, e.event.touch.y), false, [&](EventSystem& events, Vector2 at, bool covered) {
                     events.PointerMove(TouchId(e.event.touch.id), PointerKind::Touch, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::TouchEnd,
             [](PanelOrder order, const engine::InputEvent& e) {
                 ToPointer(order, Vector2(e.event.touch.x, e.event.touch.y), false, [&](EventSystem& events, Vector2 at, bool covered) {
                     events.PointerUp(TouchId(e.event.touch.id), PointerKind::Touch, 0, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::TouchCancel,
             [](PanelOrder order, const engine::InputEvent& e) {
                 for (RuntimePanel* p : order)
                     p->panel().events().PointerCancel(TouchId(e.event.touch.id), e.time);
             }},
            {platform::EventType::KeyDown,
             [](PanelOrder order, const engine::InputEvent& e) {
                 if (EventSystem* events = KeyboardPanel(order))
                     events->KeyDown(u16(e.event.key.key), e.event.modifiers, e.time);
             }},
            {platform::EventType::KeyUp,
             [](PanelOrder order, const engine::InputEvent& e) {
                 if (EventSystem* events = KeyboardPanel(order))
                     events->KeyUp(u16(e.event.key.key), e.event.modifiers, e.time);
             }},
            {platform::EventType::Text,
             [](PanelOrder order, const engine::InputEvent& e) {
                 if (EventSystem* events = KeyboardPanel(order))
                     events->Character(e.event.text.codepoint, e.event.modifiers, e.time);
             }},
            {platform::EventType::Composition,
             [](PanelOrder order, const engine::InputEvent& e) {
                 // The feed holds the composition as of the frame's last change.
                 if (EventSystem* events = KeyboardPanel(order))
                     events->SetComposition(engine::InputFeed::Composition(), e.event.composition.cursor, e.time);
             }},
        };

        // The system shapes of the cursor keywords.
        constexpr platform::SystemCursor kShapes[] = {
            platform::SystemCursor::Arrow,            platform::SystemCursor::Text,               platform::SystemCursor::ResizeVertical,
            platform::SystemCursor::ResizeHorizontal, platform::SystemCursor::Hand,               platform::SystemCursor::ResizeHorizontal,
            platform::SystemCursor::ResizeDiagonalUp, platform::SystemCursor::ResizeDiagonalDown, platform::SystemCursor::Move,
            platform::SystemCursor::Arrow,            platform::SystemCursor::ResizeDiagonalDown, platform::SystemCursor::Arrow,
            platform::SystemCursor::Arrow,            platform::SystemCursor::Move,               platform::SystemCursor::Arrow,
            platform::SystemCursor::Arrow,            platform::SystemCursor::Crosshair,          platform::SystemCursor::ResizeVertical,
            platform::SystemCursor::ResizeHorizontal, platform::SystemCursor::NotAllowed,         platform::SystemCursor::Wait,
            platform::SystemCursor::Crosshair,
        };
        static_assert(sizeof(kShapes) / sizeof(kShapes[0]) == u32(CursorShape::Crosshair) + 1);

        engine::Texture2D* CursorTextureOf(RuntimePanel* p, Atom path);

        // The cursor of the element under the mouse in the top-most panel that has one there (its style's,
        // inherited); set on the engine's Cursor when it changes, the arrow again when the mouse leaves the elements.
        void UpdateCursor(PanelOrder order)
        {
            Views& v = V();
            CursorKey key;
            RuntimePanel* over = nullptr;
            for (RuntimePanel* p : order)
            {
                EventSystem& events = p->panel().events();
                ElementHandle under = events.Pick(events.PointerPosition(PointerId::mouse));
                if (!under.IsValid())
                    continue;
                const ComputedStyle& s = p->panel().styles().Computed(under);
                key.ui = true;
                key.set = s.cursorSet;
                key.image = s.cursorImage;
                key.shape = s.cursorShape;
                key.hotspot[0] = s.cursorHotspot[0], key.hotspot[1] = s.cursorHotspot[1];
                over = p;
                break;
            }
            if (key == v.cursor)
                return;
            bool wasUi = v.cursor.ui && v.cursor.set;
            v.cursor = key;
            if (!key.set)
            {
                // Off the panels' cursors: the arrow (a game's own cursor stays while the mouse is elsewhere).
                if (wasUi)
                    engine::Cursor::SetSystemCursor(platform::SystemCursor::Arrow);
                return;
            }
            if (key.image == kNoAtom)
            {
                engine::Cursor::SetSystemCursor(kShapes[u32(key.shape)]);
                return;
            }
            engine::Texture2D* texture = over ? CursorTextureOf(over, key.image) : nullptr;
            if (texture)
                engine::Cursor::SetCursor(texture, Vector2(key.hotspot[0], key.hotspot[1]), engine::CursorMode::Auto);
            else
                engine::Cursor::SetSystemCursor(platform::SystemCursor::Arrow);
        }

        // The frame's input to the panels, once a frame (by the first view that updates).
        void ReadInput()
        {
            Views& v = V();
            u64 frame = engine::InputFeed::FrameNumber();
            i64 engineFrame = engine::Time::frameCount();
            bool input = frame != v.inputFrame;
            if (!input && engineFrame == v.tickFrame)
                return;
            // The scripts' scheduled items and the controls' timed work, once a frame (a frame of input or of the
            // engine: a run without input still ticks).
            v.tickFrame = engineFrame;
            Element::TickScripts(engine::Time::realtimeSinceStartupAsDouble());
            TickControls(engine::Time::realtimeSinceStartupAsDouble());
            if (!input)
                return;
            v.inputFrame = frame;
            Array<RuntimePanel*> order = InputOrder();
            if (order.Empty())
                return;
            PanelOrder panels(order.Data(), order.Size());
            // Redirected: the event system hands the panels their input.
            if (!PanelInput::ReadsOwnInput())
            {
                UpdateCursor(panels);
                return;
            }
            Span<const engine::InputEvent> events = engine::InputFeed::Frame();
            Array<RuntimePanel*> ofDisplay;
            for (usize i = 0; i < events.Size(); i++)
            {
                // What the immediate GUI used is not the panels'.
                if (gui::system::UsedInput(frame, i))
                    continue;
                const engine::InputEvent& e = events[i];
                // An event goes to the panels of its display.
                i32 display = e.display > 0 && engine::Displays::Get(e.display).active ? e.display : 0;
                ofDisplay.Clear();
                for (RuntimePanel* p : order)
                    if (p->display() == display)
                        ofDisplay.Push(p);
                if (ofDisplay.Empty())
                    continue;
                PanelOrder mine(ofDisplay.Data(), ofDisplay.Size());
                for (const InputRoute& r : kInputRoutes)
                    if (r.type == e.event.type)
                    {
                        r.route(mine, e);
                        break;
                    }
            }
            UpdateCursor(panels);
        }

        // The panel a view joins: its parent view's, its settings' (made with the first view), else one of its own
        // (a world space view always has its own: its GameObject places it).
        RuntimePanel* PanelFor(UIView* view)
        {
            Views& v = V();
            if (UIView* parent = view->parentUI())
                if (RuntimePanel* p = parent->runtime())
                    return p;
            UIPanelSettings* settings = view->panelSettings();
            bool world = settings && settings->renderMode() == PanelRenderMode::WorldSpace;
            if (settings && !world)
                for (RuntimePanel* p : v.panels)
                    if (p->Owns(settings))
                        return p;
            auto* p = new RuntimePanel(settings, ++v.serial, world);
            v.panels.Push(p);
            UpdateDrawer();
            return p;
        }

        // The cameras whose rays find the world space panels.
        Array<engine::Camera*> EventCameras()
        {
            PanelInputConfiguration* config = PanelInputConfiguration::current();
            Array<engine::Camera*> out;
            if (config && !config->defaultEventCameraIsMainCamera())
                out = config->eventCameras();
            else if (engine::Camera* main = engine::Camera::main())
                out.Push(main);
            return out;
        }

        void ReleasePanel(RuntimePanel* p)
        {
            Views& v = V();
            for (usize i = 0; i < v.panels.Size(); i++)
                if (v.panels[i] == p)
                {
                    v.panels.RemoveAt(i);
                    break;
                }
            if (v.keyboard == p)
                v.keyboard = nullptr;
            delete p;
            UpdateDrawer();
        }

        void RemoveFromList(UIView* view)
        {
            Views& v = V();
            for (usize i = 0; i < v.list.Size(); i++)
                if (v.list[i] == view)
                {
                    v.list.RemoveAt(i);
                    break;
                }
        }

        // The asset loaders of the markup, the sheets and the fonts are there once the module is loaded.
        struct LoaderRegistrar
        {
            LoaderRegistrar()
            {
                RegisterUIAssetLoaders();
                RegisterFontAssetLoader();
            }
        } registrar;
    }

    // The panel's fonts: a style's font definition (a font asset found like the markup's files), then its families and
    // the platform's default families as fonts of the machine. Paths are found from the views' markup, the first view
    // that has the file wins.
    class RuntimePanel::TextHost final : public PanelTextHost
    {
    public:
        explicit TextHost(RuntimePanel& panel) : panel_(panel) {}

        TextFont* Font(const ComputedStyle& style, Array<TextFont*>& fallbacks) override
        {
            TextFont* primary = nullptr;
            if (style.fontDefinition != kNoAtom)
                if (FontAsset* a = Definition(style.fontDefinition))
                    primary = a->textFont();
            auto add = [&](FontAsset* a) {
                TextFont* f = a ? a->textFont() : nullptr;
                if (!f)
                    return;
                if (!primary)
                    primary = f;
                else if (f != primary)
                    fallbacks.Push(f);
            };
            for (u32 i = 0; i < style.fontFamilyCount; i++)
                add(FontAsset::CreateFontAsset(AtomName(style.fontFamilies[i])));
            for (const char* family : kDefaultFamilies)
                add(FontAsset::CreateFontAsset(family));
            return primary;
        }

        engine::Texture2D* Texture(const TextQuad& quad) override
        {
            FontAsset* a = FontAsset::Of(quad.font);
            return a ? a->AtlasTexture(quad.page, quad.colorGlyph) : nullptr;
        }

        bool Image(Atom path, UIImage& out) override
        {
            engine::Texture* texture = TextureAt(path);
            if (!texture)
                return false;
            Atlas().Place(texture, out);
            return true;
        }

        // The texture of a path of the markup's styles (loaded once; null when there is none).
        engine::Texture* TextureAt(Atom path) override
        {
            // A texture a script set is its object's.
            if (engine::Texture* set = EntityResourceTexture(AtomName(path)))
                return set;
            for (const ImageEntry& e : images_)
                if (e.path == path)
                    return e.texture.Get();
            engine::Texture* texture = nullptr;
            engine::Object* o = Load(path, "texture");
            if (o && o->Is<engine::Texture>())
                texture = static_cast<engine::Texture*>(o);
            images_.Push({path, engine::PPtr<engine::Texture>(texture)});
            return texture;
        }

        void Forget()
        {
            definitions_.Clear();
            images_.Clear();
        }

    private:
        // The asset at a path of the styles, resolved from each view's markup in turn.
        engine::Object* Load(Atom path, const char* what)
        {
            const UITemplate* first = nullptr;
            String firstResolved;
            for (UIView* view : panel_.views())
                if (UITemplate* t = view->source())
                {
                    String resolved = ResolveAssetPath(t->path(), AtomName(path));
                    if (const engine::AssetEntry* e = engine::FindAssetAtPath(resolved.View()))
                        if (engine::Object* o = engine::LoadAsset(e->guid))
                            return o;
                    if (!first)
                        first = t, firstResolved = Move(resolved);
                }
            if (first)
                HX_LOG_WARNING(UILog, "{}: no {} at {}", first->path(), what, firstResolved.View());
            return nullptr;
        }

        FontAsset* Definition(Atom path)
        {
            for (const Entry& e : definitions_)
                if (e.path == path)
                    return e.asset.Get();
            FontAsset* asset = ScriptObjectAs<FontAsset>(Load(path, "font asset"));
            definitions_.Push({path, engine::PPtr<FontAsset>(asset)});
            return asset;
        }

        struct Entry
        {
            Atom path;
            engine::PPtr<FontAsset> asset;
        };
        struct ImageEntry
        {
            Atom path;
            engine::PPtr<engine::Texture> texture;
        };
        RuntimePanel& panel_;
        Array<Entry> definitions_;
        Array<ImageEntry> images_;
    };

    namespace
    {
        engine::Texture2D* CursorTextureOf(RuntimePanel* p, Atom path)
        {
            engine::Texture* t = p->host().TextureAt(path);
            return t && t->Is<engine::Texture2D>() ? static_cast<engine::Texture2D*>(t) : nullptr;
        }
    }

    RuntimePanel::RuntimePanel(UIPanelSettings* settings, u64 serial, bool world)
        : settings_(settings), shared_(settings != nullptr && !world), world_(world), serial_(serial), host_(std::make_unique<TextHost>(*this)),
          panel_(std::make_unique<Panel>())
    {
        panel_->SetTextHost(host_.get());
    }

    RuntimePanel::~RuntimePanel() { panel_.reset(); }

    void RuntimePanel::Add(UIView* view)
    {
        views_.Push(view);
        ElementTree& tree = panel_->tree();
        ElementHandle root = tree.Create(Intern("TemplateContainer"));
        tree.SetPicking(root, PickingMode::Ignore);
        // The view's root fills the panel and the views lie over each other; the first view of a world space panel
        // is its content (the panel may take the size of its elements).
        // A nested view placed relative is in the flow of its parent's panel.
        bool nested = view->parentUI() && view->parentUI()->runtime() == this;
        if ((world_ && views_.Size() == 1) || (nested && view->PositionInternal() == 0))
            panel_->styles().SetInlineStyle(root, "flex-grow: 1");
        else
            panel_->styles().SetInlineStyle(root, "position: absolute; left: 0; top: 0; right: 0; bottom: 0");
        view->root_ = root;
        Place(view);
    }

    void RuntimePanel::Remove(UIView* view)
    {
        for (usize i = 0; i < views_.Size(); i++)
            if (views_[i] == view)
            {
                views_.RemoveAt(i);
                break;
            }
        ElementTree& tree = panel_->tree();
        if (tree.IsAlive(view->root_))
        {
            tree.ForEachInSubtree(view->root_, [&](ElementHandle e, u32) { panel_->world().callbacks().Clear(e); });
            panel_->styles().ClearStyleSheets(view->root_);
            tree.Destroy(view->root_);
        }
        view->root_ = ElementHandle();
        host_->Forget();
    }

    void RuntimePanel::Place(UIView* view)
    {
        ElementTree& tree = panel_->tree();
        if (!tree.IsAlive(view->root_))
            return;
        tree.Detach(view->root_);
        u32 index = 0;
        for (UIView* other : views_)
            if (other != view && tree.Parent(other->root_) == panel_->root() && other->sortingOrder() <= view->sortingOrder())
                index++;
        tree.Insert(panel_->root(), index, view->root_);
    }

    void RuntimePanel::ApplyTheme()
    {
        UIPanelSettings* s = settings();
        ThemeStyleSheet* theme = s ? s->themeStyleSheet() : nullptr;
        std::shared_ptr<const StyleSheetData> sheet = theme ? theme->sheet() : DefaultThemeSheet();
        if (themed_ && sheet.get() == theme_)
            return;
        themed_ = true;
        theme_ = sheet.get();
        panel_->styles().ClearStyleSheets(panel_->root());
        if (sheet)
            panel_->styles().AddStyleSheet(panel_->root(), sheet);
    }

    bool RuntimePanel::TakesInput()
    {
        if (world_)
        {
            PanelInputConfiguration* config = PanelInputConfiguration::current();
            return !config || config->processWorldSpaceInput();
        }
        // The game's screen panels take no pointers of the editor's windows (the Game view gives them theirs).
        if (game() && !target())
            return false;
        UIPanelSettings* s = settings();
        return !target() || (s && s->screenToPanelKind() != 0);
    }

    bool RuntimePanel::ScreenToPanel(Vector2 screen, Vector2& at, f32& distance)
    {
        distance = 0;
        UIPanelSettings* s = settings();
        if (!world_)
        {
            if (s && s->screenToPanelKind() != 0)
            {
                Vector3 r;
                if (!s->ScreenToPanel(screen, r))
                    return false;
                at = Vector2(r.x, r.y);
                distance = s->screenToPanelKind() == 2 ? r.z : 0.0f;
                return true;
            }
            if (target())
                return false;
            at = ToPanel(screen);
            return true;
        }
        // A world space panel: the nearest hit of the event cameras' rays within the distance, on the interaction
        // layers.
        UIView* owner = views_.Empty() ? nullptr : views_[0];
        PanelInputConfiguration* config = PanelInputConfiguration::current();
        i32 layers = config ? config->interactionLayers() : -1;
        f32 limit = config ? config->maxInteractionDistance() : std::numeric_limits<f32>::infinity();
        if (!owner || !owner->gameObject() || !(u32(layers) & (1u << owner->gameObject()->layer())))
            return false;
        Matrix4x4 toPanel = toWorld_.inverse();
        bool found = false;
        f32 best = limit;
        for (engine::Camera* camera : EventCameras())
        {
            Ray ray = camera->ScreenPointToRay(Vector3(screen.x, f32(engine::Screen::height()) - screen.y, 0));
            Vector3 o = toPanel.MultiplyPoint(ray.origin);
            Vector3 e = toPanel.MultiplyPoint(ray.origin + ray.direction);
            Vector3 d(e.x - o.x, e.y - o.y, e.z - o.z);
            if (std::fabs(d.z) < 1e-9f)
                continue;
            f32 t = -o.z / d.z;
            f32 length = std::sqrt(ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y + ray.direction.z * ray.direction.z);
            f32 far = t * length;
            if (t < 0 || far > best)
                continue;
            best = far;
            at = Vector2(o.x + d.x * t, o.y + d.y * t);
            found = true;
        }
        // A hit is never on the screen (distance 0 is the screen's).
        distance = best > 0 ? best : 1e-6f;
        return found;
    }

    void RuntimePanel::UpdateWorld()
    {
        UIView* owner = views_[0];
        bool fixed = owner->worldSpaceSizeMode() == WorldSpaceSizeMode::Fixed;
        panel_->SetSizeToContent(!fixed);
        Vector2 box = fixed ? owner->worldSpaceSize() : Vector2(100000, 100000);
        scale_ = 1;
        panel_->Update(engine::Time::deltaTime(), box.x, box.y);
        size_ = panel_->Size();
        panel_->Draw(uiMesh_);
        Upload();
        // The pivot in the root's box or in its elements' bounds.
        Rect ref(0, 0, size_.x, size_.y);
        if (owner->pivotReferenceSize() == PivotReferenceSize::BoundingBox)
        {
            bool any = false;
            f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            LayoutEngine& layout = panel_->layout();
            panel_->tree().ForEachInSubtree(panel_->root(), [&](ElementHandle e, u32) {
                Rect r = layout.WorldRect(e);
                if (r.width <= 0 && r.height <= 0)
                    return;
                if (!any)
                    x0 = r.x, y0 = r.y, x1 = r.x + r.width, y1 = r.y + r.height, any = true;
                x0 = r.x < x0 ? r.x : x0, y0 = r.y < y0 ? r.y : y0;
                x1 = r.x + r.width > x1 ? r.x + r.width : x1, y1 = r.y + r.height > y1 ? r.y + r.height : y1;
            });
            if (any)
                ref = Rect(x0, y0, x1 - x0, y1 - y0);
        }
        // Column and row of the pivot: 0 left/top, 1 center, 2 right/bottom.
        static constexpr u8 kColumn[] = {1, 0, 1, 2, 0, 2, 0, 1, 2};
        static constexpr u8 kRow[] = {1, 0, 0, 0, 1, 1, 2, 2, 2};
        u32 k = u32(owner->pivot()) < 9 ? u32(owner->pivot()) : 0u;
        Vector2 pivot(ref.x + ref.width * 0.5f * kColumn[k], ref.y + ref.height * 0.5f * kRow[k]);
        engine::Transform* transform = owner->gameObject() ? owner->gameObject()->transform() : nullptr;
        Matrix4x4 local = Matrix4x4::Scale(Vector3(1, -1, 1)) * Matrix4x4::Translate(Vector3(-pivot.x, -pivot.y, 0));
        toWorld_ = transform ? transform->localToWorldMatrix() * local : local;
        // Queued for this frame's cameras, its batches in order (one bounds for all keeps their order).
        engine::Mesh* mesh = upload_.mesh();
        Span<const UIBatch> batches = uiMesh_.Batches();
        if (!mesh || mesh->vertexCount() == 0)
            return;
        engine::RenderParams params;
        params.layer = owner->gameObject() ? i32(owner->gameObject()->layer()) : 0;
        // A pipeline drawing linear colors says so (_HelixLinearOutput); the panel's colors are made linear there.
        f32 linear = engine::Shader::GetGlobalFloat("_HelixLinearOutput") > 0.5f ? 1.0f : 0.0f;
        for (u32 b = 0; b < batches.Size(); b++)
            if (engine::Material* m = MaterialFor(batches[b], true))
            {
                m->SetFloat("_UILinear", linear);
                params.material = m;
                engine::Graphics::RenderMesh(params, mesh, i32(b), toWorld_);
            }
        // The input method's caret where the main camera sees it.
        Rect caret;
        engine::Camera* camera = engine::Camera::main();
        if (camera && panel_->events().TextInput(caret))
        {
            Vector3 w = toWorld_.MultiplyPoint(Vector3(caret.x, caret.y + caret.height, 0));
            Vector3 sp = camera->WorldToScreenPoint(w);
            engine::InputFeed::RequestTextInput(i32(sp.x), i32(f32(engine::Screen::height()) - sp.y), i32(caret.height));
        }
    }

    void RuntimePanel::UpdateTooltip()
    {
        // The element under the mouse (or an ancestor) with a tooltip; shown after half a second at rest.
        constexpr f64 kDelay = 0.5;
        ElementTree& tree = panel_->tree();
        EventSystem& events = panel_->events();
        ElementHandle over;
        for (ElementHandle e = events.ElementUnderPointer(PointerId::mouse); e.IsValid() && !over.IsValid(); e = tree.Parent(e))
            if (Element* x = Element::Of(panel_->world(), e); x && !x->tooltip().Empty())
                over = e;
        Vector2 at = events.PointerPosition(PointerId::mouse);
        f64 now = engine::Time::realtimeSinceStartupAsDouble();
        bool moved = std::fabs(at.x - tipAt_.x) + std::fabs(at.y - tipAt_.y) > 3;
        if (over != tipFor_ || (moved && !tree.IsAlive(tipLabel_)))
        {
            tipFor_ = over;
            tipSince_ = now;
            tipAt_ = at;
            if (tree.IsAlive(tipLabel_))
            {
                tree.Destroy(tipLabel_);
                tipLabel_ = ElementHandle();
            }
        }
        if (!over.IsValid() || tree.IsAlive(tipLabel_) || now - tipSince_ < kDelay)
            return;
        // The element hears of it first (TooltipEvent, its box in newRect).
        UIEvent e;
        e.type = RegisterEventType("TooltipEvent", true, true, EventFamily::Other);
        e.target = over;
        e.newRect = panel_->layout().WorldRect(over);
        e.timestamp = now;
        events.SendEvent(e);
        Element* x = Element::Of(panel_->world(), over);
        if (!x || x->tooltip().Empty())
            return;
        tipLabel_ = MakePart(panel_->root(), "helix-text-element helix-label helix-tooltip", "Label");
        tree.SetPicking(tipLabel_, PickingMode::Ignore);
        tree.SetText(tipLabel_, x->tooltip().View());
        SetPartStyle(tipLabel_, "position", "absolute");
        SetPartStyle(tipLabel_, "left", Format("{}px", at.x + 4).View());
        SetPartStyle(tipLabel_, "top", Format("{}px", at.y + 20).View());
    }

    void RuntimePanel::Update()
    {
        ApplyTheme();
        if (world_)
        {
            UpdateWorld();
            return;
        }
        UpdateTooltip();
        UIPanelSettings* s = settings();
        engine::RenderTexture* rt = target();
        // A texture's size, else its display's (the main one: the screen's).
        const engine::DisplayState& shown = engine::Displays::Get(display());
        f32 w = rt ? f32(rt->width()) : display() > 0 ? f32(shown.systemWidth) : f32(engine::Screen::width());
        f32 h = rt ? f32(rt->height()) : display() > 0 ? f32(shown.systemHeight) : f32(engine::Screen::height());
        // The game's: its display texture's size in the editor.
        if (!rt && game())
        {
            Vector2 content = engine::Graphics::ContentDisplaySize(owner(), display());
            w = content.x;
            h = content.y;
        }
        scale_ = s ? s->ResolveScale(w, h, engine::Screen::dpi()) : 1.0f;
        if (!(scale_ > 0))
            scale_ = 1.0f;
        size_ = Vector2(w / scale_, h / scale_);
        panel_->Update(engine::Time::deltaTime(), size_.x, size_.y);
        panel_->Draw(uiMesh_);
        // The input method stays on while a text element of a screen panel has the focus.
        Rect caret;
        if (!rt && panel_->events().TextInput(caret))
            engine::InputFeed::RequestTextInput(i32(caret.x * scale_), i32(caret.y * scale_), i32(caret.height * scale_));
        Upload();
    }

    void RuntimePanel::Upload() { upload_.Upload(uiMesh_); }

    // ------------------------------------------------------------------------------------------------------- overlay

    namespace overlay
    {
        ImageAtlas& SharedAtlas() { return Atlas(); }

        TextFont* DefaultFont(Array<TextFont*>& fallbacks)
        {
            TextFont* primary = nullptr;
            for (const char* family : kDefaultFamilies)
                if (FontAsset* a = FontAsset::CreateFontAsset(family))
                    if (TextFont* f = a->textFont())
                    {
                        if (!primary)
                            primary = f;
                        else if (f != primary)
                            fallbacks.Push(f);
                    }
            return primary;
        }

        engine::Texture2D* GlyphTexture(const TextQuad& quad)
        {
            FontAsset* a = FontAsset::Of(quad.font);
            return a ? a->AtlasTexture(quad.page, quad.colorGlyph) : nullptr;
        }

        engine::Material* MaterialFor(const UIBatch& batch, bool world) { return ui::MaterialFor(batch, world); }

        void SetGuiDrawer(void (*draw)(engine::CommandBuffer& cmd))
        {
            V().gui = draw;
            UpdateDrawer();
        }

        MeshUpload::~MeshUpload()
        {
            if (engine::Mesh* m = mesh_.Get())
                engine::Object::DestroyImmediate(m);
        }

        void MeshUpload::Upload(const UIMesh& source)
        {
            engine::Mesh* m = mesh_.Get();
            if (!m)
            {
                m = engine::Mesh::Create();
                m->hideFlags = engine::HideFlags::DontSave;
                m->MarkDynamic();
                mesh_ = engine::PPtr<engine::Mesh>(m);
                // A new mesh (the last one went with the objects) has no buffers yet.
                vertexCapacity_ = indexCapacity_ = 0;
            }
            // The vertices go in as they are, one interleaved stream in UIVertex's layout (position with its depth, color,
            // six channels), the indices as 32 bit ones: two copies, no conversion. The buffers only grow; the batches
            // name the ranges in use.
            static const engine::VertexAttributeDescriptor kLayout[] = {
                {engine::VertexAttribute::Position, engine::VertexAttributeFormat::Float32, 3},
                {engine::VertexAttribute::Color, engine::VertexAttributeFormat::Float32, 4},
                {engine::VertexAttribute::TexCoord0, engine::VertexAttributeFormat::Float32, 4},
                {engine::VertexAttribute::TexCoord1, engine::VertexAttributeFormat::Float32, 4},
                {engine::VertexAttribute::TexCoord2, engine::VertexAttributeFormat::Float32, 4},
                {engine::VertexAttribute::TexCoord3, engine::VertexAttributeFormat::Float32, 4},
                {engine::VertexAttribute::TexCoord4, engine::VertexAttributeFormat::Float32, 4},
                {engine::VertexAttribute::TexCoord5, engine::VertexAttributeFormat::Float32, 4},
            };
            static_assert(sizeof(UIVertex) == 31 * sizeof(f32), "UIVertex is the mesh's interleaved layout");
            Span<const UIVertex> vertices = source.Vertices();
            Span<const u32> indices = source.Indices();
            Span<const UIBatch> batches = source.Batches();
            u32 n = u32(vertices.Size()), k = u32(indices.Size());
            // The panel is flat in its space (a 3D turn of an element is in its x and y): the depth goes in as zero.
            vertices_.Resize(n);
            if (n)
                std::memcpy(vertices_.Data(), vertices.Data(), usize(n) * sizeof(UIVertex));
            Vector3 lo(0, 0, 0), hi(0, 0, 0);
            for (u32 i = 0; i < n; i++)
            {
                UIVertex& v = vertices_[i];
                v.z = 0;
                if (i == 0)
                    lo = hi = Vector3(v.position.x, v.position.y, 0);
                lo.x = v.position.x < lo.x ? v.position.x : lo.x;
                lo.y = v.position.y < lo.y ? v.position.y : lo.y;
                hi.x = v.position.x > hi.x ? v.position.x : hi.x;
                hi.y = v.position.y > hi.y ? v.position.y : hi.y;
            }
            if (n > vertexCapacity_ || vertexCapacity_ == 0)
            {
                vertexCapacity_ = n > 1024 ? n + n / 2 : 1024;
                m->SetVertexBufferParams(i32(vertexCapacity_), Span<const engine::VertexAttributeDescriptor>(kLayout, 8));
            }
            if (k > indexCapacity_ || indexCapacity_ == 0)
            {
                indexCapacity_ = k > 4096 ? k + k / 2 : 4096;
                m->SetIndexBufferParams(i32(indexCapacity_), engine::IndexFormat::UInt32);
            }
            constexpr engine::MeshUpdateFlags kFlags = engine::MeshUpdateFlags::DontValidateIndices | engine::MeshUpdateFlags::DontRecalculateBounds;
            m->SetVertexBufferData(Span<const UIVertex>(vertices_.Data(), n), 0, 0, i32(n), 0, kFlags);
            m->SetIndexBufferData(indices, 0, 0, i32(k), kFlags);
            Bounds bounds(Vector3((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, 0), Vector3(hi.x - lo.x, hi.y - lo.y, 0));
            subMeshes_.Clear();
            for (const UIBatch& batch : batches)
            {
                engine::SubMeshDescriptor d;
                d.indexStart = i32(batch.firstIndex);
                d.indexCount = i32(batch.indexCount);
                d.topology = engine::MeshTopology::Triangles;
                d.bounds = bounds;
                d.firstVertex = 0;
                d.vertexCount = i32(n);
                subMeshes_.Push(d);
            }
            if (subMeshes_.Empty())
                subMeshes_.Push(engine::SubMeshDescriptor());
            m->SetSubMeshes(Span<const engine::SubMeshDescriptor>(subMeshes_.Data(), subMeshes_.Size()), kFlags);
            m->SetBounds(bounds);
            Atlas().Upload();
        }

        void MeshUpload::Draw(const UIMesh& source, engine::CommandBuffer& cmd) const
        {
            engine::Mesh* mesh = mesh_.Get();
            if (!mesh)
                return;
            Span<const UIBatch> batches = source.Batches();
            for (u32 b = 0; b < batches.Size(); b++)
                if (engine::Material* m = ui::MaterialFor(batches[b]))
                    cmd.DrawMesh(mesh, Matrix4x4::identity, m, i32(b));
        }
    }

    // ------------------------------------------------------------------------------------------------------- UIView

    engine::Texture2D* UIView::CursorTexture(Atom path)
    {
        engine::Texture* t = runtime_ ? runtime_->host().TextureAt(path) : nullptr;
        return t && t->Is<engine::Texture2D>() ? static_cast<engine::Texture2D*>(t) : nullptr;
    }

    UIView::UIView(const UIView& other)
        : engine::GameBehaviour(other), settings_(other.settings_), source_(other.source_), sortingOrder_(other.sortingOrder_), watchFiles_(other.watchFiles_)
    {
    }

    UIView::~UIView()
    {
        RemoveFromList(this);
        Detach();
    }

    void UIView::SetSource(UITemplate* value)
    {
        source_ = engine::PPtr<UITemplate>(value);
        // Built at once while enabled (scripts find its elements right after).
        if (runtime_)
            Build();
        else
            built_ = false;
    }

    UIView* UIView::parentUI() const
    {
        engine::GameObject* g = gameObject();
        engine::Transform* t = g ? g->transform() : nullptr;
        for (engine::Transform* up = t ? t->parent() : nullptr; up; up = up->parent())
            for (UIView* view : V().list)
                if (view != this && view->gameObject() == up->gameObject())
                    return view;
        return nullptr;
    }

    void UIView::SetPositionInternal(i32 value)
    {
        position_ = value == 0 ? 0 : 1;
        // Placed again in its panel.
        if (runtime_)
        {
            Detach();
            Attach();
            Build();
        }
    }

    Matrix4x4 UIView::PanelToWorld() const { return runtime_ && runtime_->world() ? runtime_->toWorld() : Matrix4x4::identity; }

    void UIView::SetPanelSettings(UIPanelSettings* value)
    {
        if (value == panelSettings())
            return;
        settings_ = engine::PPtr<engine::ScriptableObject>(value);
        // An enabled view moves to the settings' panel and is built there.
        if (runtime_)
        {
            Detach();
            Attach();
            Build();
        }
    }

    void UIView::SetSortingOrder(f32 value)
    {
        sortingOrder_ = value;
        if (runtime_)
            runtime_->Place(this);
    }

    Panel* UIView::panel() const { return runtime_ ? &runtime_->panel() : nullptr; }

    Element* UIView::rootElement() const { return runtime_ ? Element::Of(runtime_->panel().world(), root_) : nullptr; }

    UIView* UIView::FindUIView(Element* element)
    {
        if (!element)
            return nullptr;
        ElementTree& tree = element->world().tree();
        for (ElementHandle e = element->handle(); e.IsValid(); e = tree.Parent(e))
            for (UIView* view : V().list)
                if (view->runtime_ && view->root_ == e && &view->runtime_->panel().world() == &element->world())
                    return view;
        return nullptr;
    }

    ElementHandle UIView::Pick(Vector2 screenPosition) const
    {
        Vector2 at;
        f32 distance = 0;
        if (!runtime_ || !runtime_->ScreenToPanel(screenPosition, at, distance))
            return ElementHandle();
        return runtime_->panel().events().Pick(at);
    }

    Span<UIView* const> UIView::Views() { return Span<UIView* const>(V().list.Data(), V().list.Size()); }

    engine::Mesh* UIView::mesh() const { return runtime_ ? runtime_->mesh() : nullptr; }

    const UIMesh& UIView::uiMesh() const
    {
        static const UIMesh empty;
        return runtime_ ? runtime_->uiMesh() : empty;
    }

    void UIView::Attach()
    {
        if (runtime_)
            return;
        runtime_ = PanelFor(this);
        runtime_->Add(this);
    }

    void UIView::Detach()
    {
        if (!runtime_)
            return;
        RuntimePanel* p = runtime_;
        p->Remove(this);
        runtime_ = nullptr;
        if (p->views().Empty())
            ReleasePanel(p);
    }

    void UIView::OnEnable()
    {
        Attach();
        V().list.Push(this);
        // The elements are there when it is enabled: other scripts' Start finds them.
        Build();
    }

    void UIView::OnDisable()
    {
        RemoveFromList(this);
        Detach();
        watched_.Clear();
    }

    void UIView::Build()
    {
        built_ = true;
        errors_.Clear();
        watched_.Clear();
        runtime_->host().Forget();
        Panel& panel = runtime_->panel();
        UITemplate* t = source_.Get();
        if (!t)
        {
            Array<HxmlError> none;
            panel.BuildInto(root_, HxmlDocument(), nullptr, none);
            return;
        }
        Array<String> read;
        read.Push(String(t->path()));
        AssetHxmlResolver resolver(t->path(), &read);
        panel.BuildInto(root_, t->document(), &resolver, errors_);
        for (const HxmlError& e : errors_)
            HX_LOG_WARNING(UILog, "{}:{}:{}: {}", t->path(), e.line, e.column, e.message.View());
        // The bytes as they are now, so a change shows at the next look.
        for (const String& path : read)
        {
            Watched w;
            w.path = path;
            ReadSource(path.View(), w.bytes);
            watched_.Push(Move(w));
        }
    }

    bool UIView::SourcesChanged()
    {
        bool changed = false;
        for (Watched& w : watched_)
        {
            Array<u8> now;
            if (!ReadSource(w.path.View(), now) || (now.Size() == w.bytes.Size() && (now.Empty() || std::memcmp(now.Data(), w.bytes.Data(), now.Size()) == 0)))
                continue;
            w.bytes = Move(now);
            changed = true;
            HX_LOG_INFO(UILog, "{} changed: the view is built again.", w.path.View());
            // The asset of the file takes the new text.
            const engine::AssetEntry* e = engine::FindAssetAtPath(w.path.View());
            engine::Object* o = e && engine::IsAssetLoaded(e->guid) ? engine::LoadAsset(e->guid) : nullptr;
            StringView text(reinterpret_cast<const char*>(w.bytes.Data()), w.bytes.Size());
            if (auto* t = ScriptObjectAs<UITemplate>(o))
            {
                Array<HxmlError> errors;
                if (!t->SetText(w.path.View(), text, errors))
                    for (const HxmlError& err : errors)
                        HX_LOG_WARNING(UILog, "{}:{}:{}: {}", w.path.View(), err.line, err.column, err.message.View());
            }
            else if (auto* s = ScriptObjectAs<StyleSheet>(o))
            {
                s->SetText(w.path.View(), text);
                for (const StyleError& err : s->sheet()->Errors())
                    HX_LOG_WARNING(UILog, "{}:{}:{}: {}", w.path.View(), err.line, err.column, err.message.View());
            }
        }
        return changed;
    }

    void UIView::Update()
    {
        if (!runtime_)
            return;
        if (watchFiles_)
        {
            // The wall clock: files change while the game is paused too.
            f64 now = platform::Now();
            if (now - lastWatch_ >= kWatchInterval)
            {
                lastWatch_ = now;
                if (SourcesChanged())
                    built_ = false;
            }
        }
        if (!built_)
            Build();
        // The input on what was shown, then the panel as it is now (its first view updates it, once a frame).
        ReadInput();
        if (runtime_->views()[0] == this)
            runtime_->Update();
    }

    f32 UIView::PanelScaleInternal(Element* panelRoot)
    {
        if (!panelRoot)
            return 1.0f;
        for (RuntimePanel* p : V().panels)
            if (&p->panel().world() == &panelRoot->world() && p->panel().root() == panelRoot->handle())
                return p->scale();
        return 1.0f;
    }

    // ------------------------------------------------------------------------------------------------- PanelInput

    namespace
    {
        RuntimePanel* PanelById(u64 id)
        {
            for (RuntimePanel* p : V().panels)
                if (p->serial() == id)
                    return p;
            return nullptr;
        }
    }

    void PanelInput::SetEventSystemPresent(bool value) { V().eventSystem = value; }

    bool PanelInput::ReadsOwnInput()
    {
        PanelInputConfiguration* config = PanelInputConfiguration::current();
        i32 mode = config ? config->PanelInputRedirectionInternal() : 0;
        // AutoRedirect, NoRedirect, AlwaysRedirect.
        if (mode == 1)
            return true;
        if (mode == 2)
            return false;
        return !V().eventSystem;
    }

    Array<u64> PanelInput::Panels()
    {
        Array<u64> out;
        for (RuntimePanel* p : InputOrder())
            out.Push(p->serial());
        return out;
    }

    bool PanelInput::Alive(u64 panel) { return PanelById(panel) != nullptr; }

    String PanelInput::Name(u64 panel)
    {
        RuntimePanel* p = PanelById(panel);
        if (!p)
            return String();
        if (UIPanelSettings* s = p->settings())
            return String(s->Name());
        Span<UIView* const> views = p->views();
        return views.Empty() ? String("Panel") : String(views[0]->gameObject()->Name());
    }

    f32 PanelInput::SortingOrder(u64 panel)
    {
        RuntimePanel* p = PanelById(panel);
        return p ? p->order() : 0.0f;
    }

    i32 PanelInput::InputIndex(u64 panel)
    {
        Array<RuntimePanel*> order = InputOrder();
        for (usize i = 0; i < order.Size(); i++)
            if (order[i]->serial() == panel)
                return i32(i);
        return -1;
    }

    bool PanelInput::IsWorld(u64 panel)
    {
        RuntimePanel* p = PanelById(panel);
        return p && p->world();
    }

    bool PanelInput::ScreenToPanel(u64 panel, Vector2 screen, Vector2& at, f32& distance)
    {
        RuntimePanel* p = PanelById(panel);
        return p && p->TakesInput() && p->ScreenToPanel(screen, at, distance);
    }

    bool PanelInput::Pick(u64 panel, Vector2 at)
    {
        RuntimePanel* p = PanelById(panel);
        return p && p->panel().events().Pick(at).IsValid();
    }

    EventSystem* PanelInput::Events(u64 panel)
    {
        RuntimePanel* p = PanelById(panel);
        return p ? &p->panel().events() : nullptr;
    }

    UIView* PanelInput::FirstView(u64 panel)
    {
        RuntimePanel* p = PanelById(panel);
        return p && !p->views().Empty() ? p->views()[0] : nullptr;
    }

    void PanelInput::SetKeyboardPanel(u64 panel) { V().keyboard = PanelById(panel); }
}
