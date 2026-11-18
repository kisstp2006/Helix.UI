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
        : TextAsset(other), renderMode_(other.renderMode_), samplingPointSize_(other.samplingPointSize_), atlasPadding_(other.atlasPadding_),
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

    // ------------------------------------------------------------------------------------------------ TextAsset

    namespace
    {
        // The reference's simple hash of names (the text system's lookups by name).
        i32 SimpleHash(StringView s)
        {
            i32 hash = 0;
            for (char c : s)
                hash = ((hash << 5) + hash) ^ i32(u8(c));
            return hash;
        }
    }

    i32 TextAsset::hashCode() const { return SimpleHash(Name()); }
    i32 TextAsset::materialHashCode() const { return material() ? SimpleHash(material()->Name()) : 0; }

    // ------------------------------------------------------------------------------------------------ the tables

    Array<GlyphData> FontAsset::GlyphTableInternal()
    {
        Array<GlyphData> out;
        TextFont* f = textFont();
        if (!f)
            return out;
        GlyphCache& cache = f->Cache();
        f32 unit = f32(cache.SamplingSize()) / f32(face_->UnitsPerEm() ? face_->UnitsPerEm() : 1000);
        i32 pad = i32(cache.Padding());
        for (auto e : cache.Entries())
        {
            const GlyphEntry& g = e.value;
            GlyphData d;
            d.index = e.key;
            d.scale = 1.0f / (g.scale > 0 ? g.scale : 1.0f);
            d.atlasIndex = i32(g.page);
            if (!g.empty)
            {
                d.glyphRect = GlyphRect{i32(g.x) + pad, i32(g.y) + pad, i32(g.width) - 2 * pad, i32(g.height) - 2 * pad};
                d.metrics.width = f32(g.width) - 2.0f * f32(pad);
                d.metrics.height = f32(g.height) - 2.0f * f32(pad);
                d.metrics.horizontalBearingX = f32(g.left + pad);
                d.metrics.horizontalBearingY = -f32(g.top + pad);
            }
            d.metrics.horizontalAdvance = f32(face_->Advance(e.key)) * unit;
            out.Push(d);
        }
        // In glyph order.
        for (usize i = 1; i < out.Size(); i++)
            for (usize j = i; j > 0 && out[j].index < out[j - 1].index; j--)
                std::swap(out[j], out[j - 1]);
        return out;
    }

    Array<CharacterData> FontAsset::CharacterTableInternal()
    {
        // The characters added, and those whose glyphs the text rendered (by the face's map of the printable ranges).
        Array<CharacterData> out = characters_;
        TextFont* f = textFont();
        if (!f)
            return out;
        auto known = [&](u32 c) {
            for (const CharacterData& d : out)
                if (d.unicode == c)
                    return true;
            return false;
        };
        const HashMap<u32, GlyphEntry>& entries = f->Cache().Entries();
        for (u32 c = 0x20; c < 0x3000; c++)
        {
            u32 g = face_->Glyph(char32_t(c));
            if (g && entries.Find(g) && !known(c))
                out.Push(CharacterData{c, g, 1});
        }
        return out;
    }

    FaceInfoData FontAsset::FaceInfoInternal() const
    {
        FaceInfo i = faceInfo();
        FaceInfoData d;
        d.pointSize = i.pointSize;
        d.scale = i.scale;
        d.unitsPerEM = i.unitsPerEM;
        d.lineHeight = i.lineHeight;
        d.ascentLine = i.ascentLine;
        d.capLine = i.capLine;
        d.meanLine = i.meanLine;
        d.baseline = i.baseline;
        d.descentLine = i.descentLine;
        d.superscriptOffset = i.superscriptOffset;
        d.superscriptSize = i.superscriptSize;
        d.subscriptOffset = i.subscriptOffset;
        d.subscriptSize = i.subscriptSize;
        d.underlineOffset = i.underlineOffset;
        d.underlineThickness = i.underlineThickness;
        d.strikethroughOffset = i.strikethroughOffset;
        d.strikethroughThickness = i.strikethroughThickness;
        d.tabWidth = i.tabWidth;
        return d;
    }

    String FontAsset::FamilyNameInternal() const { return face_ ? String(face_->Family()) : String(); }
    String FontAsset::StyleNameInternal() const { return face_ ? String(face_->Style()) : String(); }

    u32 FontAsset::GlyphIndexInternal(u32 unicode) const { return face_ ? face_->Glyph(char32_t(unicode)) : 0; }

    Array<u32> FontAsset::TryAddCharactersInternal(Span<const u32> unicodes)
    {
        Array<u32> missing;
        TextFont* f = textFont();
        for (u32 c : unicodes)
        {
            u32 g = f ? face_->Glyph(char32_t(c)) : 0;
            if (!g)
            {
                missing.Push(c);
                continue;
            }
            f->Cache().Get(g);
            bool listed = false;
            for (const CharacterData& d : characters_)
                listed = listed || d.unicode == c;
            if (!listed)
                characters_.Push(CharacterData{c, g, 1});
        }
        return missing;
    }

    bool FontAsset::HasCharacterInternal(u32 unicode, bool searchFallbacks, bool tryAddCharacter)
    {
        if (face_ && face_->Has(char32_t(unicode)))
        {
            if (tryAddCharacter)
                TryAddCharactersInternal(Span<const u32>(&unicode, 1));
            return true;
        }
        if (searchFallbacks)
            for (const engine::PPtr<FontAsset>& f : fallbacks_)
                if (FontAsset* a = f.Get(); a && a != this && a->HasCharacterInternal(unicode, true, tryAddCharacter))
                    return true;
        return false;
    }

    void FontAsset::SetAtlasSizeInternal(i32 value)
    {
        if (atlasSize_ == value)
            return;
        atlasSize_ = value;
        Rebuild();
    }

    Array<FontAsset*> FontAsset::FallbacksInternal() const
    {
        Array<FontAsset*> out;
        for (const engine::PPtr<FontAsset>& f : fallbacks_)
            out.Push(f.Get());
        return out;
    }

    Array<engine::Texture2D*> FontAsset::AtlasTexturesInternal()
    {
        UpdateAtlasTextures();
        Array<engine::Texture2D*> out;
        for (const engine::PPtr<engine::Texture2D>& t : pages_)
            out.Push(t.Get());
        if (out.Empty())
            if (engine::Texture2D* first = AtlasTexture(0, false))
                out.Push(first);
        return out;
    }

    void FontAsset::ClearFontAssetData(bool setAtlasSizeToZero)
    {
        characters_.Clear();
        if (font_)
            font_->Cache().Clear();
        if (setAtlasSizeToZero)
        {
            for (engine::PPtr<engine::Texture2D>& t : pages_)
                if (engine::Texture2D* tex = t.Get())
                    engine::Object::DestroyImmediate(tex);
            pages_.Clear();
        }
        uploaded_ = ~0u;
    }

    FontAsset* FontAsset::WeightTypefaceInternal(i32 index, bool italic) const
    {
        return index >= 0 && index < 10 ? weights_[index][italic ? 1 : 0].Get() : nullptr;
    }

    void FontAsset::SetWeightTypefaceInternal(i32 index, bool italic, FontAsset* font)
    {
        if (index >= 0 && index < 10)
            weights_[index][italic ? 1 : 0] = engine::PPtr<FontAsset>(font);
    }
}
