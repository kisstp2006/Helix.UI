#include <Engine/Support/CppScripts.h>
#include <Engine/Support/TestAssets.h>
#include <Engine/Support/TestPipeline.h>
#include <Engine/Support/UIHost.h>
#include <Support/Devices.h>
#include <Support/Golden.h>

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Input/Cursor.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/Implementation/RenderCore.h>
#include <Helix/Engine/Texture/RenderTexture.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Platform/Time/Time.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Types/Type.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Engine/Containers.h>
#include <Helix/UI/Engine/PanelInputConfiguration.h>
#include <Helix/UI/Engine/UIView.h>

#include <Support/FontWriter.h>
#include <UIShadersAssembly.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>


using namespace helix;
using namespace helix::engine;
using namespace helix::ui;
using helix::ui::testing::RegisterModules;
using helix::ui::testing::AssetFolder;
using helix::ui::testing::LoadAt;
using helix::ui::testing::ManualClock;
using helix::ui::testing::NearColor;
using helix::ui::testing::PageFont;
using helix::ui::testing::Pixel;
using helix::ui::testing::ReportScreen;
using helix::ui::testing::Rgba;

namespace
{
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
    StyleSheet* sheet = LoadAt<StyleSheet>("Styles/page.hss");
    HX_REQUIRE(sheet != nullptr);
    HX_CHECK(sheet->sheet()->Errors().Empty() && sheet->sheet()->Rules().Size() == 4);
    // A sheet with errors still loads (its errors are in it).
    StyleSheet* broken = LoadAt<StyleSheet>("broken.hss");
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
        // The view's root, the card, its inner element, the faded one; quads: the card's fill and its border ring (one
        // color), the inner's fill, the faded one's.
        u32 inPanel = 0;
        view->panel()->tree().ForEachInSubtree(view->rootElement()->handle(), [&](ElementHandle, u32) { inPanel++; });
        HX_CHECK(inPanel == 4);
        HX_REQUIRE(view->mesh() != nullptr);
        HX_CHECK(view->uiMesh().Vertices().Size() == 4 * 4 && view->mesh()->vertexCount() >= 4 * 4 && view->uiMesh().DrawCalls() == 1);
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
        folder.Write("text.hss", R"(.t { -helix-font-definition: url("Fonts/Body.ttf"); font-size: 20px; color: #ffffff; margin: 4px; padding: 0; })");
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
        ElementHandle label = view->panel()->tree().FirstChild(view->rootElement()->handle());
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
  <Style src="Styles/shapes.hss"/>
  <Element name="card"/>
  <Element name="photo"/>
  <TestTriangle name="chart"/>
  <Element name="clip"><Element name="inside"/></Element>
  <Element name="raw"/>
</HXML>)";

    constexpr StringView kShapesSheet = R"(
#card { position: absolute; left: 4px; top: 4px; width: 40px; height: 30px; background-color: #0000ff; border-radius: 10px;
        border-width: 2px; border-left-color: #ff0000; border-top-color: #00ff00; border-right-color: #ffffff; border-bottom-color: #ffff00; }
#photo { position: absolute; left: 48px; top: 4px; width: 40px; height: 30px; border-radius: 15px; background-image: url("../Images/quadrants.png"); }
#chart { position: absolute; left: 4px; top: 38px; width: 40px; height: 22px; }
#clip { position: absolute; left: 48px; top: 38px; width: 40px; height: 22px; overflow: hidden; border-radius: 8px; }
#inside { position: absolute; left: -10px; top: -10px; width: 60px; height: 42px; background-color: #ff00ff; }
#raw { position: absolute; left: 4px; top: 64px; width: 40px; height: 28px; background-image: url("../Images/raw.png"); }
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
        // The sheet in a folder of its own: its paths are relative to it.
        folder.Folder("Styles");
        folder.Write("Styles/shapes.hss", kShapesSheet);
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

namespace
{
    constexpr StringView kInputPage = R"(<HXML>
  <Style src="input.hss"/>
  <Element name="button" focusable="true"/>
</HXML>)";

    constexpr StringView kInputSheet = R"(
#button { position: absolute; left: 8px; top: 8px; width: 32px; height: 24px; background-color: #ff0000; }
#button:hover { background-color: #00ff00; }
)";

    engine::InputEvent Mouse(platform::EventType type, f32 x, f32 y)
    {
        engine::InputEvent e;
        e.event.type = type;
        e.event.mouse = {x, y, platform::MouseButton::Left};
        return e;
    }

    struct Counts
    {
        i32 clicks = 0, keys = 0;
        u32 character = 0;
    };
}

HX_TEST(UIView, TheFramesInputReachesTheViewAndTheGameKeepsIt)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 64, height = 48;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        folder.Write("input.hxml", kInputPage);
        folder.Write("input.hss", kInputSheet);
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
        view->SetSource(LoadAt<UITemplate>("input.hxml"));
        engine::InputFeed::Reset();
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(), Span<const u32>());
        view->Update();
        EventSystem& events = view->panel()->events();
        ElementHandle button = view->panel()->tree().FirstChild(view->rootElement()->handle());
        HX_REQUIRE(button.IsValid());
        Counts counts;
        const EventTypes& t = Events();
        events.RegisterCallback(button, t.click, [](void* u, EventSystem&, UIEvent&) { static_cast<Counts*>(u)->clicks++; }, &counts);
        events.RegisterCallback(button, t.keyDown, [](void* u, EventSystem&, UIEvent& e) {
            static_cast<Counts*>(u)->keys++;
            if (e.character)
                static_cast<Counts*>(u)->character = e.character;
        }, &counts);

        // The mouse over it: hovered, drawn with the hover style.
        engine::InputEvent move = Mouse(platform::EventType::MouseMove, 20, 20);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(&move, 1), Span<const u32>());
        view->Update();
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(NearColor(Pixel(pixels, width, 20, 20), Rgba(0, 255, 0)));
        // A second update of the same frame reads nothing again.
        view->Update();

        // A click: focused and clicked.
        const engine::InputEvent click[] = {Mouse(platform::EventType::MouseDown, 20, 20), Mouse(platform::EventType::MouseUp, 21, 20)};
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(click, 2), Span<const u32>());
        view->Update();
        HX_CHECK(counts.clicks == 1 && events.Focused() == button);

        // Keys to the focus; the game's frame still has them all.
        engine::InputEvent keys[2];
        keys[0].event.type = platform::EventType::KeyDown;
        keys[0].event.key = {platform::KeyCode::A, 0};
        keys[1].event.type = platform::EventType::Text;
        keys[1].event.text = {'a'};
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(keys, 2), Span<const u32>());
        view->Update();
        HX_CHECK(counts.keys == 2 && counts.character == 'a' && engine::InputFeed::Frame().Size() == 2);

        // The game asks the view what is under a point.
        HX_CHECK(view->Pick(Vector2(20, 20)) == button && !view->Pick(Vector2(60, 40)).IsValid());

        // A focused text element keeps the input method on, at its caret on the screen.
        events.SetTextInput(button, Rect(2, 3, 1, 16));
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(), Span<const u32>());
        view->Update();
        i32 x = 0, y = 0, h = 0;
        HX_CHECK(engine::InputFeed::TakeTextInputRequest(x, y, h) && x == 10 && y == 11 && h == 16);
        engine::InputFeed::Reset();
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(UIScripts, CSharpCallbacksGetTheViewsEventsInTheirOrder)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ui::testing::Notes().Clear();
    AssetFolder folder;
    folder.Write("page.hxml", R"(<HXML><Element name="button" focusable="true" style="position: absolute; left: 8px; top: 8px; width: 32px; height: 24px;"/></HXML>)");
    folder.Open();
    ReportScreen(64, 48);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("page.hxml"));
    engine::InputFeed::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    frame({});
    // The C# script on the view's GameObject: its callbacks, an element it adds, a scheduled item.
    ui->AddComponent(ui::testing::CSharp("CsEventTests").type);
    RunScriptPhase(ScriptMessage::Update);
    // The phase updates the view too (a GameBehaviour): the element the script added comes into the panel.
    HX_CHECK(ui::testing::NotesAre({"found:button:1", "attached:extra", "geometry:40"}));
    // The scheduler ticks with the next frame's input.
    frame({});
    HX_CHECK(ui::testing::NotesAre({"scheduled"}));
    // A click: the root's trickle down callback first, then the button's; the focus after the press; the click.
    frame({Mouse(platform::EventType::MouseDown, 20, 20), Mouse(platform::EventType::MouseUp, 20, 20)});
    HX_CHECK(ui::testing::NotesAre({"root-down:TrickleDown:button", "button-down:BubbleUp:0:20", "focus:True", "click:1:12"}));
    engine::InputEvent keys[2];
    keys[0].event.type = platform::EventType::KeyDown;
    keys[0].event.key = {platform::KeyCode::A, 0};
    keys[1].event.type = platform::EventType::Text;
    keys[1].event.text = {'a'};
    frame({keys[0], keys[1]});
    HX_CHECK(ui::testing::NotesAre({"key:A:0", "key:None:97"}));
    // From C#: the focus, an event sent, capture.
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"focused:button", "submit:button", "capture:True:button", "released:False:button"}));
    engine::InputFeed::Reset();
}

