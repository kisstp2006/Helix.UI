#include <Helix/UI/Engine/FontAsset.h>

#include <Helix/Engine/Asset/AssetLoader.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Scripting/Types/Type.h>
#include <Helix/UI/Text/Fonts/SystemFonts.h>
#include <Helix/UI/Text/Unicode/Unicode.h>

#include <cstring>

namespace helix::ui
{
    namespace
    {
        struct Registry
        {
            // Every font asset with a text font, to find the asset of a text font.
            Array<FontAsset*> assets;
            // The fonts of the machine made so far: "family/style" to the asset.
            Array<String> systemKeys;
            Array<engine::PPtr<FontAsset>> systemAssets;
            // Whether the machine had the face (an asset made of it that was destroyed is made again).
            Array<bool> systemFound;
        };

        Registry& R()
        {
            static Registry* r = new Registry();
            return *r;
        }
    }

    FontAsset::FontAsset(const FontAsset& other)
        : engine::ScriptableObject(other), renderMode_(other.renderMode_), samplingPointSize_(other.samplingPointSize_), atlasPadding_(other.atlasPadding_),
          atlasSize_(other.atlasSize_), fallbacks_(other.fallbacks_), face_(other.face_)
    {
        Rebuild();
    }

    FontAsset::~FontAsset()
    {
        Registry& r = R();
        for (usize i = 0; i < r.assets.Size(); i++)
            if (r.assets[i] == this)
            {
                r.assets.RemoveAt(i);
                break;
            }
        for (auto* list : {&pages_, &colorPages_})
            for (engine::PPtr<engine::Texture2D>& t : *list)
                if (engine::Texture2D* tex = t.Get())
                    engine::Object::DestroyImmediate(tex);
    }

    bool FontAsset::SetFontData(Span<const u8> file, u32 index, String& error)
    {
        auto face = std::make_shared<Face>();
        if (!face->Open(file, index, error))
            return false;
        face_ = face;
        Rebuild();
        return true;
    }

    void FontAsset::Rebuild()
    {
        for (auto* list : {&pages_, &colorPages_})
        {
            for (engine::PPtr<engine::Texture2D>& t : *list)
                if (engine::Texture2D* tex = t.Get())
                    engine::Object::DestroyImmediate(tex);
            list->Clear();
        }
        uploaded_ = ~0u;
        font_.reset();
        if (!face_)
            return;
        font_ = std::make_unique<TextFont>(face_, GlyphRenderMode(renderMode_), u32(samplingPointSize_ > 4 ? samplingPointSize_ : 4),
                                           u32(atlasPadding_ > 1 ? atlasPadding_ : 1), u32(atlasSize_ >= 64 ? atlasSize_ : 64));
        Registry& r = R();
        bool listed = false;
        for (FontAsset* a : r.assets)
            listed = listed || a == this;
        if (!listed)
            r.assets.Push(this);
    }

    void FontAsset::SetAtlasRenderMode(GlyphRenderMode mode)
    {
        if (renderMode_ == i32(mode))
            return;
        renderMode_ = i32(mode);
        Rebuild();
    }

    void FontAsset::SetSamplingPointSize(i32 value)
    {
        if (samplingPointSize_ == value)
            return;
        samplingPointSize_ = value;
        Rebuild();
    }

    void FontAsset::SetAtlasPadding(i32 value)
    {
        if (atlasPadding_ == value)
            return;
        atlasPadding_ = value;
        Rebuild();
    }

    FaceInfo FontAsset::faceInfo() const { return face_ ? FaceInfoOf(*face_, f32(samplingPointSize_)) : FaceInfo(); }

    void FontAsset::SetFallbackFontAssetTable(Span<FontAsset* const> fonts)
    {
        fallbacks_.Clear();
        for (FontAsset* f : fonts)
            fallbacks_.Push(engine::PPtr<FontAsset>(f));
    }

    TextFont* FontAsset::textFont()
    {
        if (!font_ && face_)
            Rebuild();
        if (!font_)
            return nullptr;
        font_->fallbacks.Clear();
        for (const engine::PPtr<FontAsset>& f : fallbacks_)
            if (FontAsset* a = f.Get(); a && a != this)
                if (TextFont* t = a->textFont())
                    font_->fallbacks.Push(t);
        return font_.get();
    }

    bool FontAsset::HasCharacter(i32 character, bool searchFallbacks)
    {
        if (face_ && face_->Has(char32_t(character)))
            return true;
        if (searchFallbacks)
            for (const engine::PPtr<FontAsset>& f : fallbacks_)
                if (FontAsset* a = f.Get(); a && a != this && a->HasCharacter(character, false))
                    return true;
        return false;
    }

    bool FontAsset::TryAddCharacters(StringView characters)
    {
        TextFont* f = textFont();
        if (!f)
            return false;
        bool all = true;
        for (usize at = 0; at < characters.Size();)
        {
            char32_t c = DecodeUtf8(characters, at);
            u32 g = face_->Glyph(c);
            all = all && g != 0;
            if (g)
                f->Cache().Get(g);
        }
        return all;
    }

