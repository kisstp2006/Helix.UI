#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/UI/Text/Unicode/Unicode.h>

// The Unicode bidirectional algorithm (UAX 9): embedding levels of a text's characters, split into paragraphs at
// paragraph separators, and per line the levels after L1 and the visual order (L2). Characters removed by X9
// (embeddings, overrides, PDF, BN) keep the level of the character before them and are marked.
namespace helix::ui
{
    enum class BidiDirection : i8
    {
        Auto = -1,
        LeftToRight = 0,
        RightToLeft = 1
    };

    class Bidi
    {
    public:
        // The classes from the code points' properties.
        void Resolve(Span<const char32_t> text, BidiDirection direction);
        // Given classes (the text gives the brackets of N0; it may be empty).
        void Resolve(Span<const char32_t> text, Span<const BidiClass> classes, BidiDirection direction);

        u32 Size() const { return u32(levels_.Size()); }
        // The level of each character after the implicit rules (before L1).
        Span<const u8> Levels() const { return Span<const u8>(levels_.Data(), levels_.Size()); }
        // The level of the paragraph a character is in, and of the first paragraph.
        u8 ParagraphLevelAt(u32 index) const { return paragraphLevels_[index]; }
        u8 ParagraphLevel() const { return paragraphLevels_.Empty() ? 0 : paragraphLevels_[0]; }
        bool Removed(u32 index) const { return removed_[index] != 0; }

        // The levels of the line [start, end) after L1 (separators and trailing white space at the paragraph level).
        void LineLevels(u32 start, u32 end, Array<u8>& out) const;
        // The visual order of the line: the logical indices (relative to start) from left to right.
        void LineOrder(u32 start, u32 end, Array<u32>& out) const;
        // A visual order from levels (L2): the highest level's runs reversed, down to the lowest odd level.
        static void Reorder(Span<const u8> levels, Array<u32>& out);

    private:
        void ResolveParagraph(u32 start, u32 end, BidiDirection direction);
        u8 FirstStrongLevel(u32 start, u32 end, u8 fallback) const;
        void ResolveSequence(const Array<u32>& indices, BidiClass sos, BidiClass eos, u8 level);

        Array<char32_t> text_;
        Array<BidiClass> initial_;
        Array<BidiClass> types_;
        Array<u8> levels_;
        Array<u8> paragraphLevels_;
        Array<u8> removed_;
        Array<u32> matchingPdi_;
        Array<u32> matchingInitiator_;
    };
}
