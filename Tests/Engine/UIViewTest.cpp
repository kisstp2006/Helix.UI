#include <Engine/Support/TestPipeline.h>
#include <Support/Devices.h>
#include <Support/Golden.h>

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Platform/Time/Time.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Types/Type.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Engine/UIView.h>

#include <UIShadersAssembly.h>

#include <cstdio>

HX_BINDINGS_EXPORT void HxBindings_UIEngineTests_Publish();
extern "C" void HxBindings_HelixUI_Publish();

using namespace helix;
using namespace helix::engine;
using namespace helix::ui;

namespace
{
    // The UI's module and the tests' (registered once per process).
    bool RegisterModules()
    {
        static int state = 0;
        if (state == 0)
        {
            state = -1;
            HxBindings_HelixUI_Publish();
            HxBindings_UIEngineTests_Publish();
            String error;
            for (const char* module : {"HelixUI", "UIEngineTests"})
                if (!RegisterLinkedGameModule(module, error))
                {
                    std::printf("    %.*s\n", int(error.Size()), error.Data());
                    return false;
                }
            state = 1;
        }
        return state > 0;
    }

    // A folder of assets: the UI's shaders and the files a test writes, listed by a folder source.
    struct AssetFolder
    {
        String root;
        FolderAssetSource* source = nullptr;
        Array<String> files, folders;

        AssetFolder()
        {
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-ui-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
            Write("Helix.UI.Shaders.dll", StringView(reinterpret_cast<const char*>(kUIShadersAssembly), sizeof(kUIShadersAssembly)));
        }

        void Write(StringView path, StringView text)
        {
            String file = platform::JoinPath(root, path);
            platform::WriteFile(file, Span<const u8>(reinterpret_cast<const u8*>(text.Data()), text.Size()));
            files.Push(file);
        }

        void Folder(StringView path)
        {
            String folder = platform::JoinPath(root, path);
            platform::CreateDirectories(folder);
            folders.Push(folder);
        }

        // Lists the folder (again) and loads the shaders.
        void Open()
        {
            if (!source)
            {
                source = new FolderAssetSource(root);
                AddAssetSource(source);
            }
            else
                RefreshAssets();
            if (const AssetEntry* e = FindAssetAtPath("Helix.UI.Shaders.dll"))
                LoadAsset(e->guid);
        }

        ~AssetFolder()
        {
            // Every object the test made, then the assets and the files.
            GraphicsSettings::SetDefaultRenderPipeline(nullptr);
            Array<Object*> all;
            Array<EntityId> ids;
            FindObjectsOfTypeAll(ClassOf<GameObject>(), all);
            for (Object* o : all)
                ids.Push(o->GetEntityId());
            for (EntityId id : ids)
                if (GameObject* g = EntityIdToObject<GameObject>(id))
                    if (!g->transform() || !g->transform()->parent())
                        DestroyImmediate(g, true);
            FindObjectsOfTypeAll(ClassOf<Object>(), all);
            ids.Clear();
            for (Object* o : all)
                ids.Push(o->GetEntityId());
            for (EntityId id : ids)
                if (Object* o = EntityIdToObject(id))
                    DestroyImmediate(o, true);
            while (PendingDestroyCount())
                ProcessDestroyQueue(), SetLifecycleTime(LifecycleTime() + 1e9);
            SetLifecycleTime(0);
            ResetAssets();
            Shader::Globals().Clear();
            delete source;
            for (const String& f : files)
                platform::RemoveFile(f);
            for (usize i = folders.Size(); i-- > 0;)
                platform::RemoveEmptyDirectory(folders[i]);
            platform::RemoveEmptyDirectory(root);
        }
    };

    template <class T>
    T* LoadAt(StringView path)
    {
        const AssetEntry* e = FindAssetAtPath(path);
        return e ? ScriptObjectAs<T>(LoadAsset(e->guid)) : nullptr;
    }

    void ReportScreen(i32 width, i32 height)
    {
        ScreenReport r = Screen::Reported();
        r.width = width;
        r.height = height;
        Screen::Report(r);
    }

    u32 Pixel(const Array<u32>& pixels, u32 width, u32 x, u32 y) { return pixels[y * width + x]; }

    bool NearColor(u32 a, u32 b, i32 tolerance = 3)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            i32 ca = i32((a >> shift) & 0xFF), cb = i32((b >> shift) & 0xFF);
            if (ca - cb > tolerance || cb - ca > tolerance)
                return false;
        }
        return true;
    }

    constexpr u32 Rgba(u32 r, u32 g, u32 b, u32 a = 255) { return r | (g << 8) | (b << 16) | (a << 24); }

    constexpr StringView kPage = R"(<HXML>
  <Style src="Styles/page.hss"/>
  <Element name="box" class="card">
    <Element class="inner"/>
  </Element>
  <Element name="faded" style="opacity: 0.5"/>
</HXML>)";

    constexpr StringView kSheet = R"(
:root { --accent: #00ff00; }
.card { position: absolute; left: 8px; top: 8px; width: 32px; height: 24px; background-color: #0000ff;
        border-width: 2px; border-color: #ff0000; padding: 4px; }
.inner { flex-grow: 1; background-color: var(--accent); }
#faded { position: absolute; left: 44px; top: 40px; width: 16px; height: 16px; background-color: #ffffff; }
)";
}

