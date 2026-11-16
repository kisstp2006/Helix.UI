#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Math/Color.h>
#include <Helix/UI/Text/Unicode/Unicode.h>

// Rich text: the tags of the reference text system parsed out of a text into its characters, each with the index of
// its style (the state of every open tag). Tags that are not known, or not closed properly, stay in the text as written.
// Supported: b, i, u, s, color, alpha, size, font, align, mark, sub, sup, line-height, cspace, mspace, indent, margin
// (margin-left, margin-right), voffset, space, pos, nobr, noparse, br, lowercase, uppercase, allcaps, smallcaps, link,
// style, sprite, gradient, width, rotate, page.
namespace helix::ui
{
    enum class TextAlignment : u8
    {
        Left,
        Center,
        Right,
        Justified,
        Flush
    };

    // A length of a tag: pixels, a share of a reference (percent), ems of the current size, or a change of the current
    // value (+ or - before the number).
    struct TextLength
    {
        enum class Unit : u8
        {
            Pixels,
            Percent,
            Em,
            Relative
        };
        f32 value = 0;
        Unit unit = Unit::Pixels;

        // The length in pixels: percent of `reference`, ems of `size`, relative to `current`.
        f32 Resolve(f32 size, f32 reference, f32 current) const;
    };

    namespace TextFlags
    {
        inline constexpr u32 Bold = 1, Italic = 2, Underline = 4, Strikethrough = 8, Lowercase = 16, Uppercase = 32, SmallCaps = 64,
                             Superscript = 128, Subscript = 256, Mark = 512, NoBreak = 1024;
    }

    // The state of the tags at a character. Lengths stay unresolved (the layout knows the sizes they refer to).
    struct TextStyle
    {
        u32 flags = 0;
        bool hasColor = false;
        Color color = Color(1, 1, 1, 1);
        // Multiplies the color's alpha.
        f32 alpha = 1;
        bool hasSize = false;
        TextLength size;
        // A font by name (empty: the text's font); resolved by the layout.
        String font;
        bool hasAlign = false;
        TextAlignment align = TextAlignment::Left;
        Color markColor = Color(1, 1, 0, 0.25f);
        bool hasLineHeight = false;
        TextLength lineHeight;
        TextLength characterSpacing, monospace, indent, marginLeft, marginRight, verticalOffset, width;
        bool monospaced = false;
        bool hasWidth = false;
        f32 rotate = 0;
        // Into the links of the parsed text, -1 for none.
        i32 link = -1;
        // A gradient by name (empty: none).
        String gradient;

        bool operator==(const TextStyle& o) const;
    };

    enum class TextElementKind : u8
    {
        Character,
        // A sprite of a sprite asset (by name or index), as wide as its glyph.
        Sprite,
        // Horizontal room of `length` (space), or the pen moved to `length` from the line's start (pos).
        Space,
        Position,
        PageBreak
    };

    struct TextElement
    {
        TextElementKind kind = TextElementKind::Character;
        char32_t code = 0;
        u16 style = 0;
        // The byte in the source text where the element starts.
        u32 source = 0;
        // Sprites: the asset (empty: the default), the sprite's name or index.
        String spriteAsset, spriteName;
        i32 spriteIndex = -1;
        TextLength length;
    };

    struct TextLink
    {
        String id;
        u32 firstElement = 0, count = 0;
    };

    // Named styles (<style="Name">): the opening and closing tag text they stand for.
    class TextStyleSheetLookup
    {
    public:
        virtual ~TextStyleSheetLookup() = default;
        virtual bool Find(StringView name, String& opening, String& closing) const = 0;
    };

    struct ParsedText
    {
        Array<TextElement> elements;
        Array<TextStyle> styles;
        Array<TextLink> links;
        // The characters only (sprites, spaces and positions as U+FFFC, page breaks as U+000C), for shaping and breaking.
        Array<char32_t> text;
    };

    // Parses UTF-8 text with tags (richText false: every character as written, one style).
    void ParseRichText(StringView source, bool richText, const TextStyle& base, ParsedText& out, const TextStyleSheetLookup* styles = nullptr);

    // A color as tags write it: #RGB, #RGBA, #RRGGBB, #RRGGBBAA or a name (red, green, blue, ...). False when it is none.
    bool ParseTextColor(StringView text, Color& out);
}
