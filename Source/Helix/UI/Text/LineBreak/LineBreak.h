#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/UI/Text/Unicode/Unicode.h>

// The Unicode line breaking algorithm (UAX 14): the break opportunity before each character, by the rules LB1 to LB31
// with the default tailoring, and CJK strictness as its customization of the classes CJ and NS.
namespace helix::ui
{
    enum class LineBreak : u8
    {
        Prohibited,
        Allowed,
        Mandatory
    };

    // How strictly Japanese and Chinese lines avoid starting with small kana, iteration marks and the like (kinsoku):
    // Strict keeps CJ as NS (the default), Normal resolves CJ to ID, Loose also lets wide NS characters start a line.
    enum class LineBreakStrictness : u8
    {
        Strict,
        Normal,
        Loose
    };

    // out[i] is the opportunity between characters i - 1 and i: out[0] Prohibited (sot), out[n] Mandatory (eot).
    void FindLineBreaks(Span<const char32_t> text, Array<LineBreak>& out, LineBreakStrictness strictness = LineBreakStrictness::Strict);
}