HX_TEST(UIAssets, MarkupAndSheetsLoadAsAssets)
{
    HX_REQUIRE(RegisterModules());
    AssetFolder folder;
    folder.Write("page.hxml", kPage);
    folder.Folder("Styles");
    folder.Write("Styles/page.hss", kSheet);
    folder.Write("broken.hss", ".a { width: ; }");
    folder.Open();
    UITemplate* page = LoadAt<UITemplate>("page.hxml");
    HX_REQUIRE(page != nullptr);
    HX_CHECK(page->path() == "page.hxml" && page->document().Nodes().Size() == 3);
    StyleSheetAsset* sheet = LoadAt<StyleSheetAsset>("Styles/page.hss");
    HX_REQUIRE(sheet != nullptr);
    HX_CHECK(sheet->sheet()->Errors().Empty() && sheet->sheet()->Rules().Size() == 4);
    // A sheet with errors still loads (its errors are in it).
    StyleSheetAsset* broken = LoadAt<StyleSheetAsset>("broken.hss");
    HX_REQUIRE(broken != nullptr);
    HX_CHECK(!broken->sheet()->Errors().Empty());
}

HX_TEST(UIView, BuildsLaysOutAndDrawsOverTheCameraOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 64, height = 64;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        folder.Write("page.hxml", kPage);
        folder.Folder("Styles");
        folder.Write("Styles/page.hss", kSheet);
        folder.Open();
        HX_REQUIRE(Shader::Find("Hidden/UI/Flat") != nullptr);
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = width, .height = height, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth = device.CreateTexture({.format = rhi::Format::D32Float, .width = width, .height = height, .usage = rhi::TextureDepthStencil});
        Graphics::SetDevice(&device);
        Graphics::SetTarget({color, depth, width, height, rhi::Format::RGBA8Unorm, rhi::Format::D32Float});
        ReportScreen(i32(width), i32(height));
        auto* asset = ScriptableObject::CreateInstanceByName("ClearPipelineAsset");
        HX_REQUIRE(asset != nullptr);
        GraphicsSettings::SetDefaultRenderPipeline(asset);

        GameObject* cameraObject = GameObject::Create("Camera");
        Camera* camera = cameraObject->AddComponent<Camera>();
        camera->SetClearFlags(CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0, 0, 0, 1));
        GameObject* ui = GameObject::Create("UI");
        auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
        HX_REQUIRE(view != nullptr && view->panel() != nullptr);
        view->SetSource(LoadAt<UITemplate>("page.hxml"));
        view->Update();
        HX_CHECK(view->errors().Empty());
        // The root, the card, its inner element, the faded one; quads: the card's background and borders, the inner's
        // background, the faded one's.
        HX_CHECK(view->panel()->tree().Count() == 4);
        HX_REQUIRE(view->mesh() != nullptr);
        HX_CHECK(view->mesh()->vertexCount() == 7 * 4);
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        // Rows top first: the red border at 8..10, the blue padding, the green content from 14 to 34 by 14 to 26.
        HX_CHECK(NearColor(Pixel(pixels, width, 4, 4), Rgba(0, 0, 0)));
        HX_CHECK(NearColor(Pixel(pixels, width, 9, 20), Rgba(255, 0, 0)) && NearColor(Pixel(pixels, width, 20, 8), Rgba(255, 0, 0)));
        HX_CHECK(NearColor(Pixel(pixels, width, 11, 20), Rgba(0, 0, 255)) && NearColor(Pixel(pixels, width, 20, 12), Rgba(0, 0, 255)));
        HX_CHECK(NearColor(Pixel(pixels, width, 20, 20), Rgba(0, 255, 0)) && NearColor(Pixel(pixels, width, 33, 25), Rgba(0, 255, 0)));
        HX_CHECK(NearColor(Pixel(pixels, width, 39, 31), Rgba(255, 0, 0)) && NearColor(Pixel(pixels, width, 41, 33), Rgba(0, 0, 0)));
        // Half white over black (premultiplied: the color and the alpha halved, then over the target).
        HX_CHECK(NearColor(Pixel(pixels, width, 50, 50) & 0x00ffffffu, Rgba(128, 128, 128, 0), 2));
        HX_CHECK(engine::testing::MatchGolden("UIViewCard", pixels.Data(), width, height));

        // The sheet edited on disk: the view built again from the new text when it watches its files.
        view->SetWatchFiles(true);
        folder.Write("Styles/page.hss", R"(.card { position: absolute; left: 0px; top: 0px; width: 64px; height: 64px; background-color: #ffff00; })");
        view->Update();
        platform::SleepSeconds(0.6);
        view->Update();
        HX_CHECK(view->mesh()->vertexCount() > 0);
        Graphics::RenderFrame();
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(NearColor(Pixel(pixels, width, 2, 2), Rgba(255, 255, 0)) && NearColor(Pixel(pixels, width, 60, 60), Rgba(255, 255, 0)));

        // Disabled: nothing drawn over the camera.
        view->SetEnabled(false);
        HX_CHECK(UIView::Views().Empty() && view->panel() == nullptr);
        Graphics::RenderFrame();
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(NearColor(Pixel(pixels, width, 20, 20), Rgba(0, 0, 0)));
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    // Where there is no device (node, a phone without a window) nothing ran.
    std::printf("    %u devices\n", devices);
}
