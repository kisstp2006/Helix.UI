#include <Helix/UI/Engine/UIView.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Pipeline/CommandBuffer.h>
#include <Helix/Engine/Rendering/Pipeline/RenderPipeline.h>
#include <Helix/Engine/Rendering/Pipeline/ScriptableRenderContext.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Platform/Time/Time.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Engine/FontAsset.h>
#include <Helix/UI/Engine/ImageAtlas.h>

#include <cstring>
#include <utility>

namespace helix::ui
{
    namespace
    {
        LogChannel UILog{"UI"};

        // How often watched files are read again (seconds).
        constexpr f64 kWatchInterval = 0.5;

        // The fonts every view falls back to after its styles' fonts: families of the platform (the first one the
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
            const void* textures[UIBatch::kSlots] = {};
            engine::PPtr<engine::Texture2D> alive[UIBatch::kSlots];
            engine::PPtr<engine::Material> material;
        };

        struct Views
        {
            Array<UIView*> list;
            u32 drawer = 0;
            // The input frame the views read last, and the view the keys go to (the last one pressed in).
            u64 inputFrame = 0;
            UIView* keyboard = nullptr;
            // One material per set of textures in the slots.
            Array<MaterialEntry> materials;
            bool missingShader = false;
            ImageAtlas* atlas = nullptr;
        };

        Views& V()
        {
            static Views* v = new Views();
            return *v;
        }