HX_TEST(UIView, AThemeImportsItsSheetsAndTheCursorFollowsTheHoveredElement)
{
    HX_REQUIRE(RegisterModules());
    AssetFolder folder;
    folder.Write("page.hxml", R"(<HXML><Style src="Themes/main.tss"/><Style src="loop.hss"/>
        <Element name="box" class="box"><Element name="inner" style="width: 10px; height: 10px;"/></Element>
        <Element name="plain" style="position: absolute; left: 0px; top: 40px; width: 20px; height: 8px;"/></HXML>)");
    folder.Folder("Themes");
    folder.Write("Themes/main.tss", R"(@import url("../base.hss"); :root { --accent: #00ff00; })");
    folder.Write("base.hss", R"(.box { position: absolute; left: 10px; top: 10px; width: 20px; height: 20px; background-color: var(--accent); cursor: link; })");
    // A sheet importing itself through another is reported, the rest applies.
    folder.Write("loop.hss", R"(@import "loop2.hss"; .x { width: 1px; })");
    folder.Write("loop2.hss", R"(@import "loop.hss";)");
    folder.Open();
    ThemeStyleSheet* theme = LoadAt<ThemeStyleSheet>("Themes/main.tss");
    HX_REQUIRE(theme != nullptr);
    HX_CHECK(theme->sheet()->Imports().Size() == 1 && !theme->importedWithErrors() && theme->contentHash() != 0);
    ReportScreen(64, 48);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("page.hxml"));
    engine::InputFeed::Reset();
    engine::Cursor::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    frame({});
    // The theme's import and its variable.
    Element* box = view->rootElement()->ElementAtChecked(0);
    HX_REQUIRE(box != nullptr && box->name() == StringView("box"));
    HX_CHECK(box->ResolvedStyleColor("background-color").g > 0.99f && box->ResolvedStyleNumber("width") == 20);
    // Over the box (and its child, which inherits it): the link shape; off it: the arrow again.
    u64 before = engine::Cursor::Current().version;
    frame({Mouse(platform::EventType::MouseMove, 15, 15)});
    HX_CHECK(engine::Cursor::Current().shape == platform::SystemCursor::Hand && engine::Cursor::Current().version == before + 1);
    frame({Mouse(platform::EventType::MouseMove, 25, 25)});
    HX_CHECK(engine::Cursor::Current().version == before + 1);
    frame({Mouse(platform::EventType::MouseMove, 5, 44)});
    HX_CHECK(engine::Cursor::Current().shape == platform::SystemCursor::Arrow);
    // A game's cursor stays while the mouse is off the views' elements.
    engine::Cursor::SetSystemCursor(platform::SystemCursor::Crosshair);
    frame({Mouse(platform::EventType::MouseMove, 60, 2)});
    HX_CHECK(engine::Cursor::Current().shape == platform::SystemCursor::Crosshair);
    DestroyImmediate(ui);
    engine::InputFeed::Reset();
    engine::Cursor::Reset();
}

HX_TEST(UIScripts, CSharpSetsTheLooksAndReadsCustomProperties)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ui::testing::Notes().Clear();
    AssetFolder folder;
    folder.Write("page.hxml", R"(<HXML><Style src="looks.hss"/><Element name="box" class="box"/></HXML>)");
    folder.Write("looks.hss", R"(:root { --base: #00ff00; } .box { width: 20px; height: 20px; --accent: var(--base); --size: 12px; --title: "Hi"; --flag: true; })");
    folder.Open();
    ReportScreen(64, 48);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("page.hxml"));
    engine::InputFeed::Reset();
    view->Update();
    ui->AddComponent(ui::testing::CSharp("CsStyleTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"inline:10px:50%:90:3:4px:Right:5px:Repeat:Cover:3:1:2:Middle", "unset:Null", "cursor:True:2", "needs-dashes",
                                    "custom:True:1:12:Hi:True:False"}));
    RunScriptPhase(ScriptMessage::Update);
    // Scaled 2 x 3 about (0, 4): 40 x 60.
    HX_CHECK(ui::testing::NotesAre({"resolved:10:2:4:Cover:2:2:Middle", "bound:40:60"}));
    DestroyImmediate(ui);
}

