#pragma once

#include <Engine/Support/UIHost.h>

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Platform/IO/FileSystem.h>

#include <Support/FontWriter.h>
#include <UIShadersAssembly.h>

// What the UI's engine tests share: a folder of assets with the UI's shaders, the screen's size, pixels read back,
// time by hand and a font that draws the same everywhere.
namespace helix::ui::testing
{
    using namespace helix::engine;

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

    inline void ReportScreen(i32 width, i32 height)
    {
        ScreenReport r = Screen::Reported();
        r.width = width;
        r.height = height;
        Screen::Report(r);
    }

    inline u32 Pixel(const Array<u32>& pixels, u32 width, u32 x, u32 y) { return pixels[y * width + x]; }

    inline bool NearColor(u32 a, u32 b, i32 tolerance = 3)
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


    struct ManualClock : engine::TimeClock
    {
        double now = 1;
        double Now() override { return now; }

        ManualClock()
        {
            engine::Time::SetClock(this);
            engine::Time::Reset();
            engine::Time::BeginFrame();
        }

        ~ManualClock()
        {
            engine::Time::SetClock(nullptr);
            engine::Time::Reset();
        }
    };

    // Every printable ASCII character a bar of its own height: text drawn the same everywhere (no machine's fonts).
    inline StringView PageFont(Array<u8>& bytes)
    {
        using namespace font::testing;
        FontSpec s;
        s.glyphs.Resize(2 + 94);
        s.glyphs[1].advance = 250;
        s.cmap.Push({u32(' '), 1});
        for (u32 c = 33; c < 127; c++)
        {
            u32 g = 2 + (c - 33);
            s.glyphs[g].contours.Push(Square(60, 0, 420, i32(260 + (c % 9) * 55)));
            s.glyphs[g].advance = 480;
            s.cmap.Push({c, g});
        }
        s.names = {{String("1"), String("Page Test")}, {String("2"), String("Regular")}};
        bytes = BuildFont(s);
        return StringView(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    }
}