        engine::Material* MaterialFor(const UIBatch& batch)
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
                bool same = true;
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
            for (u32 k = 0; k < UIBatch::kSlots; k++)
            {
                auto* t = static_cast<engine::Texture2D*>(const_cast<void*>(batch.textures[k]));
                entry.textures[k] = batch.textures[k];
                entry.alive[k] = engine::PPtr<engine::Texture2D>(t);
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

        // The folder part of a path (with its slash), empty for a file at the root.
        StringView FolderOf(StringView path)
        {
            for (usize i = path.Size(); i-- > 0;)
                if (path[i] == '/')
                    return path.Substr(0, i + 1);
            return StringView();
        }

        // A src of the markup: relative to the markup's folder, or from the source's root when it starts with '/'.
        String Resolve(StringView base, StringView src)
        {
            if (src.StartsWith("/"))
                return String(src.Substr(1));
            String out(FolderOf(base));
            while (src.StartsWith("../"))
            {
                src = src.Substr(3);
                StringView parent = FolderOf(out.View().Substr(0, out.Size() > 0 ? out.Size() - 1 : 0));
                out = String(parent);
            }
            out += src;
            return out;
        }

        bool ReadSource(StringView path, Array<u8>& out)
        {
            const engine::AssetEntry* e = engine::FindAssetAtPath(path);
            String error;
            return e && e->source && e->source->Read(e->path.View(), out, error);
        }

        // Templates and sheets of the markup from the asset catalog; what it read is kept for watchFiles.
        class AssetResolver final : public HxmlResolver
        {
        public:
            AssetResolver(StringView base, Array<String>& read) : base_(base), read_(read) {}

            const HxmlDocument* Template(StringView src) override
            {
                String path = Resolve(base_, src);
                read_.Push(path);
                const engine::AssetEntry* e = engine::FindAssetAtPath(path.View());
                auto* t = e ? ScriptObjectAs<UITemplate>(engine::LoadAsset(e->guid)) : nullptr;
                return t ? &t->document() : nullptr;
            }

            std::shared_ptr<const StyleSheet> Style(StringView src) override
            {
                String path = Resolve(base_, src);
                read_.Push(path);
                const engine::AssetEntry* e = engine::FindAssetAtPath(path.View());
                auto* s = e ? ScriptObjectAs<StyleSheetAsset>(engine::LoadAsset(e->guid)) : nullptr;
                return s ? s->sheet() : nullptr;
            }

        private:
            StringView base_;
            Array<String>& read_;
        };

        // The views over the screen, once a frame after the cameras: pixels from the top left mapped to the target.
        void Draw(void*, engine::CommandBuffer& cmd)
        {
            Views& v = V();
            if (v.list.Empty())
                return;
            const engine::RenderTarget& target = engine::Graphics::Target();
            f32 w = f32(engine::Screen::width()), h = f32(engine::Screen::height());
            if (w <= 0 || h <= 0)
                return;
            cmd.SetViewport(Rect(0, 0, f32(target.width), f32(target.height)));
            cmd.SetViewProjectionMatrices(Matrix4x4::identity, Matrix4x4::Ortho(0, w, h, 0, -1, 1));
            // By sorting order, the order of enabling among equal ones.
            Array<UIView*> order(v.list);
            for (usize i = 1; i < order.Size(); i++)
                for (usize j = i; j > 0 && order[j - 1]->sortingOrder() > order[j]->sortingOrder(); j--)
                    std::swap(order[j - 1], order[j]);
            for (UIView* view : order)
            {
                engine::Mesh* mesh = view->mesh();
                if (!mesh || mesh->vertexCount() == 0)
                    continue;
                Span<const UIBatch> batches = view->uiMesh().Batches();
                for (u32 b = 0; b < batches.Size(); b++)
                    if (engine::Material* m = MaterialFor(batches[b]))
                        cmd.DrawMesh(mesh, Matrix4x4::identity, m, i32(b));
            }
        }

        // ----------------------------------------------------------------------------------------------- input

        // The views top-most first: by sorting order, the later enabled over the earlier among equal ones.
        Array<UIView*> InputOrder()
        {
            Array<UIView*> order;
            for (UIView* view : V().list)
                if (view->panel())
                {
                    usize at = 0;
                    while (at < order.Size() && order[at]->sortingOrder() > view->sortingOrder())
                        at++;
                    order.Insert(at, view);
                }
            return order;
        }

        // A pointer's input to every view, the top-most first: the first one with an element under the point has it,
        // the views below are covered there (their capture still gets it).
        template <class F>
        void ToPointer(Span<UIView* const> order, Vector2 position, bool press, F&& send)
        {
            bool covered = false;
            for (UIView* view : order)
            {
                EventSystem& events = view->panel()->events();
                bool hit = !covered && events.Pick(position).IsValid();
                send(events, covered);
                if (hit)
                {
                    covered = true;
                    if (press)
                        V().keyboard = view;
                }
            }
        }

        // The view keys go to: the last one pressed in while it is there, else the top-most.
        EventSystem* KeyboardView(Span<UIView* const> order)
        {
            for (UIView* view : order)
                if (view == V().keyboard)
                    return &view->panel()->events();
            return order.Empty() ? nullptr : &order[0]->panel()->events();
        }

        constexpr i32 TouchId(i32 platformId) { return PointerId::touchBase + i32(u32(platformId) % u32(PointerId::touchCount)); }

        Vector2 Where(const platform::MouseData& m) { return Vector2(m.x, m.y); }

        // How each kind of the window's input reaches the views.
        struct InputRoute
        {
            platform::EventType type;
            void (*route)(Span<UIView* const> order, const engine::InputEvent& e);
        };

        const InputRoute kInputRoutes[] = {
            {platform::EventType::MouseMove,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 ToPointer(order, Where(e.event.mouse), false, [&](EventSystem& events, bool covered) {
                     events.PointerMove(PointerId::mouse, PointerKind::Mouse, Where(e.event.mouse), e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::MouseDown,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 ToPointer(order, Where(e.event.mouse), true, [&](EventSystem& events, bool covered) {
                     events.PointerDown(PointerId::mouse, PointerKind::Mouse, i32(e.event.mouse.button), Where(e.event.mouse), e.event.modifiers, e.time,
                                        covered);
                 });
             }},
            {platform::EventType::MouseUp,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 ToPointer(order, Where(e.event.mouse), false, [&](EventSystem& events, bool covered) {
                     events.PointerUp(PointerId::mouse, PointerKind::Mouse, i32(e.event.mouse.button), Where(e.event.mouse), e.event.modifiers, e.time,
                                      covered);
                 });
             }},
            {platform::EventType::MouseWheel,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 for (UIView* view : order)
                 {
                     EventSystem& events = view->panel()->events();
                     Vector2 at = events.PointerPosition(PointerId::mouse);
                     bool hit = events.Pick(at).IsValid();
                     events.Wheel(at, Vector2(e.event.wheel.dx, e.event.wheel.dy), e.event.modifiers, e.time);
                     if (hit)
                         break;
                 }
             }},
            {platform::EventType::TouchBegin,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 Vector2 at(e.event.touch.x, e.event.touch.y);
                 ToPointer(order, at, true, [&](EventSystem& events, bool covered) {
                     events.PointerDown(TouchId(e.event.touch.id), PointerKind::Touch, 0, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::TouchMove,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 Vector2 at(e.event.touch.x, e.event.touch.y);
                 ToPointer(order, at, false, [&](EventSystem& events, bool covered) {
                     events.PointerMove(TouchId(e.event.touch.id), PointerKind::Touch, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::TouchEnd,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 Vector2 at(e.event.touch.x, e.event.touch.y);
                 ToPointer(order, at, false, [&](EventSystem& events, bool covered) {
                     events.PointerUp(TouchId(e.event.touch.id), PointerKind::Touch, 0, at, e.event.modifiers, e.time, covered);
                 });
             }},
            {platform::EventType::TouchCancel,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 for (UIView* view : order)
                     view->panel()->events().PointerCancel(TouchId(e.event.touch.id), e.time);
             }},
            {platform::EventType::KeyDown,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 if (EventSystem* events = KeyboardView(order))
                     events->KeyDown(u16(e.event.key.key), e.event.modifiers, e.time);
             }},
            {platform::EventType::KeyUp,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 if (EventSystem* events = KeyboardView(order))
                     events->KeyUp(u16(e.event.key.key), e.event.modifiers, e.time);
             }},
            {platform::EventType::Text,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 if (EventSystem* events = KeyboardView(order))
                     events->Character(e.event.text.codepoint, e.event.modifiers, e.time);
             }},
            {platform::EventType::Composition,
             [](Span<UIView* const> order, const engine::InputEvent& e) {
                 // The feed holds the composition as of the frame's last change.
                 if (EventSystem* events = KeyboardView(order))
                     events->SetComposition(engine::InputFeed::Composition(), e.event.composition.cursor, e.time);
             }},
        };

        // The frame's input to the views, once a frame (by the first view that updates).
        void ReadInput()
        {
            Views& v = V();
            u64 frame = engine::InputFeed::FrameNumber();
            if (frame == v.inputFrame)
                return;
            v.inputFrame = frame;
            // The scripts' scheduled items, once a frame.
            Element::TickScripts(engine::Time::realtimeSinceStartupAsDouble());
            Array<UIView*> order = InputOrder();
            if (order.Empty())
                return;
            Span<UIView* const> views(order.Data(), order.Size());
            for (const engine::InputEvent& e : engine::InputFeed::Frame())
                for (const InputRoute& r : kInputRoutes)
                    if (r.type == e.event.type)
                    {
                        r.route(views, e);
                        break;
                    }
        }

        void AddView(UIView* view)
        {
            Views& v = V();
            if (v.list.Empty())
                v.drawer = engine::Graphics::screenOverlayRendering().Add(&Draw);
            v.list.Push(view);
        }

        void RemoveView(UIView* view)
        {
            Views& v = V();
            for (usize i = 0; i < v.list.Size(); i++)
                if (v.list[i] == view)
                {
                    v.list.RemoveAt(i);
                    break;
                }
            if (v.keyboard == view)
                v.keyboard = nullptr;
            if (v.list.Empty() && v.drawer)
            {
                engine::Graphics::screenOverlayRendering().Remove(v.drawer);
                v.drawer = 0;
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

    // The view's fonts: a style's font definition (a font asset found like the markup's files), then its families and
    // the platform's default families as fonts of the machine.
    class UIView::TextHost final : public PanelTextHost
    {
    public:
        explicit TextHost(UIView& view) : view_(view) {}

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
            engine::Texture2D* texture = nullptr;
            bool known = false;
            for (const ImageEntry& e : images_)
                if (e.path == path)
                {
                    texture = e.texture.Get();
                    known = true;
                }
            if (!known)
            {
                if (UITemplate* t = view_.source())
                {
                    String resolved = Resolve(t->path(), AtomName(path));
                    if (const engine::AssetEntry* e = engine::FindAssetAtPath(resolved.View()))
                    {
                        engine::Object* o = engine::LoadAsset(e->guid);
                        if (o && o->Is<engine::Texture2D>())
                            texture = static_cast<engine::Texture2D*>(o);
                    }
                    if (!texture)
                        HX_LOG_WARNING(UILog, "{}: no texture at {}", t->path(), resolved.View());
                }
                images_.Push({path, engine::PPtr<engine::Texture2D>(texture)});
            }
            if (!texture)
                return false;
            Atlas().Place(texture, out);
            return true;
        }

        void Forget()
        {
            definitions_.Clear();
            images_.Clear();
        }

    private:
        FontAsset* Definition(Atom path)
        {
            for (const Entry& e : definitions_)
                if (e.path == path)
                    return e.asset.Get();
            FontAsset* asset = nullptr;
            if (UITemplate* t = view_.source())
            {
                String resolved = Resolve(t->path(), AtomName(path));
                if (const engine::AssetEntry* e = engine::FindAssetAtPath(resolved.View()))
                    asset = ScriptObjectAs<FontAsset>(engine::LoadAsset(e->guid));
                if (!asset)
                    HX_LOG_WARNING(UILog, "{}: no font asset at {}", t->path(), resolved.View());
            }
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
            engine::PPtr<engine::Texture2D> texture;
        };
        UIView& view_;
        Array<Entry> definitions_;
        Array<ImageEntry> images_;
    };

    UIView::UIView(const UIView& other)
        : engine::GameBehaviour(other), source_(other.source_), sortingOrder_(other.sortingOrder_), watchFiles_(other.watchFiles_)
    {
    }

    UIView::~UIView()
    {
        RemoveView(this);
        if (engine::Mesh* m = mesh_.Get())
            engine::Object::DestroyImmediate(m);
    }

    void UIView::SetSource(UITemplate* value)
    {
        source_ = engine::PPtr<UITemplate>(value);
        built_ = false;
    }

    Span<UIView* const> UIView::Views() { return Span<UIView* const>(V().list.Data(), V().list.Size()); }

    engine::Mesh* UIView::mesh() const { return mesh_.Get(); }

    void UIView::OnEnable()
    {
        host_ = std::make_unique<TextHost>(*this);
        panel_ = std::make_unique<Panel>();
        panel_->SetTextHost(host_.get());
        built_ = false;
        AddView(this);
    }

    void UIView::OnDisable()
    {
        RemoveView(this);
        panel_.reset();
        host_.reset();
        watched_.Clear();
        uiMesh_.Clear();
    }

    void UIView::Build()
    {
        built_ = true;
        errors_.Clear();
        watched_.Clear();
        static_cast<TextHost*>(host_.get())->Forget();
        UITemplate* t = source_.Get();
        if (!t)
        {
            Array<HxmlError> none;
            panel_->Build(HxmlDocument(), nullptr, none);
            return;
        }
        Array<String> read;
        read.Push(String(t->path()));
        AssetResolver resolver(t->path(), read);
        panel_->Build(t->document(), &resolver, errors_);
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
            else if (auto* s = ScriptObjectAs<StyleSheetAsset>(o))
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
        if (!panel_)
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
        // The input on what was shown, then the panel as it is now.
        ReadInput();
        panel_->Update(engine::Time::deltaTime(), f32(engine::Screen::width()), f32(engine::Screen::height()));
        panel_->Draw(uiMesh_);
        // The input method stays on while a text element of this view has the focus.
        Rect caret;
        if (panel_->events().TextInput(caret))
            engine::InputFeed::RequestTextInput(i32(caret.x), i32(caret.y), i32(caret.height));

        engine::Mesh* m = mesh_.Get();
        if (!m)
        {
            m = engine::Mesh::Create();
            m->hideFlags = engine::HideFlags::DontSave;
            m->MarkDynamic();
            mesh_ = engine::PPtr<engine::Mesh>(m);
        }
        Span<const UIVertex> vertices = uiMesh_.Vertices();
        usize n = vertices.Size();
        Array<Vector3> positions;
        Array<Color> colors;
        Array<Vector4> channels[6];
        positions.Reserve(n);
        colors.Reserve(n);
        for (Array<Vector4>& c : channels)
            c.Reserve(n);
        for (const UIVertex& v : vertices)
        {
            positions.Push(Vector3(v.position.x, v.position.y, 0));
            colors.Push(v.color);
            channels[0].Push(v.uvLocal);
            channels[1].Push(v.parameters);
            channels[2].Push(v.size);
            channels[3].Push(v.radii);
            channels[4].Push(v.borders);
            channels[5].Push(v.clipRect);
        }
        m->Clear();
        m->SetVertices(Span<const Vector3>(positions.Data(), positions.Size()));
        m->SetColors(Span<const Color>(colors.Data(), colors.Size()));
        for (i32 c = 0; c < 6; c++)
            m->SetUVs(c, Span<const Vector4>(channels[c].Data(), channels[c].Size()));
        Span<const UIBatch> batches = uiMesh_.Batches();
        Span<const u32> indices = uiMesh_.Indices();
        m->SetSubMeshCount(i32(batches.Size() > 0 ? batches.Size() : 1));
        for (u32 b = 0; b < batches.Size(); b++)
            m->SetTriangles(Span<const i32>(reinterpret_cast<const i32*>(indices.Data()) + batches[b].firstIndex, batches[b].indexCount), i32(b));
        Atlas().Upload();
    }
}
