#pragma once

#include <Helix/Engine/Material/Material.h>
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
    // A glyph's rectangle in its atlas (pixels, padding included) and its metrics at the sampling size.
    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct GlyphRect
    {
        HX_FIELD(Name = "m_X", Private) i32 x = 0;
        HX_FIELD(Name = "m_Y", Private) i32 y = 0;
        HX_FIELD(Name = "m_Width", Private) i32 width = 0;
        HX_FIELD(Name = "m_Height", Private) i32 height = 0;
    };

    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct GlyphMetrics
    {
        HX_FIELD(Name = "m_Width", Private) f32 width = 0;
        HX_FIELD(Name = "m_Height", Private) f32 height = 0;
        HX_FIELD(Name = "m_HorizontalBearingX", Private) f32 horizontalBearingX = 0;
        HX_FIELD(Name = "m_HorizontalBearingY", Private) f32 horizontalBearingY = 0;
        HX_FIELD(Name = "m_HorizontalAdvance", Private) f32 horizontalAdvance = 0;
    };

    // A glyph of the asset's atlases and a character mapped to a glyph, across the bindings (C# has classes over them).
    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct GlyphData
    {
        HX_FIELD() u32 index = 0;
        HX_FIELD() GlyphMetrics metrics;
        HX_FIELD() GlyphRect glyphRect;
        HX_FIELD() f32 scale = 1;
        HX_FIELD() i32 atlasIndex = 0;
    };

    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct CharacterData
    {
        HX_FIELD() u32 unicode = 0;
        HX_FIELD() u32 glyphIndex = 0;
        HX_FIELD() f32 scale = 1;
    };

    // The face's lines at the sampling size (C#'s FaceInfo adds the names).
    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct FaceInfoData
    {
        HX_FIELD() f32 pointSize = 0;
        HX_FIELD() f32 scale = 1;
        HX_FIELD() i32 unitsPerEM = 0;
        HX_FIELD() f32 lineHeight = 0;
        HX_FIELD() f32 ascentLine = 0;
        HX_FIELD() f32 capLine = 0;
        HX_FIELD() f32 meanLine = 0;
        HX_FIELD() f32 baseline = 0;
        HX_FIELD() f32 descentLine = 0;
        HX_FIELD() f32 superscriptOffset = 0;
        HX_FIELD() f32 superscriptSize = 0;
        HX_FIELD() f32 subscriptOffset = 0;
        HX_FIELD() f32 subscriptSize = 0;
        HX_FIELD() f32 underlineOffset = 0;
        HX_FIELD() f32 underlineThickness = 0;
        HX_FIELD() f32 strikethroughOffset = 0;
        HX_FIELD() f32 strikethroughThickness = 0;
        HX_FIELD() f32 tabWidth = 0;
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class TextElementType : i32
    {
        Character = 1,
        Sprite = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class AtlasPopulationMode : i32
    {
        Static = 0,
        Dynamic = 1,
        DynamicOS = 2,
    };
    // The base of the text's assets (font and sprite assets): the material they draw with and their identity.
    HX_CLASS(Namespace = "HelixEngine.Text", Abstract)
    class TextAsset : public engine::ScriptableObject
    {
    public:
        HX_PROPERTY(ReadOnly) StringView version() const { return StringView("1.1.0"); }
        HX_PROPERTY(ReadOnly) i32 instanceID() const { return i32(engine::EntityId::ToULong(GetEntityId())); }
        HX_PROPERTY(ReadOnly) i32 hashCode() const;
        HX_PROPERTY() engine::Material* material() const { return material_.Get(); }
        void SetMaterial(engine::Material* value) { material_ = engine::PPtr<engine::Material>(value); }
        HX_PROPERTY(ReadOnly) i32 materialHashCode() const;

    private:
        HX_FIELD(SerializeField, Name = "m_Material") engine::PPtr<engine::Material> material_;
    };

    HX_CLASS(Namespace = "HelixEngine.Text")
    class FontAsset : public TextAsset
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
        HX_FUNCTION(Internal, Name = "CreateFontAssetInternal") static FontAsset* CreateFontAsset(StringView familyName, StringView styleName = "");
        // The font asset of a text font (null when no font asset made it).
        static FontAsset* Of(const TextFont* font);

        // ---------------------------------------------------------------------------------------- the tables
        // The glyphs rendered into the atlases so far and the characters added or used.
        HX_FUNCTION(Internal) Array<GlyphData> GlyphTableInternal();
        HX_FUNCTION(Internal) Array<CharacterData> CharacterTableInternal();
        HX_FUNCTION(Internal) FaceInfoData FaceInfoInternal() const;
        HX_FUNCTION(Internal) String FamilyNameInternal() const;
        HX_FUNCTION(Internal) String StyleNameInternal() const;
        // The glyph of a character in the face (0: none).
        HX_FUNCTION(Internal) u32 GlyphIndexInternal(u32 unicode) const;
        // Adds the characters' glyphs (the ones the face lacks returned).
        HX_FUNCTION(Internal) Array<u32> TryAddCharactersInternal(Span<const u32> unicodes);
        HX_FUNCTION(Internal) bool HasCharacterInternal(u32 unicode, bool searchFallbacks, bool tryAddCharacter);
        HX_FUNCTION(Internal) i32 AtlasRenderModeInternal() const { return renderMode_; }
        HX_FUNCTION(Internal) void SetAtlasRenderModeInternal(i32 mode) { SetAtlasRenderMode(GlyphRenderMode(mode)); }
        HX_FUNCTION(Internal) void SetSamplingPointSizeInternal(i32 value) { SetSamplingPointSize(value); }
        HX_FUNCTION(Internal) void SetAtlasPaddingInternal(i32 value) { SetAtlasPadding(value); }
        HX_FUNCTION(Internal) void SetAtlasSizeInternal(i32 value);
        HX_FUNCTION(Internal) Array<FontAsset*> FallbacksInternal() const;
        HX_FUNCTION(Internal) void SetFallbacksInternal(Span<FontAsset* const> fonts) { SetFallbackFontAssetTable(fonts); }
        HX_FUNCTION(Internal) Array<engine::Texture2D*> AtlasTexturesInternal();
        // The atlases emptied (their textures kept, or shrunk to nothing).
        HX_FUNCTION() void ClearFontAssetData(bool setAtlasSizeToZero = false);
        HX_FUNCTION() void ReadFontAssetDefinition() {}

        HX_PROPERTY() AtlasPopulationMode atlasPopulationMode() const { return populationMode_; }
        void SetAtlasPopulationMode(AtlasPopulationMode value) { populationMode_ = value; }
        HX_PROPERTY() bool isMultiAtlasTexturesEnabled() const { return multiAtlas_; }
        void SetIsMultiAtlasTexturesEnabled(bool value) { multiAtlas_ = value; }
        HX_PROPERTY(ReadOnly) i32 atlasTextureCount() { return i32(AtlasTexturesInternal().Size()); }
        HX_PROPERTY(ReadOnly) engine::Texture2D* atlasTexture() { return AtlasTexture(0, false); }
        // The weights of the face: synthetic bold and italic (the reference's style values).
        HX_PROPERTY() f32 regularStyleWeight() const { return regularWeight_; }
        void SetRegularStyleWeight(f32 value) { regularWeight_ = value; }
        HX_PROPERTY() f32 regularStyleSpacing() const { return regularSpacing_; }
        void SetRegularStyleSpacing(f32 value) { regularSpacing_ = value; }
        HX_PROPERTY() f32 boldStyleWeight() const { return boldWeight_; }
        void SetBoldStyleWeight(f32 value) { boldWeight_ = value; }
        HX_PROPERTY() f32 boldStyleSpacing() const { return boldSpacing_; }
        void SetBoldStyleSpacing(f32 value) { boldSpacing_ = value; }
        HX_PROPERTY() u8 italicStyleSlant() const { return italicSlant_; }
        void SetItalicStyleSlant(u8 value) { italicSlant_ = value; }
        HX_PROPERTY() u8 tabMultiple() const { return tabMultiple_; }
        void SetTabMultiple(u8 value) { tabMultiple_ = value; }
        // The weight table: the regular and italic typeface of each weight (100 to 900 by index 1 to 9).
        HX_FUNCTION(Internal) FontAsset* WeightTypefaceInternal(i32 index, bool italic) const;
        HX_FUNCTION(Internal) void SetWeightTypefaceInternal(i32 index, bool italic, FontAsset* font);

    private:
        void Rebuild();
        void SyncPages(font::FontAtlas& atlas, Array<engine::PPtr<engine::Texture2D>>& pages, bool color);

        HX_FIELD(SerializeField, Name = "m_AtlasRenderMode") i32 renderMode_ = 0;
        HX_FIELD(SerializeField, Name = "m_SamplingPointSize") i32 samplingPointSize_ = 64;
        HX_FIELD(SerializeField, Name = "m_AtlasPadding") i32 atlasPadding_ = 8;
        HX_FIELD(SerializeField, Name = "m_AtlasSize") i32 atlasSize_ = 1024;
        HX_FIELD(SerializeField, Name = "m_FallbackFontAssetTable") Array<engine::PPtr<FontAsset>> fallbacks_ = Array<engine::PPtr<FontAsset>>();
        HX_FIELD(SerializeField, Name = "m_AtlasPopulationMode") AtlasPopulationMode populationMode_ = AtlasPopulationMode::Dynamic;
        HX_FIELD(SerializeField, Name = "m_IsMultiAtlasTexturesEnabled") bool multiAtlas_ = true;
        HX_FIELD(SerializeField, Name = "m_RegularStyleWeight") f32 regularWeight_ = 0;
        HX_FIELD(SerializeField, Name = "m_RegularStyleSpacing") f32 regularSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_BoldStyleWeight") f32 boldWeight_ = 0.75f;
        HX_FIELD(SerializeField, Name = "m_BoldStyleSpacing") f32 boldSpacing_ = 7;
        HX_FIELD(SerializeField, Name = "m_ItalicStyleSlant") u8 italicSlant_ = 35;
        HX_FIELD(SerializeField, Name = "m_TabMultiple") u8 tabMultiple_ = 10;
        engine::PPtr<FontAsset> weights_[10][2];
        // The characters added or asked for (unicode to glyph), in their order.
        Array<CharacterData> characters_;

        std::shared_ptr<const Face> face_;
        std::unique_ptr<TextFont> font_;
        Array<engine::PPtr<engine::Texture2D>> pages_, colorPages_;
        u32 uploaded_ = ~0u;
    };

    // Registers the loader of font files (once).
    void RegisterFontAssetLoader();
}