    void FontAsset::SyncPages(font::FontAtlas& atlas, Array<engine::PPtr<engine::Texture2D>>& pages, bool color)
    {
        for (u32 p = 0; p < atlas.PageCount(); p++)
        {
            bool fresh = p >= pages.Size() || !pages[p].Get();
            if (p >= pages.Size())
                pages.Push(engine::PPtr<engine::Texture2D>());
            u32 x0, y0, x1, y1;
            if (!fresh && !atlas.Dirty(p, x0, y0, x1, y1))
                continue;
            engine::Texture2D* tex = pages[p].Get();
            if (!tex)
            {
                engine::TextureFormat format = atlas.Channels() == 1 ? engine::TextureFormat::R8 : engine::TextureFormat::RGBA32;
                tex = engine::Texture2D::Create(i32(atlas.PageWidth()), i32(atlas.PageHeight()), format, false, true);
                tex->hideFlags = engine::HideFlags::DontSave;
                tex->SetFilterMode(engine::FilterMode::Bilinear);
                tex->SetWrapMode(engine::TextureWrapMode::Clamp);
                pages[p] = engine::PPtr<engine::Texture2D>(tex);
            }
            usize size = usize(atlas.PageWidth()) * atlas.PageHeight() * atlas.Channels();
            tex->SetPixelData(Span<const u8>(atlas.PagePixels(p), size), 0);
            tex->Apply(false);
            (void)color;
        }
    }

    void FontAsset::UpdateAtlasTextures()
    {
        if (!font_ || font_->Cache().Version() == uploaded_)
            return;
        SyncPages(font_->Cache().Atlas(), pages_, false);
        SyncPages(font_->Cache().ColorAtlas(), colorPages_, true);
        font_->Cache().Atlas().ClearDirty();
        font_->Cache().ColorAtlas().ClearDirty();
        uploaded_ = font_->Cache().Version();
    }

    engine::Texture2D* FontAsset::AtlasTexture(u32 page, bool color)
    {
        UpdateAtlasTextures();
        Array<engine::PPtr<engine::Texture2D>>& pages = color ? colorPages_ : pages_;
        return page < pages.Size() ? pages[page].Get() : nullptr;
    }

    FontAsset* FontAsset::CreateFontAsset(StringView familyName, StringView styleName)
    {
        Registry& r = R();
        String key = String(familyName);
        key += "/";
        key += styleName;
        for (usize i = 0; i < r.systemKeys.Size(); i++)
            if (r.systemKeys[i] == key)
            {
                if (FontAsset* a = r.systemAssets[i].Get(); a || !r.systemFound[i])
                    return a;
                r.systemKeys.RemoveAt(i);
                r.systemAssets.RemoveAt(i);
                r.systemFound.RemoveAt(i);
                break;
            }
        FontAsset* asset = nullptr;
        if (const SystemFontFace* face = SystemFonts::Find(familyName, styleName))
        {
            Array<u8> bytes;
            String error;
            if (platform::ReadFile(face->path.View(), bytes))
            {
                asset = static_cast<FontAsset*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<FontAsset>()));
                if (asset)
                {
                    asset->hideFlags = engine::HideFlags::DontSave;
                    asset->SetName(String(familyName).View());
                    if (!asset->SetFontData(Span<const u8>(bytes.Data(), bytes.Size()), face->index, error))
                    {
                        engine::Object::DestroyImmediate(asset);
                        asset = nullptr;
                    }
                }
            }
        }
        r.systemKeys.Push(Move(key));
        r.systemAssets.Push(engine::PPtr<FontAsset>(asset));
        r.systemFound.Push(asset != nullptr);
        return asset;
    }

    FontAsset* FontAsset::Of(const TextFont* font)
    {
        for (FontAsset* a : R().assets)
            if (a->font_.get() == font)
                return a;
        return nullptr;
    }

    namespace
    {
        struct Decoded
        {
            Array<u8> bytes;
        };

        void* Decode(const engine::AssetDecodeInput& input, String&)
        {
            auto* d = new Decoded();
            d->bytes.Append(input.bytes.Data(), input.bytes.Size());
            return d;
        }

        bool Integrate(void* decoded, engine::AssetObjects& out, Array<String>& errors)
        {
            auto* d = static_cast<Decoded*>(decoded);
            auto* asset = static_cast<FontAsset*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<FontAsset>()));
            if (!asset)
            {
                errors.Push(String("the UI module's FontAsset class is not registered"));
                return false;
            }
            String error;
            if (!asset->SetFontData(Span<const u8>(d->bytes.Data(), d->bytes.Size()), 0, error))
                errors.Push(error);
            out.Add(engine::MainAssetFileId(engine::ClassId::MonoBehaviour), asset, true);
            return true;
        }
    }

    void RegisterFontAssetLoader()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;
        engine::AssetLoader l;
        l.name = "HelixUI.Fonts";
        for (const char* e : {"ttf", "otf", "ttc", "otc"})
            l.extensions.Push(String(e));
        l.decode = &Decode;
        l.integrate = &Integrate;
        l.release = [](void* d) { delete static_cast<Decoded*>(d); };
        engine::RegisterAssetLoader(l);
    }
}
