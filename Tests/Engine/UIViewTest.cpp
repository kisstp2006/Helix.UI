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

#include <Support/FontWriter.h>
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
        HX_REQUIRE(Shader::Find("Hidden/UI/Default") != nullptr);
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
        // The root, the card, its inner element, the faded one; quads: the card's fill and its border ring (one
        // color), the inner's fill, the faded one's.
        HX_CHECK(view->panel()->tree().Count() == 4);
        HX_REQUIRE(view->mesh() != nullptr);
        HX_CHECK(view->mesh()->vertexCount() == 4 * 4 && view->uiMesh().DrawCalls() == 1);
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

namespace
{
    // a and b: squares from 50 to 450 units wide and 500 tall (at 20 pixels: 1 to 9 pixels, 10 above the baseline);
    // ascender 800 (16 pixels).
    StringView BodyFont(Array<u8>& bytes)
    {
        using namespace font::testing;
        FontSpec s;
        s.glyphs.Resize(4);
        for (u32 g = 1; g < 3; g++)
            s.glyphs[g].contours.Push(Square(50, 0, 450, 500));
        s.glyphs[3].advance = 250;
        s.cmap = {{'a', 1}, {'b', 2}, {' ', 3}};
        s.names = {{String("1"), String("Body Test")}, {String("2"), String("Regular")}};
        bytes = BuildFont(s);
        return StringView(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    }
}

HX_TEST(UIView, LabelsDrawTheirTextOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 64, height = 32;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        Array<u8> font;
        folder.Folder("Fonts");
        folder.Write("Fonts/Body.ttf", BodyFont(font));
        folder.Write("text.hxml", R"(<HXML><Style src="text.hss"/><Label class="t" text="ab ab"/></HXML>)");
        folder.Write("text.hss", R"(.t { -helix-font-definition: url("Fonts/Body.ttf"); font-size: 20px; color: #ffffff; margin: 4px; })");
        folder.Open();
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
        HX_REQUIRE(view != nullptr);
        view->SetSource(LoadAt<UITemplate>("text.hxml"));
        view->Update();
        HX_CHECK(view->errors().Empty());
        ElementHandle label = view->panel()->tree().FirstChild(view->panel()->root());
        const TextInfo* text = view->panel()->TextOf(label);
        HX_REQUIRE(text != nullptr && text->characterCount == 5 && text->quads.Size() == 4);
        HX_CHECK(text->characterInfo[0].font != nullptr && text->characterInfo[0].font->GetFace().Family() == "Body Test");
        // Stretched across the root inside its 4 pixel margin, as tall as its line (ascender 16 and descender 4).
        Rect r = view->panel()->layout().WorldRect(label);
        HX_CHECK(r.x == 4 && r.y == 4 && r.width == 56 && r.height > 19.9f && r.height < 20.1f);
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        // Inside the first a (x 5 to 13, y 10 to 20 on the screen), between the letters, above them.
        HX_CHECK(NearColor(Pixel(pixels, width, 9, 15), Rgba(255, 255, 255), 8) && NearColor(Pixel(pixels, width, 14, 15), Rgba(0, 0, 0), 8));
        HX_CHECK(NearColor(Pixel(pixels, width, 9, 6), Rgba(0, 0, 0), 8) && NearColor(Pixel(pixels, width, 19, 15), Rgba(255, 255, 255), 8));
        HX_CHECK(engine::testing::MatchGolden("UIViewText", pixels.Data(), width, height, 8));
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}

namespace
{
    // A triangle filling the lower right half of the element's content box.
    void PaintTriangle(Painter& p, const ElementTree&, ElementHandle, f32 width, f32 height)
    {
        p.fillColor = Color(1, 1, 1, 1);
        p.MoveTo(Vector2(0, height));
        p.LineTo(Vector2(width, height));
        p.LineTo(Vector2(width, 0));
        p.ClosePath();
        p.Fill();
    }

    // A 2 by 2 image: red, blue on top, green, white below.
    Array<u8> QuadrantsPng()
    {
        Texture2D* t = Texture2D::Create(2, 2, TextureFormat::RGBA32, false);
        t->SetPixel(0, 1, Color(1, 0, 0, 1));
        t->SetPixel(1, 1, Color(0, 0, 1, 1));
        t->SetPixel(0, 0, Color(0, 1, 0, 1));
        t->SetPixel(1, 0, Color(1, 1, 1, 1));
        Array<u8> png;
        t->EncodeToPNG(png);
        Object::DestroyImmediate(t, true);
        return png;
    }

    constexpr StringView kShapesPage = R"(<HXML>
  <Style src="shapes.hss"/>
  <Element name="card"/>
  <Element name="photo"/>
  <TestTriangle name="chart"/>
  <Element name="clip"><Element name="inside"/></Element>
  <Element name="raw"/>
</HXML>)";