HX_TEST(UIScripts, MarkupMakesElementTypesOfCSharpAndCppClasses)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ui::testing::Notes().Clear();
    ui::testing::CppNotes().Clear();
    AssetFolder folder;
    folder.Write("types.hxml", R"(<HXML><Style src="types.hss"/>
        <CsCard name="card" title="Hello" count="3" accent-color="#ff0000" mood="very-happy"><Element name="inner"/></CsCard>
        <fancy-card name="fancy" shiny="true" title="F"/>
        <CsCard name="bad" nonsense="1"/>
        <CppGauge name="gauge" value="0.5" label="fuel" max-value="10" tint="#00ff00" level="0.75" wrong="2"/>
    </HXML>)");
    folder.Write("types.hss", R"(CsCard { opacity: 0.5; } CppGauge { opacity: 0.25; })");
    folder.Open();
    ReportScreen(64, 48);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("types.hxml"));
    engine::InputFeed::Reset();
    view->Update();
    // The unknown attributes are the build's errors; the C++ part was made with its own child.
    u32 unknown = 0;
    for (const HxmlError& e : view->errors())
        unknown += e.message.View().Contains("has no attribute") ? 1 : 0;
    HX_CHECK(unknown == 2);
    HX_CHECK(ui::testing::CppNotes().Size() == 2 && ui::testing::CppNotes()[0] == StringView("gauge-made:1") &&
             ui::testing::CppNotes()[1] == StringView("level:0.75"));
    // Built again: the old gauge's C++ part goes with its element (no script holds it), the new one is made.
    ui::testing::CppNotes().Clear();
    view->Rebuild();
    view->Update();
    HX_CHECK(ui::testing::CppNotes().Size() == 3 && ui::testing::CppNotes()[0] == StringView("gauge-gone") &&
             ui::testing::CppNotes()[1] == StringView("gauge-made:1") && ui::testing::CppNotes()[2] == StringView("level:0.75"));
    // The C++ object of the gauge, found from its element's handle, with its fields.
    Element* gauge = nullptr;
    for (i32 i = 0; i < view->rootElement()->childCount(); i++)
        if (Element* c = view->rootElement()->ElementAtChecked(i); c && c->name() == StringView("gauge"))
            gauge = c;
    HX_REQUIRE(gauge != nullptr);
    auto* part = scripting::HandleObject<ui::testing::CppGauge>(::HelixEngine::UI::Element::HxFromId(gauge->Id()));
    HX_REQUIRE(part != nullptr);
    HX_CHECK(part->value == 0.5f && part->label == StringView("fuel") && part->maxValue == 10 && part->color.g > 0.99f);
    ui->AddComponent(ui::testing::CSharp("CsElementTypeTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"card:True:Hello:3:1:VeryHappy:header:1:2", "inner:content", "fancy:True:F:CsFancy",
                                    "instance:True:4:True", "clone:0:4", "sheets:1:True:True", "removed:0:False", "moved:1:True",
                                    "gauge:0.25", "made:content"}));
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"typed:0.5:0.5:1"}));
    DestroyImmediate(ui);
}

HX_TEST(UIPanelSettings, TheScaleModesResolveAgainstTheScreen)
{
    HX_REQUIRE(RegisterModules());
    auto* s = static_cast<UIPanelSettings*>(ScriptableObject::CreateInstance(scripting::TypeOf<UIPanelSettings>()));
    HX_REQUIRE(s != nullptr);
    s->SetScale(2);
    HX_CHECK(s->ResolveScale(640, 480, 0) == 2);
    s->SetScaleMode(PanelScaleMode::ConstantPhysicalSize);
    s->SetScale(1);
    HX_CHECK(s->ResolveScale(640, 480, 192) == 2 && s->ResolveScale(640, 480, 0) == 1);
    s->SetScaleMode(PanelScaleMode::ScaleWithScreenSize);
    // The reference 1200 x 800: twice as wide, as tall.
    HX_CHECK(s->ResolveScale(2400, 800, 0) == 2);
    s->SetMatch(1);
    HX_CHECK(s->ResolveScale(2400, 800, 0) == 1);
    s->SetMatch(0.5f);
    HX_CHECK(s->ResolveScale(2400, 800, 0) > 1.414f && s->ResolveScale(2400, 800, 0) < 1.4143f);
    s->SetScreenMatchMode(PanelScreenMatchMode::Shrink);
    HX_CHECK(s->ResolveScale(2400, 800, 0) == 2);
    s->SetScreenMatchMode(PanelScreenMatchMode::Expand);
    HX_CHECK(s->ResolveScale(2400, 800, 0) == 1);
    Object::DestroyImmediate(s);
}

namespace
{
    constexpr StringView kRedPage =
        R"(<HXML><Element name="red" style="position: absolute; left: 0px; top: 0px; width: 10px; height: 10px; background-color: #ff0000;"/></HXML>)";
    constexpr StringView kGreenPage =
        R"(<HXML><Element name="green" style="position: absolute; left: 0px; top: 0px; width: 20px; height: 20px; background-color: #00ff00;"/></HXML>)";
}

HX_TEST(UIView, ViewsOfOneSettingsShareAScaledSortedPanelOrDrawIntoItsTexture)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 64, height = 48;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        folder.Write("red.hxml", kRedPage);
        folder.Write("green.hxml", kGreenPage);
        folder.Open();
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = width, .height = height, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth = device.CreateTexture({.format = rhi::Format::D32Float, .width = width, .height = height, .usage = rhi::TextureDepthStencil});
        Graphics::SetDevice(&device);
        Graphics::SetTarget({color, depth, width, height, rhi::Format::RGBA8Unorm, rhi::Format::D32Float});
        ReportScreen(i32(width), i32(height));
        GraphicsSettings::SetDefaultRenderPipeline(ScriptableObject::CreateInstanceByName("ClearPipelineAsset"));
        Camera* camera = GameObject::Create("Camera")->AddComponent<Camera>();
        camera->SetClearFlags(CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0, 0, 0, 1));
        auto* settings = static_cast<UIPanelSettings*>(ScriptableObject::CreateInstance(scripting::TypeOf<UIPanelSettings>()));
        settings->SetScale(2);
        auto view = [&](StringView page, UIPanelSettings* s, f32 order) {
            auto* v = static_cast<UIView*>(GameObject::Create("UI")->AddComponent(scripting::TypeOf<UIView>()));
            v->SetPanelSettings(s);
            v->SetSortingOrder(order);
            v->SetSource(LoadAt<UITemplate>(page));
            return v;
        };
        // Two views of one settings asset: one panel, the red view (order 1) over the green one, both twice as big.
        UIView* red = view("red.hxml", settings, 1);
        UIView* green = view("green.hxml", settings, 0);
        HX_REQUIRE(red->panel() != nullptr && red->panel() == green->panel());
        ElementTree& tree = red->panel()->tree();
        HX_CHECK(tree.ChildAt(red->panel()->root(), 0) == green->rootElement()->handle() &&
                 tree.ChildAt(red->panel()->root(), 1) == red->rootElement()->handle());
        HX_CHECK(UIView::FindUIView(Element::Of(tree.FirstChild(red->rootElement()->handle()))) == red);
        engine::InputFeed::Reset();
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(), Span<const u32>());
        red->Update();
        green->Update();
        HX_CHECK(UIView::PanelScaleInternal(Element::Of(red->panel()->root())) == 2);
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(NearColor(Pixel(pixels, width, 5, 5), Rgba(255, 0, 0)) && NearColor(Pixel(pixels, width, 18, 18), Rgba(255, 0, 0)));
        HX_CHECK(NearColor(Pixel(pixels, width, 30, 30), Rgba(0, 255, 0)) && NearColor(Pixel(pixels, width, 42, 30), Rgba(0, 0, 0)));
        // Screen points are the panel's halved.
        HX_CHECK(red->Pick(Vector2(18, 18)) == tree.FirstChild(red->rootElement()->handle()));
        HX_CHECK(red->Pick(Vector2(30, 30)) == tree.FirstChild(green->rootElement()->handle()));
        // The sorting order changed: the green view over the red one.
        green->SetSortingOrder(2);
        HX_CHECK(tree.ChildAt(red->panel()->root(), 1) == green->rootElement()->handle());

        // A settings asset with a target texture: its panel draws there (cleared to blue), not on the screen.
        auto* textured = static_cast<UIPanelSettings*>(ScriptableObject::CreateInstance(scripting::TypeOf<UIPanelSettings>()));
        RenderTexture* rt = RenderTexture::New(32, 32, 0);
        textured->SetTargetTexture(rt);
        textured->SetClearColor(true);
        textured->SetColorClearValue(Color(0, 0, 1, 1));
        red->SetPanelSettings(textured);
        HX_CHECK(red->panel() != green->panel() && red->Pick(Vector2(5, 5)) == ElementHandle());
        red->Update();
        green->Update();
        Graphics::RenderFrame();
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(NearColor(Pixel(pixels, width, 5, 5), Rgba(0, 255, 0)));
        const render::GpuRenderTarget* g = render::RenderTargetOf(*rt);
        HX_REQUIRE(g != nullptr);
        Array<u32> texels;
        texels.Resize(32 * 32);
        HX_REQUIRE(device.ReadTexture(g->color, 0, texels.Data(), texels.Size() * 4));
        // Drawn like a camera into a texture: its first row is the panel's bottom.
        HX_CHECK(NearColor(Pixel(texels, 32, 5, 27), Rgba(255, 0, 0)) && NearColor(Pixel(texels, 32, 5, 4), Rgba(0, 0, 255)));
        HX_CHECK(NearColor(Pixel(texels, 32, 20, 27), Rgba(0, 0, 255)));
        red->SetPanelSettings(nullptr);
        Object::DestroyImmediate(rt);
        engine::InputFeed::Reset();
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}

