#pragma once

#include <Helix/Core/Basics/Types.h>
#include <Helix/Core/Strings/StringView.h>

// The character properties the text system needs, from the Unicode Character Database (ThirdParty/Unicode): bidi
// class, line break class, East Asian width, general category, joining type, script and Extended_Pictographic; bracket
// pairs and mirrored characters. Values are the database's short names.
namespace helix::ui
{
    enum class BidiClass : u8
    {
        L, R, AL, EN, ES, ET, AN, CS, NSM, BN, B, S, WS, ON, LRE, LRO, RLE, RLO, PDF, LRI, RLI, FSI, PDI,
        Count
    };

    enum class LineBreakClass : u8
    {
        BK, CR, LF, CM, NL, SG, WJ, ZW, GL, SP, ZWJ, B2, BA, BB, HY, CB, CL, CP, EX, IN, NS, OP, QU, IS, NU, PO, PR, SY,
        AI, AK, AL, AP, AS, CJ, EB, EM, H2, H3, HL, HH, ID, JL, JV, JT, RI, SA, VF, VI, XX,
        Count
    };

    enum class EastAsianWidth : u8
    {
        N, A, H, W, F, Na
    };

    enum class GeneralCategory : u8
    {
        Lu, Ll, Lt, Lm, Lo, Mn, Mc, Me, Nd, Nl, No, Pc, Pd, Ps, Pe, Pi, Pf, Po, Sm, Sc, Sk, So, Zs, Zl, Zp, Cc, Cf, Cs, Co,
        Cn
    };

    enum class JoiningType : u8
    {
        U, R, L, D, C, T
    };

    // An index into the scripts (ISO 15924 codes), Common (Zyyy) first, then Inherited (Zinh) and Unknown (Zzzz).
    using Script = u8;
    inline constexpr Script kScriptCommon = 0, kScriptInherited = 1, kScriptUnknown = 2;

    struct CharProperties
    {
        BidiClass bidi;
        LineBreakClass lineBreak;
        EastAsianWidth width;
        GeneralCategory category;
        JoiningType joining;
        Script script;
        u8 pictographic;
    };

    enum class BracketType : u8
    {
        None, Open, Close
    };

    struct BracketEntry
    {
        char32_t code;
        char32_t pair;
        BracketType type;
    };

    struct MirrorEntry
    {
        char32_t code;
        char32_t mirror;
    };

    // The properties of a code point (those of an unassigned one beyond U+10FFFF).
    const CharProperties& PropertiesOf(char32_t c);
    // Its bracket pair (0 when it is no bracket) and whether it opens or closes.
    char32_t BracketPairOf(char32_t c, BracketType& type);
    // Its mirrored form (itself when it has none).
    char32_t MirrorOf(char32_t c);
    // The ISO 15924 code of a script ("Latn") and a script by its code (kScriptUnknown when there is none).
    StringView ScriptCode(Script script);
    Script FindScript(StringView code);
    u32 ScriptCount();
    StringView UnicodeVersion();

    // UTF-8 to code points: the code point at `at` and the index after it (U+FFFD for a malformed sequence, one byte).
    char32_t DecodeUtf8(StringView text, usize& at);
}