    constexpr StringView kShapesSheet = R"(
#card { position: absolute; left: 4px; top: 4px; width: 40px; height: 30px; background-color: #0000ff; border-radius: 10px;
        border-width: 2px; border-left-color: #ff0000; border-top-color: #00ff00; border-right-color: #ffffff; border-bottom-color: #ffff00; }
#photo { position: absolute; left: 48px; top: 4px; width: 40px; height: 30px; border-radius: 15px; background-image: url("Images/quadrants.png"); }
#chart { position: absolute; left: 4px; top: 38px; width: 40px; height: 22px; }
#clip { position: absolute; left: 48px; top: 38px; width: 40px; height: 22px; overflow: hidden; border-radius: 8px; }
#inside { position: absolute; left: -10px; top: -10px; width: 60px; height: 42px; background-color: #ff00ff; }
#raw { position: absolute; left: 4px; top: 64px; width: 40px; height: 28px; background-image: url("Images/raw.png"); }
)";
}

HX_TEST(UIView, ShapesImagesPaintingAndClipsDrawOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    RegisterElementType("TestTriangle", nullptr, &PaintTriangle);
    constexpr u32 width = 96, height = 96;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        folder.Write("shapes.hxml", kShapesPage);
        folder.Write("shapes.hss", kShapesSheet);
        folder.Folder("Images");
        Array<u8> png = QuadrantsPng();
        folder.Write("Images/quadrants.png", StringView(reinterpret_cast<const char*>(png.Data()), png.Size()));
        // Readable, so it goes into the UI's atlas.
        folder.Write("Images/quadrants.png.meta", "TextureImporter:\n  mipmaps:\n    enableMipMap: 0\n  sRGBTexture: 0\n  isReadable: 1\n");
        // Not readable: drawn from its own texture (clamped, as images in a UI are).
        folder.Write("Images/raw.png", StringView(reinterpret_cast<const char*>(png.Data()), png.Size()));
        folder.Write("Images/raw.png.meta", "TextureImporter:\n  mipmaps:\n    enableMipMap: 0\n  sRGBTexture: 0\n  textureSettings:\n    wrapU: 1\n    wrapV: 1\n");
        folder.Open();
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
        HX_REQUIRE(view != nullptr);
        view->SetSource(LoadAt<UITemplate>("shapes.hxml"));
        view->Update();
        HX_CHECK(view->errors().Empty());
        // One batch: the atlas page and the texture that is not readable.
        HX_CHECK(view->uiMesh().DrawCalls() == 1);
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        auto at = [&](u32 x, u32 y, u32 expected, i32 tolerance = 12) {
            u32 p = Pixel(pixels, width, x, y);
            bool ok = NearColor(p, expected, tolerance);
            if (!ok)
                std::printf("    pixel %u %u is %08x, expected %08x\n", x, y, p, expected);
            return ok;
        };
        // The card: its rounded corner leaves the background, each side its color, the fill inside.
        HX_CHECK(at(5, 5, Rgba(0, 0, 0)));
        HX_CHECK(at(4, 19, Rgba(255, 0, 0)) && at(24, 4, Rgba(0, 255, 0)) && at(43, 19, Rgba(255, 255, 255)) && at(24, 33, Rgba(255, 255, 0)));
        HX_CHECK(at(24, 19, Rgba(0, 0, 255)));
        // The image's quadrants (top left red), cut to the pill shape.
        HX_CHECK(at(57, 11, Rgba(255, 0, 0)) && at(78, 11, Rgba(0, 0, 255)) && at(57, 26, Rgba(0, 255, 0)) && at(78, 26, Rgba(255, 255, 255)));
        HX_CHECK(at(49, 5, Rgba(0, 0, 0)) && at(86, 32, Rgba(0, 0, 0)));
        // The painted triangle: the lower right half.
        HX_CHECK(at(40, 56, Rgba(255, 255, 255)) && at(8, 42, Rgba(0, 0, 0)));
        // The child cut to its parent's rounded box.
        HX_CHECK(at(68, 49, Rgba(255, 0, 255)) && at(48, 38, Rgba(0, 0, 0)) && at(45, 49, Rgba(0, 0, 0)) && at(90, 49, Rgba(0, 0, 0)));
        HX_CHECK(at(48, 49, Rgba(255, 0, 255)));
        HX_CHECK(at(13, 70, Rgba(255, 0, 0)) && at(34, 70, Rgba(0, 0, 255)) && at(13, 85, Rgba(0, 255, 0)) && at(34, 85, Rgba(255, 255, 255)));
        HX_CHECK(engine::testing::MatchGolden("UIViewShapes", pixels.Data(), width, height, 8));
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}