namespace
{
    constexpr StringView kWorldPage =
        R"(<HXML><Element name="panel" style="width: 20px; height: 20px; background-color: #00ff00;">)"
        R"(<Element name="corner" style="position: absolute; left: 0px; top: 0px; width: 10px; height: 10px; background-color: #ff0000;"/>)"
        R"(<Element name="tilted" style="position: absolute; left: 12px; top: 12px; width: 4px; height: 4px; rotate: 1 0 0 60deg; background-color: #0000ff;"/>)"
        R"(</Element></HXML>)";
}

HX_TEST(UIView, AWorldSpacePanelIsDrawnInTheSceneAndTakesRaysFromTheEventCameras)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 64, height = 64;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        folder.Write("world.hxml", kWorldPage);
        folder.Open();
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = width, .height = height, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth = device.CreateTexture({.format = rhi::Format::D32Float, .width = width, .height = height, .usage = rhi::TextureDepthStencil});
        Graphics::SetDevice(&device);
        Graphics::SetTarget({color, depth, width, height, rhi::Format::RGBA8Unorm, rhi::Format::D32Float});
        ReportScreen(i32(width), i32(height));
        // No pipeline: the engine's own drawing of the queued meshes. An orthographic camera two units tall.
        GameObject* cameraObject = GameObject::Create("Camera");
        Camera* camera = cameraObject->AddComponent<Camera>();
        camera->SetClearFlags(CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0, 0, 0, 1));
        camera->SetOrthographic(true);
        camera->SetOrthographicSize(1);
        cameraObject->transform()->SetPosition(Vector3(0, 0, -10));
        auto* config = static_cast<PanelInputConfiguration*>(cameraObject->AddComponent(scripting::TypeOf<PanelInputConfiguration>()));
        config->SetDefaultEventCameraIsMainCamera(false);
        Camera* cameras[] = {camera};
        config->SetEventCameras(Span<Camera* const>(cameras, 1));
        auto* settings = static_cast<UIPanelSettings*>(ScriptableObject::CreateInstance(scripting::TypeOf<UIPanelSettings>()));
        settings->SetRenderMode(PanelRenderMode::WorldSpace);
        // The panel's 20 pixels are one unit (32 screen pixels), its center at the origin.
        GameObject* ui = GameObject::Create("World UI");
        ui->transform()->SetLocalScale(Vector3(0.05f, 0.05f, 0.05f));
        auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
        view->SetPanelSettings(settings);
        view->SetSource(LoadAt<UITemplate>("world.hxml"));
        engine::InputFeed::Reset();
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(), Span<const u32>());
        view->Update();
        HX_REQUIRE(view->panel() != nullptr);
        bool deep = false;
        for (const ui::UIVertex& v : view->uiMesh().Vertices())
            deep = deep || std::fabs(v.z) > 0.5f;
        HX_CHECK(deep);
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        // The panel's top left (its red corner) up and left on the screen, its green rest down and right.
        HX_CHECK(NearColor(Pixel(pixels, width, 20, 20), Rgba(255, 0, 0)) && NearColor(Pixel(pixels, width, 42, 42), Rgba(0, 255, 0)));
        HX_CHECK(NearColor(Pixel(pixels, width, 8, 8), Rgba(0, 0, 0)) && NearColor(Pixel(pixels, width, 56, 56), Rgba(0, 0, 0)));
        // A press on the red corner: the ray finds it.
        ElementHandle root = view->rootElement()->handle();
        ElementTree& tree = view->panel()->tree();
        ElementHandle corner = tree.FirstChild(tree.FirstChild(root));
        HX_CHECK(view->Pick(Vector2(20, 20)) == corner && view->Pick(Vector2(8, 8)) == ElementHandle());
        Counts counts;
        view->panel()->events().RegisterCallback(corner, Events().click, [](void* u, EventSystem&, UIEvent&) { static_cast<Counts*>(u)->clicks++; }, &counts);
        const engine::InputEvent click[] = {Mouse(platform::EventType::MouseDown, 20, 20), Mouse(platform::EventType::MouseUp, 20, 20)};
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(click, 2), Span<const u32>());
        view->Update();
        HX_CHECK(counts.clicks == 1);
        // World space input off: nothing reaches it.
        config->SetProcessWorldSpaceInput(false);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(click, 2), Span<const u32>());
        view->Update();
        HX_CHECK(counts.clicks == 1);
        engine::InputFeed::Reset();
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}

namespace
{
    constexpr StringView kControlsPage = R"(<HXML>
  <Element name="page" style="width: 400px;">
    <Label name="label" text="Hello"/>
    <Button name="button" text="Press"/>
    <RepeatButton name="repeat" text="Hold" delay="100" interval="50"/>
    <Toggle name="toggle" label="Toggle" text="On"/>
    <GroupBox name="group" text="Group">
      <RadioButton name="r1" label="One" value="true"/>
      <RadioButton name="r2" label="Two"/>
    </GroupBox>
    <RadioButtonGroup name="radios" label="Radios" choices="A, B, C" value="1"/>
    <Slider name="slider" label="Slider" low-value="0" high-value="100" value="25" style="width: 300px;"/>
    <SliderInt name="sliderInt" low-value="0" high-value="10" value="3"/>
    <MinMaxSlider name="minmax" min-value="2" max-value="8" low-limit="0" high-limit="10"/>
    <TextField name="text" label="Text" value="ab"/>
    <IntegerField name="integer" label="Integer" value="42"/>
    <FloatField name="float" label="Float" value="1.5"/>
    <Vector3Field name="vector" label="Vector"/>
    <Hash128Field name="hash"/>
    <DropdownField name="dropdown" label="Dropdown" choices="Red, Green, Blue" index="0"/>
    <EnumField name="enum"/>
    <MaskField name="mask"/>
    <ProgressBar name="progress" value="40" title="Loading"/>
    <HelpBox name="help" text="Careful" message-type="Warning"/>
    <Foldout name="foldout" text="More" value="false"><Label text="Inside"/></Foldout>
    <ScrollView name="scroll" style="height: 40px;"><Element style="height: 200px;"/></ScrollView>
    <TabView name="tabs"><Tab label="First"><Label text="1"/></Tab><Tab label="Second"><Label text="2"/></Tab></TabView>
    <TwoPaneSplitView fixed-pane-initial-dimension="80" style="height: 40px;"><Element/><Element/></TwoPaneSplitView>
    <ToggleButtonGroup name="tbg" is-multiple-selection="true"><Button text="a"/><Button text="b"/></ToggleButtonGroup>
    <PopupWindow text="Window"><Label text="In it"/></PopupWindow>
    <Box/><Image/>
  </Element>
</HXML>)";
}

