#pragma once

#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>
#include <Helix/UI/Text/Bidi/Bidi.h>
#include <Helix/UI/Text/Fonts/TextFont.h>
#include <Helix/UI/Text/LineBreak/LineBreak.h>
#include <Helix/UI/Text/RichText/RichText.h>
#include <Helix/UI/Text/Shaping/Shaper.h>

// Text layout: rich text into positioned glyph quads and the text info (characters, words, lines, links, pages) in a
// box. Paragraphs end at line feeds; each is resolved by the bidi algorithm, split into runs by script, level and font
// (the font chains of TextFont), shaped, broken into lines at the UAX 14 opportunities (or within a word longer than a
// line), reordered per line and aligned. Coordinates are pixels from the box's top left, y down.
namespace helix::ui
{
    enum class TextWrappingMode : u8
    {
        NoWrap,
        Normal,
        PreserveWhitespace,
        PreserveWhitespaceNoWrap
    };

    enum class TextOverflowMode : u8
    {
        Overflow,
        Ellipsis,
        Masking,
        Truncate,
        Page
    };

    // Where an ellipsis replaces the text that does not fit (TextOverflowMode::Ellipsis).
    enum class TextEllipsisPosition : u8
    {
        End,
        Start,
        Middle
    };

    enum class TextVerticalAlignment : u8
    {
        Top,
        Middle,
        Bottom,
        // The first baseline at the box's top.
        Baseline,
        // The glyphs' bounds centered.
        Geometry,
        // The first line's cap height at the box's top.
        Capline
    };

    // A sprite of a <sprite> tag as the caller's sprite assets give it: its image (an opaque handle with a rectangle in
    // its texture's uvs) and its glyph metrics in pixels at the text's size (y down from the baseline).
    struct TextSprite
    {
        const void* texture = nullptr;
        Rect uv;
        f32 width = 0, height = 0, bearingX = 0, bearingY = 0, advance = 0;
        bool tint = false;
    };

    struct TextGenerationSettings
    {
        TextFont* font = nullptr;
        // After the font's own chain.
        Span<TextFont* const> fallbacks;
        // A font of a <font> tag by name (null: the text's font).
        TextFont* (*findFont)(StringView name, void* user) = nullptr;
        // A sprite of a <sprite> tag (false: none, it takes an em of room).
        bool (*findSprite)(const TextElement& element, f32 size, TextSprite& out, void* user) = nullptr;
        void* user = nullptr;
        const TextStyleSheetLookup* styleSheet = nullptr;

        f32 fontSize = 36;
        Color color = Color(1, 1, 1, 1);
        // TextFlags of the whole text.
        u32 fontStyle = 0;
        TextAlignment alignment = TextAlignment::Left;
        TextVerticalAlignment verticalAlignment = TextVerticalAlignment::Top;
        // In hundredths of an em, added between characters, words, lines and paragraphs.
        f32 characterSpacing = 0, wordSpacing = 0, lineSpacing = 0, paragraphSpacing = 0;
        TextWrappingMode wrapping = TextWrappingMode::Normal;
        TextOverflowMode overflow = TextOverflowMode::Overflow;
        TextEllipsisPosition ellipsisPosition = TextEllipsisPosition::End;
        LineBreakStrictness strictness = LineBreakStrictness::Strict;
        BidiDirection direction = BidiDirection::Auto;
        bool richText = true;
        // The box: a negative size does not bound its axis.
        f32 width = -1, height = -1;
        // Sizes between min and max so the text fits the box (the largest that does).
        bool autoSize = false;
        f32 minFontSize = 8, maxFontSize = 72;
        // The page shown in the Page overflow mode.
        u32 page = 0;
    };

