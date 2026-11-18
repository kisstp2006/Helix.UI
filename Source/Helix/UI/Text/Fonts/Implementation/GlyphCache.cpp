#include <Helix/UI/Text/Fonts/GlyphCache.h>

namespace helix::ui
{
    FaceInfo FaceInfoOf(const Face& face, f32 pointSize)
    {
        const font::Font& f = face.Font();
        FaceInfo info;
        info.familyName = String(face.Family());
        info.styleName = String(face.Style());
        info.pointSize = pointSize;
        info.unitsPerEM = f.Head().unitsPerEm ? f.Head().unitsPerEm : 1000;
        f32 s = pointSize / f32(info.unitsPerEM);
        info.scale = 1;
        const font::Os2Table& os2 = f.Os2();
        // The typographic metrics when OS/2 asks for them (USE_TYPO_METRICS), else hhea.
        bool typo = os2.present && (os2.fsSelection & 0x80);
        f32 ascender = typo ? os2.typoAscender : f.Hhea().ascender;
        f32 descender = typo ? os2.typoDescender : f.Hhea().descender;
        f32 gap = typo ? os2.typoLineGap : f.Hhea().lineGap;
        if (ascender == 0 && descender == 0 && os2.present)
        {
            ascender = os2.winAscent;
            descender = -f32(os2.winDescent);
        }
        info.ascentLine = ascender * s;
        info.descentLine = descender * s;
        info.lineHeight = (ascender - descender + gap) * s;
        info.baseline = 0;
        info.capLine = (os2.present && os2.capHeight ? os2.capHeight : ascender * 0.7f) * s;
        info.meanLine = (os2.present && os2.xHeight ? os2.xHeight : ascender * 0.5f) * s;
        info.superscriptOffset = (os2.present && os2.superscriptYOffset ? os2.superscriptYOffset : ascender * 0.5f) * s;
        info.superscriptSize = os2.present && os2.superscriptYSize ? f32(os2.superscriptYSize) / f32(info.unitsPerEM) : 0.5f;
        info.subscriptOffset = -(os2.present && os2.subscriptYOffset ? os2.subscriptYOffset : -descender * 0.5f) * s;
        info.subscriptSize = os2.present && os2.subscriptYSize ? f32(os2.subscriptYSize) / f32(info.unitsPerEM) : 0.5f;
        info.underlineOffset = (f.Post().underlinePosition ? f.Post().underlinePosition : descender * 0.5f) * s;
        info.underlineThickness = (f.Post().underlineThickness ? f.Post().underlineThickness : info.unitsPerEM * 0.05f) * s;
        info.strikethroughOffset = (os2.present && os2.strikeoutPosition ? os2.strikeoutPosition : info.meanLine / s * 0.5f) * s;
        info.strikethroughThickness = (os2.present && os2.strikeoutSize ? os2.strikeoutSize : info.underlineThickness / s) * s;
        u32 space = face.Glyph(' ');
        info.tabWidth = f32(space ? face.Advance(space) : info.unitsPerEM / 4) * s;
        return info;
    }

    GlyphCache::GlyphCache(std::shared_ptr<const Face> face, GlyphRenderMode mode, u32 samplingSize, u32 padding, u32 atlasSize)
        : face_(Move(face)), mode_(mode), samplingSize_(samplingSize), padding_(padding),
          atlas_(atlasSize, atlasSize, mode == GlyphRenderMode::Msdf ? 4 : 1, 1), colorAtlas_(atlasSize, atlasSize, 4, 1)
    {
        String error;
        outlines_.Open(face_->Font(), error);
        layers_.Read(face_->Font(), error);
        bitmaps_.Read(face_->Font(), error);
    }

    void GlyphCache::Clear()
    {
        entries_.Clear();
        atlas_.Clear();
        colorAtlas_.Clear();
        version_++;
    }

    const GlyphEntry* GlyphCache::Get(u32 glyph)
    {
        if (const GlyphEntry* e = entries_.Find(glyph))
            return e;
        GlyphEntry entry;
        font::GlyphBitmap bitmap;
        String error;
        f32 scale = f32(samplingSize_) / f32(face_->UnitsPerEm() ? face_->UnitsPerEm() : 1000);
        bool color = false;
        bool made = false;
        // Color glyphs first: COLR layers, then CBDT bitmaps (scaled at draw time to the sampling size).
        if (!layers_.Empty() && !layers_.Layers(glyph).Empty())
        {
            font::RasterOptions r;
            r.scale = scale;
            made = font::RenderColorGlyph(layers_, outlines_, glyph, 0, 0xFFFFFFFFu, r, bitmap, error);
            color = made;
        }
        if (!made && !bitmaps_.Empty())
        {
            u32 strike = 0;
            if (bitmaps_.Get(glyph, samplingSize_, bitmap, strike, error) && bitmap.width > 0)
            {
                // A strike of another size is scaled to the sampling size where the glyph is drawn.
                made = color = true;
                u16 ppem = bitmaps_.Strikes()[strike].ppemY;
                entry.scale = ppem ? f32(samplingSize_) / f32(ppem) : 1;
            }
        }
        if (!made)
        {
            font::Outline outline;
            if (!outlines_.Get(glyph, outline, error))
                return nullptr;
            if (outline.Empty())
            {
                entry.empty = true;
                entries_.Set(glyph, entry);
                return entries_.Find(glyph);
            }
            if (mode_ == GlyphRenderMode::Smooth)
            {
                font::RasterOptions r;
                r.scale = scale;
                made = font::Rasterize(outline, r, bitmap, error);
            }
            else
            {
                font::DistanceOptions d;
                d.scale = scale;
                d.padding = padding_;
                d.range = f32(padding_);
                made = mode_ == GlyphRenderMode::Msdf ? font::MultiChannelDistanceField(outline, d, bitmap, error)
                                                      : font::SignedDistanceField(outline, d, bitmap, error);
            }
        }
        if (!made || bitmap.width == 0 || bitmap.height == 0)
        {
            entry.empty = true;
            entries_.Set(glyph, entry);
            return entries_.Find(glyph);
        }
        font::FontAtlas& atlas = color ? colorAtlas_ : atlas_;
        font::AtlasPlace place;
        if (!atlas.Add(font::FontAtlas::Key(glyph, samplingSize_, color ? 1 : 0), bitmap, place))
            return nullptr;
        entry.page = place.page;
        entry.x = place.x;
        entry.y = place.y;
        entry.width = place.width;
        entry.height = place.height;
        entry.left = place.left;
        entry.top = place.top;
        entry.color = color;
        entry.empty = false;
        version_++;
        entries_.Set(glyph, entry);
        return entries_.Find(glyph);
    }
}
