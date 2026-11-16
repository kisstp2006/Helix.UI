#pragma once

#include <Helix/Engine/Object/Object.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/UI/Text/Fonts/TextFont.h>

#include <memory>

namespace helix::engine
{
    class Texture2D;
}

// A font asset: a font file's face with its glyph atlases as textures, filled as text asks for glyphs (dynamic), and the
// fonts it falls back to. Font files (.ttf, .otf, .ttc, .otc) load as font assets; fonts of the machine are made into
// font assets by family and style.
namespace helix::ui
{
    HX_CLASS(Namespace = "HelixEngine.Text")
    class FontAsset : public engine::ScriptableObject
    {
    public:
        FontAsset() = default;
        // A copy (Instantiate) shares the face; its atlases start empty.
        FontAsset(const FontAsset& other);
        FontAsset& operator=(const FontAsset&) = delete;
        ~FontAsset() override;

        // The face `index` of a font file (the asset keeps the bytes); false with the reason when it cannot be read.
        bool SetFontData(Span<const u8> file, u32 index, String& error);
        bool HasFontData() const { return face_ != nullptr; }

        // How glyphs are rendered into the atlas, at which size, with what padding, into pages of what size (taking
        // effect for glyphs rendered after the change: the atlas starts anew).
        GlyphRenderMode atlasRenderMode() const { return GlyphRenderMode(renderMode_); }
        void SetAtlasRenderMode(GlyphRenderMode mode);
        HX_PROPERTY() i32 samplingPointSize() const { return samplingPointSize_; }
        HX_PROPERTY() i32 atlasPadding() const { return atlasPadding_; }
        HX_PROPERTY(ReadOnly) i32 atlasWidth() const { return atlasSize_; }
        HX_PROPERTY(ReadOnly) i32 atlasHeight() const { return atlasSize_; }
        void SetSamplingPointSize(i32 value);
        void SetAtlasPadding(i32 value);

        // The face's lines at the sampling size.
        FaceInfo faceInfo() const;
        HX_FUNCTION() bool HasCharacter(i32 character, bool searchFallbacks = false);
        // Renders the glyphs of the characters now (true when the font has all of them).
        HX_FUNCTION() bool TryAddCharacters(StringView characters);

        // The fonts after this one, in order.
        Span<const engine::PPtr<FontAsset>> fallbackFontAssetTable() const
        {
            return Span<const engine::PPtr<FontAsset>>(fallbacks_.Data(), fallbacks_.Size());
        }
        void SetFallbackFontAssetTable(Span<FontAsset* const> fonts);

        // The text system's font (null without font data), its fallbacks set from the table.
        TextFont* textFont();
        // The atlas pages as textures (distance fields or coverage), and the color glyphs' pages.
        u32 atlasTextureCount() const { return u32(pages_.Size()); }
        engine::Texture2D* AtlasTexture(u32 page, bool color);
        // Uploads the pages glyphs were added to since the last upload.
        void UpdateAtlasTextures();

        // A font asset of a font of the machine (made once per family and style; null when the machine has none).
        static FontAsset* CreateFontAsset(StringView familyName, StringView styleName = StringView());
        // The font asset of a text font (null when no font asset made it).
        static FontAsset* Of(const TextFont* font);

    private:
        void Rebuild();
        void SyncPages(font::FontAtlas& atlas, Array<engine::PPtr<engine::Texture2D>>& pages, bool color);

        HX_FIELD(SerializeField, Name = "m_AtlasRenderMode") i32 renderMode_ = 0;
        HX_FIELD(SerializeField, Name = "m_SamplingPointSize") i32 samplingPointSize_ = 64;
        HX_FIELD(SerializeField, Name = "m_AtlasPadding") i32 atlasPadding_ = 8;
        HX_FIELD(SerializeField, Name = "m_AtlasSize") i32 atlasSize_ = 1024;
        HX_FIELD(SerializeField, Name = "m_FallbackFontAssetTable") Array<engine::PPtr<FontAsset>> fallbacks_ = Array<engine::PPtr<FontAsset>>();

        std::shared_ptr<const Face> face_;
        std::unique_ptr<TextFont> font_;
        Array<engine::PPtr<engine::Texture2D>> pages_, colorPages_;
        u32 uploaded_ = ~0u;
    };

    // Registers the loader of font files (once).
    void RegisterFontAssetLoader();
}