HX_TEST(UIControls, TheDefaultThemeParsesAndEveryControlBuildsAndTakesInput)
{
    HX_REQUIRE(RegisterModules());
    HX_REQUIRE(DefaultThemeSheet() != nullptr);
    for (const StyleError& e : DefaultThemeSheet()->Errors())
        std::printf("    theme %u:%u: %s\n", e.line, e.column, String(e.message).CStr());
    HX_CHECK(DefaultThemeSheet()->Errors().Empty());
    AssetFolder folder;
    folder.Write("controls.hxml", kControlsPage);
    folder.Open();
    ReportScreen(800, 1200);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    view->SetSource(LoadAt<UITemplate>("controls.hxml"));
    for (const HxmlError& e : view->errors())
        std::printf("    %u:%u: %s\n", e.line, e.column, String(e.message).CStr());
    HX_CHECK(view->errors().Empty());
    engine::InputFeed::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    frame({});
    frame({});
    Element* root = view->rootElement();
    ElementTree& tree = view->panel()->tree();
    LayoutEngine& layout = view->panel()->layout();
    auto find = [&](StringView name) {
        ElementHandle found;
        tree.ForEachInSubtree(root->handle(), [&](ElementHandle e, u32) {
            if (!found.IsValid() && tree.Name(e) == Intern(name))
                found = e;
        });
        return found;
    };
    auto center = [&](ElementHandle e) {
        Rect r = layout.WorldRect(e);
        return Vector2(r.x + r.width * 0.5f, r.y + r.height * 0.5f);
    };
    auto click = [&](Vector2 at) {
        frame({Mouse(platform::EventType::MouseMove, at.x, at.y)});
        frame({Mouse(platform::EventType::MouseDown, at.x, at.y), Mouse(platform::EventType::MouseUp, at.x, at.y)});
    };
    // A label's text, a button's click.
    HX_CHECK(tree.Text(find("label")) == StringView("Hello") && tree.HasClass(find("label"), Intern("helix-label")));
    auto* button = static_cast<Button*>(Element::Of(find("button")));
    HX_REQUIRE(button != nullptr);
    click(center(find("button")));
    HX_CHECK(button->clickCount() == 1);
    // A toggle turns on; the radio buttons of a group are exclusive.
    auto* toggle = static_cast<Toggle*>(Element::Of(find("toggle")));
    click(center(find("toggle")));
    HX_CHECK(toggle->IntegerInternal(0) == 1 && Any(tree.States(find("toggle")) & PseudoStates::Checked));
    auto* r1 = static_cast<RadioButton*>(Element::Of(find("r1")));
    auto* r2 = static_cast<RadioButton*>(Element::Of(find("r2")));
    HX_CHECK(r1->IntegerInternal(0) == 1);
    click(center(find("r2")));
    HX_CHECK(r2->IntegerInternal(0) == 1 && r1->IntegerInternal(0) == 0);
    auto* radios = static_cast<RadioButtonGroup*>(Element::Of(find("radios")));
    HX_CHECK(radios->IntegerInternal(0) == 1 && radios->ChoicesInternal() == StringView("A\nB\nC"));
    // A slider: a press near its tracker's end takes the value there.
    auto* slider = static_cast<Slider*>(Element::Of(find("slider")));
    HX_CHECK(slider->NumberInternal(0) == 25);
    Rect sr = layout.WorldRect(find("slider"));
    click(Vector2(sr.x + sr.width - 2, sr.y + sr.height * 0.5f));
    HX_CHECK(slider->NumberInternal(0) > 90);
    // Typing into the text field: a click selects all of it (selectAllOnMouseUp), what is typed replaces it.
    auto* text = static_cast<TextField*>(Element::Of(find("text")));
    HX_CHECK(text->StringInternal() == StringView("ab"));
    click(center(find("text")));
    engine::InputEvent typed[2];
    typed[0].event.type = platform::EventType::Text;
    typed[0].event.text = {'c'};
    typed[1].event.type = platform::EventType::Text;
    typed[1].event.text = {'d'};
    frame({typed[0], typed[1]});
    HX_CHECK(text->StringInternal() == StringView("cd") && text->TextOptionInternal(AbstractBaseField::CursorIndex) == 2);
    // Numbers: their text.
    auto* integer = static_cast<IntegerField*>(Element::Of(find("integer")));
    HX_CHECK(integer->IntegerInternal(0) == 42 && integer->TextInternal() == StringView("42"));
    auto* floating = static_cast<FloatField*>(Element::Of(find("float")));
    HX_CHECK(floating->NumberInternal(0) == 1.5);
    // The dropdown: its menu opens over the panel, a pick sets it.
    auto* dropdown = static_cast<DropdownField*>(Element::Of(find("dropdown")));
    HX_CHECK(dropdown->IntegerInternal(0) == 0);
    click(center(find("dropdown")));
    frame({});
    ElementHandle menuItem;
    tree.ForEachInSubtree(view->panel()->root(), [&](ElementHandle e, u32) {
        if (tree.HasClass(e, Intern("helix-base-dropdown__item")))
            menuItem = e;
    });
    HX_REQUIRE(menuItem.IsValid());
    click(center(menuItem));
    HX_CHECK(dropdown->IntegerInternal(0) == 2 && dropdown->StringInternal() == StringView("Blue"));
    // A foldout opens.
    auto* foldout = static_cast<Foldout*>(Element::Of(find("foldout")));
    HX_CHECK(!foldout->value());
    click(center(foldout->toggle()->handle()));
    HX_CHECK(foldout->value());
    // Tabs: the second header picks the second tab.
    auto* tabs = static_cast<TabView*>(Element::Of(find("tabs")));
    frame({});
    HX_REQUIRE(tabs->GetTab(1) != nullptr);
    click(center(tabs->GetTabHeader(1)->handle()));
    HX_CHECK(tabs->selectedTabIndex() == 1);
    // A toggle button group of several.
    auto* tbg = static_cast<ToggleButtonGroup*>(Element::Of(find("tbg")));
    frame({});
    Element* container = tbg->contentContainer();
    HX_REQUIRE(container != nullptr && tree.FirstChild(container->handle()).IsValid());
    click(center(tree.FirstChild(container->handle())));
    click(center(tree.NextSibling(tree.FirstChild(container->handle()))));
    HX_CHECK(tbg->IntegerInternal(0) == 3);
    // The scroll view: the wheel moves its content.
    auto* scroll = static_cast<ScrollView*>(Element::Of(find("scroll")));
    frame({});
    Vector2 sc = center(find("scroll"));
    engine::InputEvent wheel;
    wheel.event.type = platform::EventType::MouseWheel;
    wheel.event.wheel.dy = -2;
    frame({Mouse(platform::EventType::MouseMove, sc.x, sc.y)});
    frame({wheel});
    HX_CHECK(scroll->scrollOffset().y > 10);
    auto* progress = static_cast<ProgressBar*>(Element::Of(find("progress")));
    HX_CHECK(progress->title() == StringView("Loading") && progress->value() == 40);
    auto* help = static_cast<HelpBox*>(Element::Of(find("help")));
    HX_CHECK(help->messageType() == HelpBoxMessageType::Warning && help->text() == StringView("Careful"));
    engine::InputFeed::Reset();
}

