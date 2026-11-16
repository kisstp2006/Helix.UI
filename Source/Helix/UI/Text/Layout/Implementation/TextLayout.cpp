#include <Helix/UI/Text/Layout/TextLayout.h>

#include <cmath>

namespace helix::ui
{
    void TextInfo::Clear()
    {
        characterInfo.Clear();
        wordInfo.Clear();
        lineInfo.Clear();
        pageInfo.Clear();
        linkInfo.Clear();
        quads.Clear();
        rects.Clear();
        characterCount = lineCount = pageCount = 0;
        width = height = fontSize = 0;
        overflowed = false;
    }

    namespace
    {
        constexpr u32 kNone = ~0u;
        constexpr f32 kUnbounded = 1e30f;
        constexpr f32 kItalicSlant = 0.2f;
        constexpr f32 kBoldSpacing = 0.07f;
        constexpr f32 kSmallCaps = 0.8f;

        char32_t ToUpper(char32_t c)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7) || (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) || (c >= 0x430 && c <= 0x44F))
                return c - 32;
            if (c >= 0x450 && c <= 0x45F)
                return c - 80;
            return c;
        }

        char32_t ToLower(char32_t c)
        {
            if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7) || (c >= 0x391 && c <= 0x3A9) || (c >= 0x410 && c <= 0x42F))
                return c + 32;
            if (c >= 0x400 && c <= 0x40F)
                return c + 80;
            return c;
        }

        bool IsWhitespace(char32_t c) { return c == '\t' || PropertiesOf(c).category == GeneralCategory::Zs; }
        bool IsParagraphEnd(char32_t c) { return c == '\n' || c == 0x2029 || c == 0xC; }
        bool IsWordCharacter(char32_t c)
        {
            GeneralCategory g = PropertiesOf(c).category;
            return u8(g) <= u8(GeneralCategory::No) || c == '\'' || c == 0x2019;
        }

        // A line before it is placed: its characters, the glyphs in visual order and its measures.
        struct LineDraft
        {
            u32 start, end;
            u32 paragraph;
            bool paragraphEnd;
            u8 paragraphLevel;
            f32 width, trailing;
            f32 ascender, descender, lineHeight;
            f32 x, baseline;
            u32 spaces;
            u32 page;
            bool visible;
        };

        struct VisualItem
        {
            u32 glyph;
            u32 character;
        };
    }

    void TextGenerator::Generate(StringView source, const TextGenerationSettings& settings, TextInfo& out)
    {
        f32 size = settings.fontSize;
        bool bounded = settings.width >= 0 || settings.height >= 0;
        if (settings.autoSize && bounded)
        {
            f32 lo = settings.minFontSize, hi = settings.maxFontSize, best = lo;
            for (int i = 0; i < 8; i++)
            {
                f32 mid = (lo + hi) * 0.5f;
                Layout(source, settings, mid, out, false);
                if (!out.overflowed)
                {
                    best = mid;
                    lo = mid;
                }
                else
                    hi = mid;
            }
            Layout(source, settings, settings.minFontSize, out, false);
            if (!out.overflowed)
                size = best;
            else
                size = settings.minFontSize;
        }
        Layout(source, settings, size, out);
    }

    Vector2 TextGenerator::PreferredSize(StringView source, const TextGenerationSettings& settings, f32 maxWidth)
    {
        TextGenerationSettings s = settings;
        s.width = maxWidth;
        s.height = -1;
        s.overflow = TextOverflowMode::Overflow;
        s.autoSize = false;
        TextInfo info;
        Layout(source, s, s.fontSize, info, false);
        return Vector2(info.width, info.height);
    }

    void TextGenerator::Layout(StringView source, const TextGenerationSettings& settings, f32 size, TextInfo& out, bool quads)
    {
        out.Clear();
        out.fontSize = size;
        if (!settings.font)
            return;
        TextStyle base;
        base.flags = settings.fontStyle;
        ParseRichText(source, settings.richText, base, parsed_, settings.styleSheet);
        u32 n = u32(parsed_.text.Size());
        const f32 boxWidth = settings.width >= 0 ? settings.width : -1;
        const bool wrap = settings.width >= 0 && (settings.wrapping == TextWrappingMode::Normal || settings.wrapping == TextWrappingMode::PreserveWhitespace);

        // Per character: code point (case changed), size, font, color.
        Array<char32_t> codes;
        Array<f32> sizes;
        Array<TextFont*> baseFonts;
        Array<Color> colors;
        codes.Resize(n);
        sizes.Resize(n);
        baseFonts.Resize(n);
        colors.Resize(n);
        for (u32 i = 0; i < n; i++)
        {
            const TextElement& e = parsed_.elements[i];
            const TextStyle& st = parsed_.styles[e.style];
            char32_t c = parsed_.text[i];
            f32 s = st.hasSize ? st.size.Resolve(size, size, size) : size;
            if (st.flags & (TextFlags::Uppercase | TextFlags::SmallCaps))
            {
                char32_t up = ToUpper(c);
                if ((st.flags & TextFlags::SmallCaps) && up != c)
                    s *= kSmallCaps;
                c = up;
            }
            else if (st.flags & TextFlags::Lowercase)
                c = ToLower(c);
            if (st.flags & (TextFlags::Superscript | TextFlags::Subscript))
                s *= 0.5f;
            codes[i] = c;
            sizes[i] = s > 0 ? s : 0;
            TextFont* f = settings.font;
            if (!st.font.Empty() && settings.findFont)
                if (TextFont* named = settings.findFont(st.font.View(), settings.user))
                    f = named;
            baseFonts[i] = f;
            Color col = st.hasColor ? st.color : settings.color;
            col.a *= st.alpha;
            colors[i] = col;
        }

        // Fonts: every font of the chains used, by index.
        fonts_.Clear();
        auto fontIndex = [&](TextFont* f) {
            for (u32 k = 0; k < fonts_.Size(); k++)
                if (fonts_[k] == f)
                    return k;
            fonts_.Push(f);
            return u32(fonts_.Size() - 1);
        };

        // Paragraphs: bidi, runs, shaping, advances and line breaks.
        runs_.Clear();
        glyphs_.Clear();
        Array<u32> runOf, clusterOf;
        Array<f32> advance, shift;
        Array<u8> levels;
        runOf.Resize(n);
        clusterOf.Resize(n);
        advance.Resize(n);
        shift.Resize(n);
        levels.Resize(n);
        Array<LineDraft> lines;
        Array<u8> paragraphLevels;
        Array<TextFont*> chain;
        Array<const Face*> faces;
        Array<ShapingRun> shaping;
        u32 paragraph = 0;
        for (u32 ps = 0; ps < n || (ps == 0 && n == 0);)
        {
            u32 pe = ps;
            while (pe < n && !IsParagraphEnd(codes[pe]))
                pe++;
            Span<const char32_t> para(codes.Data() + ps, pe - ps);
            bidi_.Resolve(para, settings.direction);
            u8 paraLevel = pe > ps ? bidi_.ParagraphLevel() : (settings.direction == BidiDirection::RightToLeft ? 1 : 0);
            paragraphLevels.Push(paraLevel);
            for (u32 i = ps; i < pe; i++)
            {
                levels[i] = bidi_.Levels()[i - ps];
            }
            // Segments of one base font and no special elements, itemized with the font's chain.
            for (u32 s = ps; s < pe;)
            {
                u32 e = s + 1;
                bool special = parsed_.elements[s].kind != TextElementKind::Character;
                if (!special)
                    while (e < pe && baseFonts[e] == baseFonts[s] && parsed_.elements[e].kind == TextElementKind::Character)
                        e++;
                if (special)
                {
                    Run r{s, e, levels[s], fontIndex(baseFonts[s]), u32(glyphs_.Size()), 0};
                    runOf[s] = u32(runs_.Size());
                    runs_.Push(r);
                    s = e;
                    continue;
                }
                TextFont::Chain(baseFonts[s], settings.fallbacks, chain);
                faces.Clear();
                for (TextFont* f : chain)
                    faces.Push(&f->GetFace());
                Itemize(para, s - ps, e - ps, bidi_, Span<const Face* const>(faces.Data(), faces.Size()), shaping);
                for (const ShapingRun& sr : shaping)
                {
                    TextFont* font = chain[sr.face];
                    u32 fi = fontIndex(font);
                    shaper_.Shape(font->GetFace(), para, sr, shaped_);
                    Run r{ps + sr.start, ps + sr.end, sr.level, fi, u32(glyphs_.Size()), u32(shaped_.Size())};
                    for (u32 i = r.start; i < r.end; i++)
                        runOf[i] = u32(runs_.Size());
                    for (const ShapedGlyph& g : shaped_)
                    {
                        u32 c = ps + g.cluster;
                        f32 k = font->Scale(sizes[c]);
                        glyphs_.Push({g.glyph, c, fi, f32(g.xAdvance) * k, f32(g.xOffset) * k, f32(g.yOffset) * k});
                    }
                    runs_.Push(r);
                }
                s = e;
            }
            // Clusters: a character belongs to the cluster that starts at or before it in its run.
            for (u32 i = ps; i < pe; i++)
            {
                clusterOf[i] = i;
                advance[i] = 0;
                shift[i] = 0;
            }
            for (u32 r = 0; r < runs_.Size(); r++)
            {
                const Run& run = runs_[r];
                if (run.start < ps || run.start >= pe)
                    continue;
                for (u32 g = run.firstGlyph; g < run.firstGlyph + run.glyphCount; g++)
                    advance[glyphs_[g].cluster] += glyphs_[g].advance;
                // The characters that start a cluster: those a glyph names.
                for (u32 i = run.start; i < run.end; i++)
                    clusterOf[i] = run.glyphCount == 0 ? i : kNone;
                for (u32 g = run.firstGlyph; g < run.firstGlyph + run.glyphCount; g++)
                    clusterOf[glyphs_[g].cluster] = glyphs_[g].cluster;
                u32 current = run.start;
                for (u32 i = run.start; i < run.end; i++)
                {
                    if (clusterOf[i] == i)
                        current = i;
                    clusterOf[i] = current;
                }
            }
            // Spacing, bold, monospace and the special elements.
            for (u32 i = ps; i < pe; i++)
            {
                const TextElement& e = parsed_.elements[i];
                const TextStyle& st = parsed_.styles[e.style];
                f32 s = sizes[i];
                if (e.kind == TextElementKind::Space)
                    advance[i] = e.length.Resolve(s, boxWidth > 0 ? boxWidth : 0, 0);
                else if (e.kind == TextElementKind::Sprite)
                {
                    TextSprite sprite;
                    advance[i] = settings.findSprite && settings.findSprite(e, s, sprite, settings.user) ? sprite.advance : s;
                }
                if (clusterOf[i] != i || e.kind != TextElementKind::Character)
                    continue;
                if (st.monospaced)
                {
                    f32 m = st.monospace.Resolve(s, 0, 0);
                    shift[i] = (m - advance[i]) * 0.5f;
                    advance[i] = m;
                }
                advance[i] += settings.characterSpacing * 0.01f * s + st.characterSpacing.Resolve(s, 0, 0);
                if (codes[i] == ' ' || codes[i] == 0x3000)
                    advance[i] += settings.wordSpacing * 0.01f * s;
                if (st.flags & TextFlags::Bold)
                    advance[i] += kBoldSpacing * s;
                if (codes[i] == '\t')
                {
                    f32 tab = baseFonts[i]->Info(s).tabWidth * 4;
                    advance[i] = tab > 0 ? tab : s;
                }
            }
            // Break opportunities (no break inside nobr).
            FindLineBreaks(para, breaks_, settings.strictness);
            for (u32 k = 1; k < pe - ps; k++)
            {
                u32 f1 = parsed_.styles[parsed_.elements[ps + k - 1].style].flags, f2 = parsed_.styles[parsed_.elements[ps + k].style].flags;
                if ((f1 & f2 & TextFlags::NoBreak) && breaks_[k] == LineBreak::Allowed)
                    breaks_[k] = LineBreak::Prohibited;
            }
            // Lines: as many characters as fit, broken at the last opportunity (within the word when there is none).
            u32 ls = ps;
            do
            {
                const TextStyle& first = parsed_.styles[parsed_.elements[ls < n ? ls : (n ? n - 1 : 0)].style];
                f32 s0 = ls < n ? sizes[ls] : size;
                f32 margins = n ? first.marginLeft.Resolve(s0, boxWidth, 0) + first.marginRight.Resolve(s0, boxWidth, 0) + first.indent.Resolve(s0, boxWidth, 0) : 0;
                f32 avail = wrap ? boxWidth - margins : kUnbounded;
                f32 pen = 0;
                u32 lastBreak = kNone, end = pe;
                for (u32 i = ls; i < pe; i++)
                {
                    if (i > ls && breaks_[i - ps] == LineBreak::Mandatory)
                    {
                        end = i;
                        break;
                    }
                    if (i > ls && breaks_[i - ps] == LineBreak::Allowed)
                        lastBreak = i;
                    if (!IsWhitespace(codes[i]) && pen + advance[i] > avail + 0.001f && i > ls)
                    {
                        end = lastBreak != kNone ? lastBreak : i;
                        while (end > ls && end < pe && clusterOf[end] != end)
                            end++;
                        break;
                    }
                    pen += advance[i];
                }
                if (end <= ls)
                    end = ls < pe ? ls + 1 : pe;
                LineDraft d{};
                d.start = ls;
                d.end = end;
                d.paragraph = paragraph;
                d.paragraphLevel = paraLevel;
                d.paragraphEnd = end == pe;
                lines.Push(d);
                ls = end;
            } while (ls < pe);
            // The paragraph's end character belongs to its last line.
            if (pe < n)
                lines.Back().end = pe + 1;
            paragraph++;
            ps = pe + 1;
            if (n == 0)
                break;
        }

        // Each line: its visual order, width and vertical measures.
        Array<Array<VisualItem>> visual;
        visual.Resize(lines.Size());
        Array<u8> lineLevels;
        Array<u32> order;
        for (u32 li = 0; li < lines.Size(); li++)
        {
            LineDraft& d = lines[li];
            u32 end = d.end;
            while (end > d.start && IsParagraphEnd(codes[end - 1]))
                end--;
            // Trailing white space takes no width.
            u32 trail = end;
            while (trail > d.start && IsWhitespace(codes[trail - 1]))
                trail--;
            d.width = d.trailing = 0;
            d.spaces = 0;
            for (u32 i = d.start; i < end; i++)
            {
                if (clusterOf[i] != i)
                    continue;
                (i < trail ? d.width : d.trailing) += advance[i];
                if (i < trail && (codes[i] == ' ' || codes[i] == 0x3000))
                    d.spaces++;
            }
            // Levels after L1: white space at the line's end and separators at the paragraph level.
            lineLevels.Clear();
            for (u32 i = d.start; i < end; i++)
                lineLevels.Push(i >= trail || codes[i] == '\t' ? d.paragraphLevel : levels[i]);
            Bidi::Reorder(Span<const u8>(lineLevels.Data(), lineLevels.Size()), order);
            Array<VisualItem>& items = visual[li];
            for (u32 k = 0; k < order.Size();)
            {
                u32 c = d.start + order[k];
                u32 r = runOf[c];
                bool rtl = (lineLevels[order[k]] & 1) != 0;
                u32 lo = c, hi = c, next = k + 1;
                while (next < order.Size())
                {
                    u32 c2 = d.start + order[next];
                    if (runOf[c2] != r || lineLevels[order[next]] != lineLevels[order[k]] || c2 != (rtl ? lo - 1 : hi + 1))
                        break;
                    lo = c2 < lo ? c2 : lo;
                    hi = c2 > hi ? c2 : hi;
                    next++;
                }
                const Run& run = runs_[r];
                if (run.glyphCount == 0)
                    for (u32 i = 0; i <= hi - lo; i++)
                        items.Push({kNone, rtl ? hi - i : lo + i});
                else if (!rtl)
                {
                    for (u32 g = run.firstGlyph; g < run.firstGlyph + run.glyphCount; g++)
                        if (glyphs_[g].cluster >= lo && glyphs_[g].cluster <= hi)
                            items.Push({g, glyphs_[g].cluster});
                }
                else
                    for (u32 g = run.firstGlyph + run.glyphCount; g-- > run.firstGlyph;)
                        if (glyphs_[g].cluster >= lo && glyphs_[g].cluster <= hi)
                            items.Push({g, glyphs_[g].cluster});
                k = next;
            }
            // Vertical measures from the fonts and sizes in the line (the paragraph's end alone for an empty line).
            d.ascender = d.descender = d.lineHeight = 0;
            u32 measureEnd = end > d.start ? end : (d.end > d.start ? d.end : d.start + (n ? 1 : 0));
            TextFont* lastFont = nullptr;
            f32 lastSize = -1;
            FaceInfo fi;
            for (u32 i = d.start; i < measureEnd || (i == d.start && n == 0); i++)
            {
                TextFont* f = n ? baseFonts[i < n ? i : n - 1] : settings.font;
                f32 s = n ? sizes[i < n ? i : n - 1] : size;
                // The metrics again only when the font or the size changes.
                if (f != lastFont || s != lastSize)
                {
                    fi = f->Info(s);
                    lastFont = f;
                    lastSize = s;
                }
                d.ascender = fi.ascentLine > d.ascender ? fi.ascentLine : d.ascender;
                d.descender = fi.descentLine < d.descender ? fi.descentLine : d.descender;
                f32 lh = fi.lineHeight;
                if (n)
                {
                    const TextStyle& st = parsed_.styles[parsed_.elements[i < n ? i : n - 1].style];
                    if (st.hasLineHeight)
                        lh = st.lineHeight.Resolve(s, fi.lineHeight, fi.lineHeight);
                }
                d.lineHeight = lh > d.lineHeight ? lh : d.lineHeight;
                if (n == 0)
                    break;
            }
        }

        // Horizontal place and baselines.
        f32 widest = 0;
        for (const LineDraft& d : lines)
            widest = d.width > widest ? d.width : widest;
        f32 alignWidth = boxWidth >= 0 ? boxWidth : widest;
        f32 y = 0;
        for (u32 li = 0; li < lines.Size(); li++)
        {
            LineDraft& d = lines[li];
            const TextStyle& st = parsed_.styles[parsed_.elements[d.start < n ? d.start : (n ? n - 1 : 0)].style];
            f32 s0 = n ? sizes[d.start < n ? d.start : n - 1] : size;
            f32 left = n ? st.marginLeft.Resolve(s0, alignWidth, 0) + st.indent.Resolve(s0, alignWidth, 0) : 0;
            f32 right = n ? st.marginRight.Resolve(s0, alignWidth, 0) : 0;
            f32 room = alignWidth - left - right;
            TextAlignment align = st.hasAlign ? st.align : settings.alignment;
            f32 x = left;
            if (align == TextAlignment::Center)
                x += (room - d.width) * 0.5f;
            else if (align == TextAlignment::Right)
                x += room - d.width;
            // Right to left paragraphs have their trailing white space at the left.
            if (d.paragraphLevel & 1)
                x -= d.trailing;
            d.x = x;
            f32 spacing = settings.lineSpacing * 0.01f * s0;
            if (li == 0)
                y = d.ascender;
            else
            {
                const LineDraft& p = lines[li - 1];
                f32 step = d.lineHeight > d.ascender - p.descender ? d.lineHeight : d.ascender - p.descender;
                y += step + spacing;
                if (p.paragraphEnd)
                    y += settings.paragraphSpacing * 0.01f * s0;
            }
            d.baseline = y;
            d.visible = true;
            d.page = 0;
        }
        f32 total = lines.Empty() ? 0 : lines.Back().baseline - lines.Back().descender;
        // Overflow of the box's height: truncated lines, pages.
        f32 boxHeight = settings.height;
        if (boxHeight >= 0)
        {
            out.overflowed = total > boxHeight + 0.01f;
            if (settings.overflow == TextOverflowMode::Page)
            {
                f32 pageTop = 0;
                u32 page = 0;
                for (u32 li = 0; li < lines.Size(); li++)
                {
                    LineDraft& d = lines[li];
                    if (d.baseline - d.descender - pageTop > boxHeight + 0.01f && li > 0)
                    {
                        page++;
                        pageTop = d.baseline - d.ascender;
                    }
                    d.page = page;
                    d.baseline -= pageTop;
                    d.visible = page == settings.page;
                }
                out.pageCount = page + 1;
            }
            else if (settings.overflow == TextOverflowMode::Truncate || settings.overflow == TextOverflowMode::Ellipsis)
                for (LineDraft& d : lines)
                    d.visible = d.baseline - d.descender <= boxHeight + 0.01f || &d == &lines[0];
        }
        if (settings.width >= 0 && !wrap)
            out.overflowed = out.overflowed || widest > settings.width + 0.01f;
        if (out.pageCount == 0)
            out.pageCount = 1;
        // Vertical alignment in the box.
        f32 dy = 0;
        if (!lines.Empty())
        {
            f32 h = boxHeight >= 0 ? boxHeight : total;
            f32 shown = 0;
            for (const LineDraft& d : lines)
                if (d.visible)
                    shown = d.baseline - d.descender;
            switch (settings.verticalAlignment)
            {
            case TextVerticalAlignment::Middle:
            case TextVerticalAlignment::Geometry: dy = (h - shown) * 0.5f; break;
            case TextVerticalAlignment::Bottom: dy = h - shown; break;
            case TextVerticalAlignment::Baseline: dy = -lines[0].ascender; break;
            case TextVerticalAlignment::Capline: dy = settings.font->Info(size).capLine - lines[0].ascender; break;
            default: break;
            }
            for (LineDraft& d : lines)
                d.baseline += dy;
        }

        // Ellipsis: the last visible line ends with one when lines were cut or a line is wider than the box.
        u32 ellipsisLine = kNone;
        if (settings.overflow == TextOverflowMode::Ellipsis)
        {
            for (u32 li = 0; li < lines.Size(); li++)
                if (lines[li].visible && ((li + 1 < lines.Size() && !lines[li + 1].visible) ||
                                          (settings.width >= 0 && lines[li].width > settings.width + 0.01f)))
                    ellipsisLine = li;
        }

        out.characterCount = n;
        out.width = widest;
        out.height = total;
        if (!quads)
            return;

        // Characters, quads and rectangles.
        out.characterInfo.Resize(n);
        for (u32 i = 0; i < n; i++)
        {
            CharacterInfo& ci = out.characterInfo[i];
            ci.character = parsed_.text[i];
            ci.index = parsed_.elements[i].source;
            ci.elementType = parsed_.elements[i].kind;
            ci.font = baseFonts[i];
            ci.pointSize = sizes[i];
            ci.style = parsed_.styles[parsed_.elements[i].style].flags;
            ci.color = colors[i];
        }
        for (u32 li = 0; li < lines.Size(); li++)
        {
            LineDraft& d = lines[li];
            LineInfo info;
            info.firstCharacterIndex = d.start;
            info.lastCharacterIndex = d.end > d.start ? d.end - 1 : d.start;
            info.characterCount = d.end - d.start;
            info.width = d.width;
            info.x = d.x;
            info.ascender = d.baseline - d.ascender;
            info.baseline = d.baseline;
            info.descender = d.baseline - d.descender;
            info.lineHeight = d.lineHeight;
            info.spaceCount = d.spaces;
            info.pageNumber = d.page;
            const TextStyle& st = parsed_.styles[parsed_.elements[d.start < n ? d.start : (n ? n - 1 : 0)].style];
            info.alignment = n && st.hasAlign ? st.align : settings.alignment;
            // Justified lines (but a paragraph's last) and flush ones spread their spaces.
            f32 extra = 0;
            if (boxWidth >= 0 && d.spaces > 0 && ((info.alignment == TextAlignment::Justified && !d.paragraphEnd) || info.alignment == TextAlignment::Flush))
            {
                f32 s0 = n ? sizes[d.start < n ? d.start : n - 1] : size;
                f32 room = boxWidth - (n ? st.marginLeft.Resolve(s0, boxWidth, 0) + st.marginRight.Resolve(s0, boxWidth, 0) + st.indent.Resolve(s0, boxWidth, 0) : 0);
                extra = (room - d.width) / f32(d.spaces);
                if (extra < 0)
                    extra = 0;
            }
            // The ellipsis takes the room of the characters it replaces.
            f32 ellipsisWidth = 0;
            u32 ellipsisGlyph = 0;
            TextFont* ellipsisFont = settings.font;
            f32 cutAt = kUnbounded;
            if (li == ellipsisLine)
            {
                ellipsisGlyph = ellipsisFont->GetFace().Glyph(0x2026);
                f32 s0 = n ? sizes[d.start < n ? d.start : n - 1] : size;
                if (ellipsisGlyph)
                    ellipsisWidth = f32(ellipsisFont->GetFace().Advance(ellipsisGlyph)) * ellipsisFont->Scale(s0);
                cutAt = (boxWidth >= 0 ? boxWidth : d.width) - ellipsisWidth;
            }
            // An ellipsis at the start or in the middle: the clusters kept on each side of it (by their place from the
            // line's start in visual order) and how far the kept tail moves back.
            f32 headEnd = 0, tailStart = kUnbounded, tailShift = 0;
            bool ellipsisInside = li == ellipsisLine && settings.ellipsisPosition != TextEllipsisPosition::End;
            if (ellipsisInside)
            {
                f32 room = cutAt - d.x > 0 ? cutAt - d.x : 0;
                f32 x = 0;
                u32 previous = kNone;
                f32 firstTail = kUnbounded;
                bool middle = settings.ellipsisPosition == TextEllipsisPosition::Middle;
                for (const VisualItem& item : visual[li])
                {
                    u32 cluster = clusterOf[item.character];
                    if (cluster == previous)
                        continue;
                    previous = cluster;
                    if (middle && x + advance[cluster] <= room * 0.5f + 0.01f)
                        headEnd = x + advance[cluster];
                    if (x >= d.width - (middle ? room - headEnd : room) - 0.01f && firstTail == kUnbounded)
                        firstTail = x;
                    x += advance[cluster];
                }
                tailStart = firstTail;
                tailShift = firstTail == kUnbounded ? 0.0f : firstTail - (headEnd + ellipsisWidth);
            }
            f32 pen = d.x;
            f32 lastEnd = d.x;
            bool cut = false;
            u32 lastCluster = kNone;
            f32 clusterPen = pen, inCluster = 0;
            f32 moved = 0;
            for (const VisualItem& item : visual[li])
            {
                u32 c = item.character;
                const TextElement& e = parsed_.elements[c];
                const TextStyle& st2 = parsed_.styles[e.style];
                u32 cluster = clusterOf[c];
                if (cluster != lastCluster)
                {
                    lastCluster = cluster;
                    if (e.kind == TextElementKind::Position)
                    {
                        f32 target = d.x + e.length.Resolve(sizes[c], alignWidth, 0);
                        pen = target > pen ? target : pen;
                    }
                    if (ellipsisInside)
                    {
                        // Kept before the ellipsis, kept after it (moved back), or cut.
                        f32 x = pen - d.x;
                        bool head = x + advance[cluster] <= headEnd + 0.01f;
                        bool tail = x >= tailStart - 0.01f;
                        cut = !head && !tail;
                        moved = tail ? -tailShift : 0.0f;
                    }
                    else if (li == ellipsisLine && pen + advance[cluster] > cutAt + 0.01f && !IsWhitespace(codes[cluster]))
                        cut = true;
                    clusterPen = pen + shift[cluster] + moved;
                    inCluster = 0;
                    CharacterInfo& ci = out.characterInfo[cluster];
                    ci.origin = pen + moved;
                    ci.xAdvance = pen + moved + advance[cluster];
                    ci.lineNumber = li;
                    ci.pageNumber = d.page;
                    ci.ascender = d.baseline - d.ascender;
                    ci.baseLine = d.baseline;
                    ci.descender = d.baseline - d.descender;
                    ci.topLeft = ci.bottomLeft = Vector2(pen + moved, d.baseline);
                    ci.topRight = ci.bottomRight = Vector2(pen + moved + advance[cluster], d.baseline);
                    pen += advance[cluster];
                    if (codes[cluster] == ' ' || codes[cluster] == 0x3000)
                        pen += extra;
                    if (!cut)
                        lastEnd = pen;
                }
                CharacterInfo& ci = out.characterInfo[cluster];
                if (!d.visible || cut)
                    continue;
                f32 s = sizes[cluster];
                f32 vy = d.baseline - st2.verticalOffset.Resolve(s, 0, 0);
                if (st2.flags & TextFlags::Superscript)
                    vy -= baseFonts[cluster]->Info(s * 2).superscriptOffset;
                if (st2.flags & TextFlags::Subscript)
                    vy -= baseFonts[cluster]->Info(s * 2).subscriptOffset;
                if (item.glyph == kNone)
                {
                    if (e.kind == TextElementKind::Sprite && settings.findSprite)
                    {
                        TextSprite sprite;
                        if (settings.findSprite(e, s, sprite, settings.user))
                        {
                            TextQuad q;
                            f32 x0 = ci.origin + sprite.bearingX, y0 = vy - sprite.bearingY;
                            q.corners[0] = Vector2(x0, y0);
                            q.corners[1] = Vector2(x0 + sprite.width, y0);
                            q.corners[2] = Vector2(x0 + sprite.width, y0 + sprite.height);
                            q.corners[3] = Vector2(x0, y0 + sprite.height);
                            q.uv = sprite.uv;
                            q.color = sprite.tint ? ci.color : Color(1, 1, 1, ci.color.a);
                            q.sprite = sprite.texture;
                            q.colorGlyph = true;
                            out.quads.Push(q);
                            ci.isVisible = true;
                        }
                    }
                    continue;
                }
                const Glyph& g = glyphs_[item.glyph];
                TextFont* font = fonts_[g.font];
                f32 gx = clusterPen + inCluster + g.x;
                inCluster += g.advance;
                if (ci.glyph == 0)
                {
                    ci.glyph = g.glyph;
                    ci.font = font;
                }
                const GlyphEntry* entry = font->Cache().Get(g.glyph);
                if (!entry || entry->empty)
                    continue;
                font::FontAtlas& atlas = entry->color ? font->Cache().ColorAtlas() : font->Cache().Atlas();
                f32 k = s / f32(font->Cache().SamplingSize()) * entry->scale;
                f32 x0 = gx + f32(entry->left) * k, y0 = vy - g.y + f32(entry->top) * k;
                f32 x1 = x0 + f32(entry->width) * k, y1 = y0 + f32(entry->height) * k;
                TextQuad q;
                f32 slant = (st2.flags & TextFlags::Italic) ? kItalicSlant : 0;
                q.corners[0] = Vector2(x0 + (vy - y0) * slant, y0);
                q.corners[1] = Vector2(x1 + (vy - y0) * slant, y0);
                q.corners[2] = Vector2(x1 + (vy - y1) * slant, y1);
                q.corners[3] = Vector2(x0 + (vy - y1) * slant, y1);
                f32 pw = f32(atlas.PageWidth()), ph = f32(atlas.PageHeight());
                q.uv = Rect(f32(entry->x) / pw, f32(entry->y) / ph, f32(entry->width) / pw, f32(entry->height) / ph);
                q.color = entry->color ? Color(1, 1, 1, ci.color.a) : ci.color;
                q.font = font;
                q.page = entry->page;
                q.colorGlyph = entry->color;
                q.sdfScale = k;
                q.bold = (st2.flags & TextFlags::Bold) != 0;
                if (!ci.isVisible)
                {
                    ci.topLeft = q.corners[0];
                    ci.topRight = q.corners[1];
                    ci.bottomRight = q.corners[2];
                    ci.bottomLeft = q.corners[3];
                }
                ci.isVisible = true;
                out.quads.Push(q);
            }
            if (li == ellipsisLine && ellipsisGlyph)
            {
                // The ellipsis glyph after the last character that fit.
                const GlyphEntry* entry = ellipsisFont->Cache().Get(ellipsisGlyph);
                f32 s = n ? sizes[d.start < n ? d.start : n - 1] : size;
                if (entry && !entry->empty)
                {
                    font::FontAtlas& atlas = ellipsisFont->Cache().Atlas();
                    f32 k = s / f32(ellipsisFont->Cache().SamplingSize());
                    f32 at = ellipsisInside ? d.x + headEnd : lastEnd;
                    f32 x0 = at + f32(entry->left) * k, y0 = d.baseline + f32(entry->top) * k;
                    TextQuad q;
                    q.corners[0] = Vector2(x0, y0);
                    q.corners[1] = Vector2(x0 + f32(entry->width) * k, y0);
                    q.corners[2] = Vector2(x0 + f32(entry->width) * k, y0 + f32(entry->height) * k);
                    q.corners[3] = Vector2(x0, y0 + f32(entry->height) * k);
                    q.uv = Rect(f32(entry->x) / f32(atlas.PageWidth()), f32(entry->y) / f32(atlas.PageHeight()), f32(entry->width) / f32(atlas.PageWidth()),
                                f32(entry->height) / f32(atlas.PageHeight()));
                    q.color = n ? colors[d.start < n ? d.start : n - 1] : settings.color;
                    q.font = ellipsisFont;
                    q.page = entry->page;
                    q.sdfScale = k;
                    out.quads.Push(q);
                }
                out.overflowed = true;
            }
            for (u32 i = d.start; i < d.end && i < n; i++)
                if (out.characterInfo[i].isVisible)
                    info.visibleCharacterCount++;
            // Marks, underlines and strikethroughs over runs of the line's characters in visual order.
            if (d.visible)
            {
                struct Span
                {
                    f32 x0 = 0, x1 = 0;
                    bool open = false;
                    Color color;
                };
                Span mark, under, strike;
                FaceInfo fi = settings.font->Info(n ? sizes[d.start < n ? d.start : n - 1] : size);
                auto flush = [&](Span& sp, f32 top, f32 bottom, bool behind) {
                    if (sp.open && sp.x1 > sp.x0)
                        out.rects.Push({Rect(sp.x0, top, sp.x1 - sp.x0, bottom - top), sp.color, behind});
                    sp.open = false;
                };
                // Lines at least a pixel thick, on whole pixels (thinner ones would cover no pixel's center).
                fi.underlineThickness = std::round(fi.underlineThickness > 1 ? fi.underlineThickness : 1);
                fi.strikethroughThickness = std::round(fi.strikethroughThickness > 1 ? fi.strikethroughThickness : 1);
                f32 underY = std::round(d.baseline - fi.underlineOffset), strikeY = std::round(d.baseline - fi.strikethroughOffset);
                auto step = [&](Span& sp, bool on, const CharacterInfo& ci, Color color, f32 top, f32 bottom, bool behind) {
                    if (on && (!sp.open || !(sp.color == color)))
                    {
                        flush(sp, top, bottom, behind);
                        sp = {ci.origin, ci.xAdvance, true, color};
                    }
                    else if (on)
                        sp.x1 = ci.xAdvance > sp.x1 ? ci.xAdvance : sp.x1;
                    else
                        flush(sp, top, bottom, behind);
                };
                u32 last = kNone;
                for (const VisualItem& item : visual[li])
                {
                    u32 c = clusterOf[item.character];
                    if (c == last || c >= n)
                        continue;
                    last = c;
                    const CharacterInfo& ci = out.characterInfo[c];
                    const TextStyle& st2 = parsed_.styles[parsed_.elements[c].style];
                    bool trailing = c >= d.start && IsWhitespace(codes[c]);
                    for (u32 k = c; trailing && k < d.end && k < n; k++)
                        trailing = IsWhitespace(codes[k]) || IsParagraphEnd(codes[k]);
                    step(mark, (st2.flags & TextFlags::Mark) != 0, ci, st2.markColor, d.baseline - d.ascender, d.baseline - d.descender, true);
                    step(under, (st2.flags & TextFlags::Underline) != 0 && !trailing, ci, colors[c], underY, underY + fi.underlineThickness, false);
                    step(strike, (st2.flags & TextFlags::Strikethrough) != 0 && !trailing, ci, colors[c], strikeY,
                         strikeY + fi.strikethroughThickness, false);
                }
                flush(mark, d.baseline - d.ascender, d.baseline - d.descender, true);
                flush(under, underY, underY + fi.underlineThickness, false);
                flush(strike, strikeY, strikeY + fi.strikethroughThickness, false);
            }
            out.lineInfo.Push(info);
        }
        // Words and links.
        for (u32 i = 0; i < n;)
        {
            if (!IsWordCharacter(codes[i]))
            {
                i++;
                continue;
            }
            u32 e = i;
            while (e < n && IsWordCharacter(codes[e]))
                e++;
            out.wordInfo.Push({i, e - 1, e - i});
            if (out.characterInfo[i].lineNumber < out.lineInfo.Size())
                out.lineInfo[out.characterInfo[i].lineNumber].wordCount++;
            i = e;
        }
        for (const TextLink& l : parsed_.links)
            out.linkInfo.Push({l.id, l.firstElement, l.count});
        for (u32 p = 0; p < out.pageCount; p++)
        {
            PageInfo page;
            bool any = false;
            for (const LineInfo& l : out.lineInfo)
                if (l.pageNumber == p)
                {
                    if (!any)
                    {
                        page.firstCharacterIndex = l.firstCharacterIndex;
                        page.ascender = l.ascender;
                    }
                    page.lastCharacterIndex = l.lastCharacterIndex;
                    page.descender = l.descender;
                    any = true;
                }
            out.pageInfo.Push(page);
        }
        out.characterCount = n;
        out.lineCount = u32(out.lineInfo.Size());
        out.width = widest;
        out.height = total;
    }
}