    struct CharacterInfo
    {
        char32_t character = 0;
        // The byte of the source text it came from.
        u32 index = 0;
        TextElementKind elementType = TextElementKind::Character;
        TextFont* font = nullptr;
        f32 pointSize = 0;
        u32 style = 0;
        u32 lineNumber = 0, pageNumber = 0;
        // The pen where it starts, its advance and its line's ascender, baseline and descender (y down).
        f32 origin = 0, xAdvance = 0, ascender = 0, baseLine = 0, descender = 0;
        // The corners of its quad (the first glyph's for a cluster).
        Vector2 topLeft, topRight, bottomLeft, bottomRight;
        Color color;
        bool isVisible = false;
        // The cluster's first glyph and the face index in its font chain.
        u32 glyph = 0;
    };

    struct WordInfo
    {
        u32 firstCharacterIndex = 0, lastCharacterIndex = 0, characterCount = 0;
    };

    struct LineInfo
    {
        u32 firstCharacterIndex = 0, lastCharacterIndex = 0, characterCount = 0, visibleCharacterCount = 0, spaceCount = 0, wordCount = 0;
        // The width of its characters (trailing white space left out) and where it starts.
        f32 width = 0, x = 0;
        f32 ascender = 0, baseline = 0, descender = 0, lineHeight = 0;
        TextAlignment alignment = TextAlignment::Left;
        u32 pageNumber = 0;
    };

    struct PageInfo
    {
        u32 firstCharacterIndex = 0, lastCharacterIndex = 0;
        f32 ascender = 0, descender = 0;
    };

    struct LinkInfo
    {
        String id;
        u32 firstCharacterIndex = 0, characterCount = 0;
    };

    // A glyph's quad: corners clockwise from the top left, uvs of its image (0 to 1, v down), its color and where its
    // image is (a font's atlas page, its color atlas, or a sprite's texture).
    struct TextQuad
    {
        Vector2 corners[4];
        Rect uv;
        Color color;
        TextFont* font = nullptr;
        u32 page = 0;
        bool colorGlyph = false;
        const void* sprite = nullptr;
        // Screen pixels per distance field pixel (the shader's antialiasing width), and bold (a thicker threshold).
        f32 sdfScale = 1;
        bool bold = false;
    };

    // A solid rectangle under or over the glyphs: marks (drawn before the glyphs), underlines and strikethroughs.
    struct TextRect
    {
        Rect rect;
        Color color;
        bool behind = false;
    };

    struct TextInfo
    {
        Array<CharacterInfo> characterInfo;
        Array<WordInfo> wordInfo;
        Array<LineInfo> lineInfo;
        Array<PageInfo> pageInfo;
        Array<LinkInfo> linkInfo;
        Array<TextQuad> quads;
        Array<TextRect> rects;
        u32 characterCount = 0, lineCount = 0, pageCount = 0;
        // The size the text takes (the width of its widest line and the height of its lines) and the size it was laid
        // out at (autoSize).
        f32 width = 0, height = 0, fontSize = 0;
        bool overflowed = false;
        void Clear();
    };

    class TextGenerator
    {
    public:
        // Lays out `source` (UTF-8, rich text when the settings say).
        void Generate(StringView source, const TextGenerationSettings& settings, TextInfo& out);
        // The size the text wants: its width unbounded (or within maxWidth) and the height that takes. It renders no
        // glyph images (it only reads the fonts), so generators on several threads may measure at once.
        Vector2 PreferredSize(StringView source, const TextGenerationSettings& settings, f32 maxWidth = -1);

    private:
        struct Glyph
        {
            u32 glyph;
            u32 cluster;
            u32 font;
            f32 advance, x, y;
        };
        struct Run
        {
            u32 start, end;
            u8 level;
            u32 font;
            u32 firstGlyph, glyphCount;
        };
        void Layout(StringView source, const TextGenerationSettings& settings, f32 size, TextInfo& out, bool quads = true);

        ParsedText parsed_;
        Bidi bidi_;
        Shaper shaper_;
        Array<TextFont*> fonts_;
        Array<Run> runs_;
        Array<Glyph> glyphs_;
        Array<ShapedGlyph> shaped_;
        Array<LineBreak> breaks_;
    };
}