HX_TEST(UIScripts, CSharpControlsSendTheirChangeEventsAndClicks)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ui::testing::Notes().Clear();
    AssetFolder folder;
    folder.Write("cs-controls.hxml", R"(<HXML><Label name="label" text="Hi"/><DropdownField name="dropdown" choices="x, y, z" index="0"/></HXML>)");
    folder.Open();
    ReportScreen(400, 600);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("cs-controls.hxml"));
    engine::InputFeed::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    frame({});
    ui->AddComponent(ui::testing::CSharp("CsControlTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"toggle:False:True", "toggle-state:False:on:Sound:True", "slider:0:10", "slider-state:10:2:10", "int:0:7:7",
                                    "vector:1:2:3", "text::Heli", "text-state:Heli:Heli:4", "popup:20:30:2", "enum:Pear:Plum", "mask:5",
                                    "foldout:True:False", "hash:True:True", "markup:Hi:x:0:x|y|z", "dropdown:x:z"}));
    // The C# button clicked by the pointer: its clicked handler runs.
    frame({});
    frame({Mouse(platform::EventType::MouseMove, 20, 10)});
    frame({Mouse(platform::EventType::MouseDown, 20, 10), Mouse(platform::EventType::MouseUp, 20, 10)});
    HX_CHECK(ui::testing::NotesAre({"clicked"}));
    RunScriptPhase(ScriptMessage::Update);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"button:1"}));
    engine::InputFeed::Reset();
    DestroyImmediate(ui);
}

namespace
{
    // The engine's clock moved by the tests (the time of the frames they run).
}

HX_TEST(UIControls, ATooltipShowsAfterTheMouseRestsOnItsElement)
{
    HX_REQUIRE(RegisterModules());
    ManualClock clock;
    AssetFolder folder;
    folder.Write("tooltip.hxml", R"(<HXML><Button name="save" text="Save" tooltip="Saves the file" style="width: 100px; height: 30px;"/></HXML>)");
    folder.Open();
    ReportScreen(400, 300);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    view->SetSource(LoadAt<UITemplate>("tooltip.hxml"));
    engine::InputFeed::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::Time::BeginFrame();
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    ElementTree& tree = view->panel()->tree();
    auto tips = [&] {
        ElementHandle found;
        tree.ForEachInSubtree(view->panel()->root(), [&](ElementHandle e, u32) {
            if (tree.HasClass(e, Intern("helix-tooltip")))
                found = e;
        });
        return found;
    };
    frame({});
    frame({Mouse(platform::EventType::MouseMove, 20, 10)});
    frame({});
    HX_CHECK(!tips().IsValid());
    clock.now += 0.6;
    frame({});
    ElementHandle tip = tips();
    HX_REQUIRE(tip.IsValid());
    HX_CHECK(tree.Text(tip) == StringView("Saves the file"));
    // Leaving the element takes it away.
    frame({Mouse(platform::EventType::MouseMove, 300, 250)});
    frame({});
    HX_CHECK(!tips().IsValid());
    engine::InputFeed::Reset();
    DestroyImmediate(ui);
}

HX_TEST(UIScripts, CSharpCollectionsMenusAnimationsPaintingAndBindings)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ui::testing::Notes().Clear();
    ManualClock clock;
    AssetFolder folder;
    folder.Write("collections.hxml", R"(<HXML><Element name="page" style="width: 400px;">)"
                                     R"(<Label name="marked"><Bindings><DataBinding property="text" data-source-path="level" binding-mode="ToTarget"/></Bindings></Label>)"
                                     R"(</Element></HXML>)");
    folder.Open();
    ReportScreen(800, 1200);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("collections.hxml"));
    for (const HxmlError& e : view->errors())
        std::printf("    %u:%u: %s\n", e.line, e.column, String(e.message).CStr());
    HX_CHECK(view->errors().Empty());
    engine::InputFeed::Reset();
    auto frame = [&] {
        clock.now += 0.01;
        engine::Time::BeginFrame();
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(), Span<const u32>());
        view->Update();
    };
    frame();
    ui->AddComponent(ui::testing::CSharp("CsCollectionTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"selected:item 2", "tree:2", "expanded:4:apple", "menu:2:Disabled", "generic:2", "easing:0.25:0.25:0.75",
                                    "vector:True:True:True", "path:4:2:a.b[2].c"}));
    for (int i = 0; i < 2; i++)
    {
        frame();
        RunScriptPhase(ScriptMessage::Update);
    }
    HX_CHECK(ui::testing::NotesAre({"rows:True:True:True", "scrolled:True", "sorted:a", "bound:first:3", "marked:50%"}));
    // The animation runs out in 50 ms.
    clock.now += 0.08;
    for (int i = 0; i < 2; i++)
    {
        frame();
        RunScriptPhase(ScriptMessage::Update);
    }
    HX_CHECK(ui::testing::NotesAre({"bound:second:9", "animated:True:10", "painted:True"}));
    engine::InputFeed::Reset();
    DestroyImmediate(ui);
}

HX_TEST(UIScripts, CSharpImguiContainerDrawsAndHearsItsElementsEvents)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ui::testing::Notes().Clear();
    ManualClock clock;
    AssetFolder folder;
    folder.Write("imgui.hxml", R"(<HXML><Element name="page" style="width: 400px; height: 300px;"/></HXML>)");
    folder.Open();
    ReportScreen(800, 600);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    HX_REQUIRE(view != nullptr);
    view->SetSource(LoadAt<UITemplate>("imgui.hxml"));
    engine::InputFeed::Reset();
    auto frame = [&] {
        clock.now += 0.01;
        engine::Time::BeginFrame();
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(), Span<const u32>());
        view->Update();
    };
    frame();
    ui->AddComponent(ui::testing::CSharp("CsImguiContainerTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(ui::testing::NotesAre({"class:True"}));
    for (int i = 0; i < 3; i++)
    {
        frame();
        RunScriptPhase(ScriptMessage::Update);
    }
    HX_CHECK(ui::testing::NotesAre({"repainted:True:True", "layout:True:True", "pressed", "disposed"}));
    engine::InputFeed::Reset();
    DestroyImmediate(ui);
}

HX_TEST(UIScripts, CppScriptsBuildControlsAndHearTheirEvents)
{
    HX_REQUIRE(RegisterModules());
    ui::testing::CppNotes().Clear();
    AssetFolder folder;
    folder.Write("cpp-controls.hxml", R"(<HXML><Element name="page"/></HXML>)");
    folder.Open();
    ReportScreen(400, 600);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    view->SetSource(LoadAt<UITemplate>("cpp-controls.hxml"));
    engine::InputFeed::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    frame({});
    ui->AddComponent(scripting::TypeOf<ui::testing::CppControlScript>());
    RunScriptPhase(ScriptMessage::Update);
    for (const String& n : ui::testing::CppNotes())
        std::printf("    %s\n", n.CStr());
    HX_CHECK(ui::testing::CppNotes().Size() == 3 && ui::testing::CppNotes()[0] == StringView("cpp-slider-changed") && ui::testing::CppNotes()[1] == StringView("cpp-foldout-changed") &&
             ui::testing::CppNotes()[2] == StringView("cpp-values:40:Helix:7:false:false"));
    ui::testing::CppNotes().Clear();
    frame({});
    frame({});
    ElementTree& tree = view->panel()->tree();
    LayoutEngine& layout = view->panel()->layout();
    auto find = [&](StringView name) {
        ElementHandle found;
        tree.ForEachInSubtree(view->panel()->root(), [&](ElementHandle e, u32) {
            if (!found.IsValid() && tree.Name(e) == Intern(name))
                found = e;
        });
        return found;
    };
    auto click = [&](ElementHandle e) {
        Rect r = layout.WorldRect(e);
        Vector2 at(r.x + 12, r.y + r.height * 0.5f);
        frame({Mouse(platform::EventType::MouseMove, at.x, at.y)});
        frame({Mouse(platform::EventType::MouseDown, at.x, at.y), Mouse(platform::EventType::MouseUp, at.x, at.y)});
    };
    HX_REQUIRE(find("cpp-button").IsValid() && find("cpp-toggle").IsValid());
    click(find("cpp-button"));
    click(find("cpp-toggle"));
    for (const String& n : ui::testing::CppNotes())
        std::printf("    %s\n", n.CStr());
    HX_CHECK(ui::testing::CppNotes().Size() == 2 && ui::testing::CppNotes()[0] == StringView("cpp-clicked") && ui::testing::CppNotes()[1] == StringView("cpp-toggle-changed"));
    engine::InputFeed::Reset();
    DestroyImmediate(ui);
}

namespace
{
    constexpr StringView kBehaviourPage = R"(<HXML>
  <Element name="page" style="width: 360px;">
    <RepeatButton name="repeat" text="Hold" delay="100" interval="50"/>
    <SliderInt name="sliderInt" low-value="0" high-value="10" value="3" style="width: 300px;"/>
    <MinMaxSlider name="minmax" min-value="2" max-value="8" low-limit="0" high-limit="10" style="width: 300px;"/>
    <Foldout name="foldout" text="More" value="false"><Label name="inside" text="Inside"/></Foldout>
    <TabView name="tabs"><Tab name="first" label="First"><Label text="1"/></Tab><Tab name="second" label="Second"><Label text="2"/></Tab></TabView>
    <TwoPaneSplitView name="split" fixed-pane-initial-dimension="80" style="height: 40px;"><Element name="left"/><Element name="right"/></TwoPaneSplitView>
    <IntegerField name="integer" label="Integer" value="10"/>
    <Vector3Field name="vector" label="Vector" x="1" y="2" z="3"/>
    <Hash128Field name="hash"/>
    <Mask64Field name="mask64" choices="A, B, C" value="5"/>
    <TextField name="limited" label="Limited" max-length="3"/>
    <TextField name="secret" label="Secret" password="true" value="abc"/>
    <TextField name="lines" label="Lines" multiline="true"/>
    <ScrollView name="scroll" style="height: 40px;"><Element style="height: 200px;"/></ScrollView>
  </Element>
</HXML>)";
}

