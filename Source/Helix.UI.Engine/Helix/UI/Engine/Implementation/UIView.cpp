#include <Helix/UI/Engine/UIView.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Asset/Asset.h>
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
#include <Helix/UI/Engine/FontAsset.h>

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
            engine::PPtr<engine::Texture2D> texture;
            engine::PPtr<engine::Material> material;
        };

        struct Views
        {
            Array<UIView*> list;
            u32 drawer = 0;
            // One material per texture (the first without one).
            Array<MaterialEntry> materials;
            bool missingShader = false;
        };

        Views& V()
        {
            static Views* v = new Views();
            return *v;
        }

        engine::Material* MaterialFor(engine::Texture2D* texture)
        {
            Views& v = V();
            for (usize i = 0; i < v.materials.Size(); i++)
            {
                MaterialEntry& e = v.materials[i];
                if (e.texture.Get() == texture && e.material.Get())
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
            if (texture)
                m->SetTexture("_MainTex", texture);
            // Entries of destroyed textures make room.
            for (usize i = 0; i < v.materials.Size();)
                if (!v.materials[i].material.Get() || (!v.materials[i].texture.Get() && i > 0))
                    v.materials.RemoveAt(i);
                else
                    i++;
            v.materials.Push({engine::PPtr<engine::Texture2D>(texture), engine::PPtr<engine::Material>(m)});
            return m;
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

        // The views' quads over a game camera's image: pixels from the top left mapped to the camera's target.
        void Draw(void*, engine::ScriptableRenderContext& context, engine::Camera* camera)
        {
            Views& v = V();
            if (!camera || camera->cameraType() != engine::CameraType::Game || camera->targetTexture() || v.list.Empty())
                return;
            f32 w = f32(camera->pixelWidth()), h = f32(camera->pixelHeight());
            engine::CommandBuffer cmd;
            cmd.SetName("UI");
            cmd.SetRenderTarget(engine::RenderTargetIdentifier(engine::BuiltinRenderTextureType::CameraTarget));
            cmd.SetViewport(Rect(0, 0, w, h));
            cmd.SetViewProjectionMatrices(Matrix4x4::identity, Matrix4x4::Ortho(0, w, h, 0, -1, 1));
            // By sorting order, the order of enabling among equal ones.
            Array<UIView*> order(v.list);
            for (usize i = 1; i < order.Size(); i++)
                for (usize j = i; j > 0 && order[j - 1]->sortingOrder() > order[j]->sortingOrder(); j--)
                    std::swap(order[j - 1], order[j]);
            bool any = false;
            for (UIView* view : order)
            {
                engine::Mesh* mesh = view->mesh();
                if (!mesh || mesh->vertexCount() == 0)
                    continue;
                Span<const PanelBatch> batches = view->batches();
                for (u32 b = 0; b < batches.Size(); b++)
                    if (engine::Material* m = MaterialFor(batches[b].texture))
                    {
                        cmd.DrawMesh(mesh, Matrix4x4::identity, m, i32(b));
                        any = true;
                    }
            }
            if (!any)
                return;
            context.ExecuteCommandBuffer(cmd);
            context.Submit();
        }

        void AddView(UIView* view)
        {
            Views& v = V();
            if (v.list.Empty())
                v.drawer = engine::RenderPipelineManager::endCameraRendering().Add(&Draw);
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
            if (v.list.Empty() && v.drawer)
            {
                engine::RenderPipelineManager::endCameraRendering().Remove(v.drawer);
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

        void Forget() { definitions_.Clear(); }

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
        UIView& view_;
        Array<Entry> definitions_;
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
        batches_.Clear();
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
        panel_->Update(engine::Time::deltaTime(), f32(engine::Screen::width()), f32(engine::Screen::height()));
        panel_->Draw(vertices_, indices_, batches_);

        engine::Mesh* m = mesh_.Get();
        if (!m)
        {
            m = engine::Mesh::Create();
            m->hideFlags = engine::HideFlags::DontSave;
            m->MarkDynamic();
            mesh_ = engine::PPtr<engine::Mesh>(m);
        }
        usize n = vertices_.Size();
        Array<Vector3> positions;
        Array<Color> colors;
        Array<Vector2> uvs;
        Array<Vector4> parameters;
        positions.Reserve(n);
        colors.Reserve(n);
        uvs.Reserve(n);
        parameters.Reserve(n);
        for (const PanelVertex& q : vertices_)
        {
            positions.Push(Vector3(q.x, q.y, 0));
            colors.Push(q.color);
            uvs.Push(Vector2(q.u, q.v));
            parameters.Push(Vector4(q.kind, q.edge, q.scale, q.unused));
        }
        m->Clear();
        m->SetVertices(Span<const Vector3>(positions.Data(), positions.Size()));
        m->SetColors(Span<const Color>(colors.Data(), colors.Size()));
        m->SetUVs(0, Span<const Vector2>(uvs.Data(), uvs.Size()));
        m->SetUVs(1, Span<const Vector4>(parameters.Data(), parameters.Size()));
        m->SetSubMeshCount(i32(batches_.Size() > 0 ? batches_.Size() : 1));
        for (u32 b = 0; b < batches_.Size(); b++)
            m->SetTriangles(Span<const i32>(reinterpret_cast<const i32*>(indices_.Data()) + batches_[b].firstIndex, batches_[b].indexCount), i32(b));
    }
}
