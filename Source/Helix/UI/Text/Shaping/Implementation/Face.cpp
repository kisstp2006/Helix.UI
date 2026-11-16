#include <Helix/UI/Text/Shaping/Face.h>

namespace helix::ui
{
    bool Face::Open(Span<const u8> file, u32 index, String& error)
    {
        open_ = false;
        bytes_ = std::make_shared<Array<u8>>();
        bytes_->Append(file.Data(), file.Size());
        if (!font_.Open(Span<const u8>(bytes_->Data(), bytes_->Size()), index, error))
            return false;
        using font::Tag;
        if (!gsub_.Read(font_, Tag("GSUB"), error) || !gpos_.Read(font_, Tag("GPOS"), error) || !gdef_.Read(font_, error) || !kern_.Read(font_, error))
            return false;
        hasGdef_ = font_.HasTable(Tag("GDEF"));
        // The digests from the subtables' coverages of their first glyph.
        for (int pass = 0; pass < 2; pass++)
        {
            const font::Layout& layout = pass ? gpos_ : gsub_;
            Array<LookupDigest>& digests = pass ? gposDigests_ : gsubDigests_;
            digests.Clear();
            digests.Resize(layout.lookups.Size());
            for (u32 l = 0; l < layout.lookups.Size(); l++)
            {
                const font::LayoutLookup& lk = layout.lookups[l];
                for (u32 k = 0; k < lk.subtables.count; k++)
                {
                    u32 coverage = layout.subtables[lk.subtables.begin + k].coverage;
                    if (coverage >= layout.coverages.Size())
                    {
                        digests[l].Add(0, 0xFFFF);
                        continue;
                    }
                    font::LayoutList ranges = layout.coverages[coverage];
                    for (u32 r = 0; r < ranges.count; r++)
                        digests[l].Add(layout.ranges[ranges.begin + r].first, layout.ranges[ranges.begin + r].last);
                }
            }
        }
        open_ = true;
        return true;
    }

    u32 Face::Glyph(char32_t c, char32_t selector) const
    {
        if (selector)
            if (u32 g = font_.GlyphIndex(u32(c), u32(selector)))
                return g;
        return font_.GlyphIndex(u32(c));
    }

    StringView Face::Family() const
    {
        StringView typographic = font_.Name(font::kNameTypographicFamily);
        return typographic.Empty() ? font_.Name(font::kNameFamily) : typographic;
    }

    StringView Face::Style() const
    {
        StringView typographic = font_.Name(font::kNameTypographicSubfamily);
        return typographic.Empty() ? font_.Name(font::kNameSubfamily) : typographic;
    }
}