HX_TEST(UIControls, EachControlBehavesAsTheReferenceDocumentsIt)
{
    HX_REQUIRE(RegisterModules());
    ManualClock clock;
    AssetFolder folder;
    folder.Write("behaviour.hxml", kBehaviourPage);
    folder.Open();
    ReportScreen(400, 900);
    GameObject* ui = GameObject::Create("UI");
    auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
    view->SetSource(LoadAt<UITemplate>("behaviour.hxml"));
    for (const HxmlError& e : view->errors())
        std::printf("    %u:%u: %s\n", e.line, e.column, String(e.message).CStr());
    HX_CHECK(view->errors().Empty());
    engine::InputFeed::Reset();
    auto frame = [&](std::initializer_list<engine::InputEvent> events) {
        clock.now += 1.0 / 60.0;
        engine::Time::BeginFrame();
        Array<engine::InputEvent> list;
        for (const engine::InputEvent& e : events)
            list.Push(e);
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        view->Update();
    };
    frame({});
    frame({});
    ElementTree& tree = view->panel()->tree();
    LayoutEngine& layout = view->panel()->layout();
    auto find = [&](StringView name) {
        ElementHandle found;
        tree.ForEachInSubtree(view->panel()->root(), [&](ElementHandle e, u32) {
            if (!found.IsValid() && tree.Name(e) == Intern(name))
                found = e;
        });
        return found;
    };
    auto rect = [&](StringView name) { return layout.WorldRect(find(name)); };
    auto click = [&](Vector2 at) {
        frame({Mouse(platform::EventType::MouseMove, at.x, at.y)});
        frame({Mouse(platform::EventType::MouseDown, at.x, at.y), Mouse(platform::EventType::MouseUp, at.x, at.y)});
    };
    auto type = [&](StringView text) {
        Array<engine::InputEvent> list;
        for (char c : text)
        {
            engine::InputEvent e;
            e.event.type = platform::EventType::Text;
            e.event.text = {u32(u8(c))};
            list.Push(e);
        }
        engine::InputFeed::SetFrame(Span<const engine::InputEvent>(list.Data(), list.Size()), Span<const u32>());
        clock.now += 1.0 / 60.0;
        engine::Time::BeginFrame();
        view->Update();
    };

    // RepeatButton: a click on the press, again after the delay, then every interval while held.
    auto* repeat = static_cast<RepeatButton*>(Element::Of(find("repeat")));
    Rect rr = rect("repeat");
    Vector2 rc(rr.x + rr.width * 0.5f, rr.y + rr.height * 0.5f);
    frame({Mouse(platform::EventType::MouseMove, rc.x, rc.y)});
    frame({Mouse(platform::EventType::MouseDown, rc.x, rc.y)});
    HX_CHECK(repeat->clickCount() == 1);
    for (u32 i = 0; i < 12; i++)
        frame({});
    i32 held = repeat->clickCount();
    frame({Mouse(platform::EventType::MouseUp, rc.x, rc.y)});
    for (u32 i = 0; i < 6; i++)
        frame({});
    std::printf("    repeat clicks after 0.2 s: %d\n", held);
    HX_CHECK(held >= 3 && held <= 4 && repeat->clickCount() == held);

    // SliderInt: whole values only.
    auto* sliderInt = static_cast<SliderInt*>(Element::Of(find("sliderInt")));
    HX_CHECK(sliderInt->NumberInternal(0) == 3);
    Rect sr = rect("sliderInt");
    click(Vector2(sr.x + sr.width * 0.73f, sr.y + sr.height * 0.5f));
    f64 whole = sliderInt->NumberInternal(0);
    HX_CHECK(whole > 3 && whole == std::floor(whole));

    // MinMaxSlider: its two values inside its limits.
    auto* minmax = static_cast<MinMaxSlider*>(Element::Of(find("minmax")));
    HX_CHECK(minmax->NumberInternal(0) == 2 && minmax->NumberInternal(1) == 8);

    // Foldout: a click on its toggle opens it, its content shows.
    auto* foldout = static_cast<Foldout*>(Element::Of(find("foldout")));
    HX_CHECK(!foldout->value());
    Rect fr = rect("foldout");
    click(Vector2(fr.x + 10, fr.y + 8));
    frame({});
    HX_CHECK(foldout->value() && layout.WorldRect(find("inside")).height > 0);

    // TabView: a click on a tab's header picks it.
    auto* tabs = static_cast<TabView*>(Element::Of(find("tabs")));
    HX_CHECK(tabs->selectedTabIndex() == 0);
    auto* second = static_cast<Tab*>(Element::Of(find("second")));
    Rect hr = layout.WorldRect(second->tabHeader()->handle());
    click(Vector2(hr.x + hr.width * 0.5f, hr.y + hr.height * 0.5f));
    HX_CHECK(tabs->selectedTabIndex() == 1);

    // TwoPaneSplitView: the fixed pane at its initial size.
    HX_CHECK(std::fabs(rect("left").width - 80) <= 1);

    // IntegerField: a drag on its label changes the value.
    auto* integer = static_cast<IntegerField*>(Element::Of(find("integer")));
    Rect lr = layout.WorldRect(integer->labelElement()->handle());
    Vector2 lc(lr.x + 6, lr.y + lr.height * 0.5f);
    frame({Mouse(platform::EventType::MouseMove, lc.x, lc.y)});
    frame({Mouse(platform::EventType::MouseDown, lc.x, lc.y)});
    frame({Mouse(platform::EventType::MouseMove, lc.x + 40, lc.y)});
    frame({Mouse(platform::EventType::MouseUp, lc.x + 40, lc.y)});
    HX_CHECK(integer->IntegerInternal(0) > 10);

    // Vector3Field: its components from markup.
    auto* vector = static_cast<Vector3Field*>(Element::Of(find("vector")));
    HX_CHECK(vector->NumberInternal(0) == 1 && vector->NumberInternal(1) == 2 && vector->NumberInternal(2) == 3);

    // Hash128Field: a hash typed in is the value; text that is no hash is not.
    auto* hash = static_cast<Hash128Field*>(Element::Of(find("hash")));
    hash->SetTextInternal("0123456789abcdef0123456789abcdef");
    String hashText = hash->TextInternal();
    HX_CHECK(hashText == StringView("0123456789abcdef0123456789abcdef"));

    // Mask64Field: the bits of its value.
    auto* mask64 = static_cast<Mask64Field*>(Element::Of(find("mask64")));
    HX_CHECK(mask64->IntegerInternal(0) == 5);

    // TextField: max length cuts what is typed; a password shows its mask; multiline keeps lines.
    auto* limited = static_cast<TextField*>(Element::Of(find("limited")));
    Rect tr = rect("limited");
    click(Vector2(tr.x + tr.width - 20, tr.y + tr.height * 0.5f));
    type("abcdef");
    HX_CHECK(limited->StringInternal() == StringView("abc"));
    auto* secret = static_cast<TextField*>(Element::Of(find("secret")));
    HX_CHECK(secret->TextOptionInternal(AbstractBaseField::Password) == 1 && secret->StringInternal() == StringView("abc"));
    auto* lines = static_cast<TextField*>(Element::Of(find("lines")));
    lines->SetStringInternal("one\ntwo");
    HX_CHECK(lines->StringInternal() == StringView("one\ntwo"));

    // ScrollView: the wheel scrolls by its wheel size, within its content.
    auto* scroll = static_cast<ScrollView*>(Element::Of(find("scroll")));
    Rect cr = rect("scroll");
    engine::InputEvent wheel;
    wheel.event.type = platform::EventType::MouseWheel;
    wheel.event.wheel.dy = -40;
    frame({Mouse(platform::EventType::MouseMove, cr.x + 10, cr.y + 10)});
    frame({wheel});
    frame({});
    HX_CHECK(scroll->scrollOffset().y > 0 && scroll->scrollOffset().y <= 160 + 1);
    engine::InputFeed::Reset();
    DestroyImmediate(ui);
}


