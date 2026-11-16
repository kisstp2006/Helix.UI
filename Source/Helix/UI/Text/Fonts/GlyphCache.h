#pragma once

#include <Helix/Font/Atlas/FontAtlas.h>
#include <Helix/Font/Color/Color.h>
#include <Helix/Font/Outline/Outline.h>
#include <Helix/UI/Text/Shaping/Face.h>

#include <memory>

// The glyph images of a face: rendered at a sampling size when first asked for and packed into atlas pages, the
// distance fields (or coverage) in one atlas and color glyphs (COLR layers, CBDT bitmaps) in an RGBA one. FaceInfo is
// the face's lines at a point size.
namespace helix::ui
{
    enum class GlyphRenderMode : u8
    {
        // A signed distance field (one channel): sharp at every size, outlines and shadows from the field.
        Sdf,
        // A multi channel distance field (RGBA): sharper corners when magnified.
        Msdf,
        // Coverage (one channel), for text drawn near the sampling size.
        Smooth
    };

    // The lines of a face at a point size (pixels), y up from the baseline.
    struct FaceInfo
    {
        String familyName, styleName;
        f32 pointSize = 0;
        f32 scale = 1;
        i32 unitsPerEM = 0;
        f32 lineHeight = 0, ascentLine = 0, capLine = 0, meanLine = 0, baseline = 0, descentLine = 0;
        f32 superscriptOffset = 0, superscriptSize = 0, subscriptOffset = 0, subscriptSize = 0;
        f32 underlineOffset = 0, underlineThickness = 0, strikethroughOffset = 0, strikethroughThickness = 0;
        f32 tabWidth = 0;
    };
    FaceInfo FaceInfoOf(const Face& face, f32 pointSize);

    // A glyph in its atlas: the page and pixel rectangle (padding included), and where its top left corner is from the
    // glyph origin in pixels at the sampling size (y down). Empty glyphs (spaces) have no rectangle.
    struct GlyphEntry
    {
        u32 page = 0;
        u32 x = 0, y = 0, width = 0, height = 0;
        i32 left = 0, top = 0;
        // Sampling size pixels per image pixel (not 1 only for color bitmaps of another strike size).
        f32 scale = 1;
        bool color = false;
        bool empty = true;
    };

    class GlyphCache
    {
    public:
        // samplingSize: pixels per em of the images; padding: pixels around each (the distance fields' range).
        GlyphCache(std::shared_ptr<const Face> face, GlyphRenderMode mode, u32 samplingSize = 64, u32 padding = 8, u32 atlasSize = 1024);

        const Face& GetFace() const { return *face_; }
        GlyphRenderMode Mode() const { return mode_; }
        u32 SamplingSize() const { return samplingSize_; }
        u32 Padding() const { return padding_; }
        // The image of a glyph, rendered and packed on first use (null when the glyph cannot be drawn).
        const GlyphEntry* Get(u32 glyph);
        bool HasColorGlyphs() const { return !layers_.Empty() || !bitmaps_.Empty(); }

        font::FontAtlas& Atlas() { return atlas_; }
        font::FontAtlas& ColorAtlas() { return colorAtlas_; }
        // Bumped whenever a glyph is added (the pages need uploading).
        u32 Version() const { return version_; }

    private:
        std::shared_ptr<const Face> face_;
        GlyphRenderMode mode_;
        u32 samplingSize_, padding_;
        font::Outlines outlines_;
        font::ColorLayers layers_;
        font::ColorBitmaps bitmaps_;
        font::FontAtlas atlas_;
        font::FontAtlas colorAtlas_;
        HashMap<u32, GlyphEntry> entries_;
        u32 version_ = 0;
    };
}
