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
#include <Helix/Engine/Time/Time.h>
#include <Helix/Platform/Time/Time.h>

#include <cstring>
#include <utility>

namespace helix::ui
{
    namespace
    {
        LogChannel UILog{"UI"};

        // How often watched files are read again (seconds).
        constexpr f64 kWatchInterval = 0.5;

        struct Views
        {
            Array<UIView*> list;
            u32 drawer = 0;
            engine::PPtr<engine::Material> material;
        };

        Views& V()
        {
            static Views* v = new Views();
            return *v;
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
            // "../" goes up a folder.
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
            engine::Material* material = v.material.Get();
            if (!material)
            {
                engine::Shader* shader = engine::Shader::Find("Hidden/UI/Flat");
                if (!shader)
                    return;
                material = engine::Material::Create(shader);
                material->hideFlags = engine::HideFlags::DontSave;
                v.material = engine::PPtr<engine::Material>(material);
            }
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
            for (UIView* view : order)
                if (engine::Mesh* mesh = view->mesh(); mesh && mesh->vertexCount() > 0)
                    cmd.DrawMesh(mesh, Matrix4x4::identity, material);
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

        // The asset loaders of the markup and the sheets are there once the module is loaded.
        struct LoaderRegistrar
        {
            LoaderRegistrar() { RegisterUIAssetLoaders(); }
        } registrar;
    }

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
        panel_ = std::make_unique<Panel>();
        built_ = false;
        AddView(this);
    }

    void UIView::OnDisable()
    {
        RemoveView(this);
        panel_.reset();
        watched_.Clear();
    }

    void UIView::Build()
    {
        built_ = true;
        errors_.Clear();
        watched_.Clear();
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
        panel_->Quads(quads_);

        engine::Mesh* m = mesh_.Get();
        if (!m)
        {
            m = engine::Mesh::Create();
            m->hideFlags = engine::HideFlags::DontSave;
            m->MarkDynamic();
            mesh_ = engine::PPtr<engine::Mesh>(m);
        }
        usize n = quads_.Size();
        Array<Vector3> positions;
        Array<Color> colors;
        Array<i32> triangles;
        positions.Reserve(n);
        colors.Reserve(n);
        triangles.Reserve(n / 4 * 6);
        for (const PanelVertex& q : quads_)
        {
            positions.Push(Vector3(q.x, q.y, 0));
            colors.Push(q.color);
        }
        for (i32 i = 0; i < i32(n); i += 4)
            for (i32 k : {0, 1, 2, 0, 2, 3})
                triangles.Push(i + k);
        m->Clear();
        m->SetVertices(Span<const Vector3>(positions.Data(), positions.Size()));
        m->SetColors(Span<const Color>(colors.Data(), colors.Size()));
        m->SetTriangles(Span<const i32>(triangles.Data(), triangles.Size()));
    }
}