HX_TEST(UIControls, EveryControlDrawsTheSameOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    ManualClock clock;
    constexpr u32 width = 400, height = 760;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        AssetFolder folder;
        Array<u8> font;
        folder.Folder("Fonts");
        folder.Write("Fonts/Page.ttf", PageFont(font));
        String page(kControlsPage);
        // The page in the test font (the theme names none).
        String styled("<HXML><Style src=\"font.hss\"/>");
        styled += page.View().Substr(6);
        folder.Write("controls.hxml", styled.View());
        folder.Write("font.hss", R"(:root, * { -helix-font-definition: url("Fonts/Page.ttf"); })");
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
        camera->SetBackgroundColor(Color(0.16f, 0.17f, 0.2f, 1));
        GameObject* ui = GameObject::Create("UI");
        auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
        view->SetSource(LoadAt<UITemplate>("controls.hxml"));
        // The parts that follow their geometry (tab headers, split panes) settle within a few frames.
        for (u32 k = 0; k < 6; k++)
        {
            clock.now += 1.0 / 60.0;
            engine::Time::BeginFrame();
            view->Update();
        }
        HX_CHECK(view->errors().Empty());
        Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(engine::testing::MatchGolden("UIControlsPage", pixels.Data(), width, height, 8));
        DestroyImmediate(ui);
        DestroyImmediate(cameraObject);
        Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}

HX_TEST(UIControls, TheControlsPageBuiltAgainLaysOutTheSame)
{
    HX_REQUIRE(RegisterModules());
    ManualClock clock;
    AssetFolder folder;
    folder.Write("again3.hxml", kControlsPage);
    folder.Open();
    ReportScreen(400, 760);
    Array<u32> counts;
    Array<f32> heights;
    for (u32 round = 0; round < 3; round++)
    {
        GameObject* ui = GameObject::Create("UI");
        auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
        view->SetSource(LoadAt<UITemplate>("again3.hxml"));
        for (u32 k = 0; k < 6; k++)
        {
            clock.now += 1.0 / 60.0;
            engine::Time::BeginFrame();
            view->Update();
        }
        u32 n = 0;
        view->panel()->tree().ForEachInSubtree(view->rootElement()->handle(), [&](ElementHandle, u32) { n++; });
        Rect page = view->panel()->layout().WorldRect(view->panel()->tree().FirstChild(view->rootElement()->handle()));
        std::printf("    round %u: %u elements, page %.1f high, %u vertices\n", round, n, f64(page.height), u32(view->uiMesh().Vertices().Size()));
        counts.Push(n);
        heights.Push(page.height);
        DestroyImmediate(ui);
    }
    HX_CHECK(counts[0] == counts[1] && counts[1] == counts[2] && heights[0] == heights[1] && heights[1] == heights[2]);
}

// With the C# side up, script held elements outlive their page: the page built again must not share slots with them
// (a destroy whose objects destroyed their parts freed those slots twice).
HX_TEST(UIControls, TheControlsPageBuiltAgainWithScriptsLaysOutTheSame)
{
    HX_REQUIRE(ui::testing::StartScripts());
    ManualClock clock;
    AssetFolder folder;
    folder.Write("again2.hxml", kControlsPage);
    folder.Open();
    ReportScreen(400, 760);
    Array<u32> counts;
    Array<f32> heights;
    for (u32 round = 0; round < 3; round++)
    {
        GameObject* ui = GameObject::Create("UI");
        auto* view = static_cast<UIView*>(ui->AddComponent(scripting::TypeOf<UIView>()));
        view->SetSource(LoadAt<UITemplate>("again2.hxml"));
        for (u32 k = 0; k < 6; k++)
        {
            clock.now += 1.0 / 60.0;
            engine::Time::BeginFrame();
            view->Update();
        }
        u32 n = 0;
        view->panel()->tree().ForEachInSubtree(view->rootElement()->handle(), [&](ElementHandle, u32) { n++; });
        Rect page = view->panel()->layout().WorldRect(view->panel()->tree().FirstChild(view->rootElement()->handle()));
        std::printf("    round %u: %u elements, page %.1f high, %u vertices\n", round, n, f64(page.height), u32(view->uiMesh().Vertices().Size()));
        counts.Push(n);
        heights.Push(page.height);
        DestroyImmediate(ui);
    }
    HX_CHECK(counts[0] == counts[1] && counts[1] == counts[2] && heights[0] == heights[1] && heights[1] == heights[2]);
}
