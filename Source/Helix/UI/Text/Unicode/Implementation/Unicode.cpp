#include <Helix/UI/Text/Unicode/Unicode.h>

namespace helix::ui
{
    namespace
    {
#include "UnicodeTables.inl"

        constexpr CharProperties kBeyond{BidiClass::L, LineBreakClass::XX, EastAsianWidth::N, GeneralCategory::Cn, JoiningType::U, kScriptUnknown, 0};
    }

    const CharProperties& PropertiesOf(char32_t c)
    {
        if (c >= 0x110000)
            return kBeyond;
        return kRecords[kStage2[u32(kStage1[c / kUnicodeBlock]) * kUnicodeBlock + c % kUnicodeBlock]];
    }

    char32_t BracketPairOf(char32_t c, BracketType& type)
    {
        usize lo = 0, hi = sizeof(kBrackets) / sizeof(kBrackets[0]);
        while (lo < hi)
        {
            usize mid = (lo + hi) / 2;
            if (kBrackets[mid].code < c)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo < sizeof(kBrackets) / sizeof(kBrackets[0]) && kBrackets[lo].code == c)
        {
            type = kBrackets[lo].type;
            return kBrackets[lo].pair;
        }
        type = BracketType::None;
        return 0;
    }

    char32_t MirrorOf(char32_t c)
    {
        usize lo = 0, hi = sizeof(kMirrors) / sizeof(kMirrors[0]);
        while (lo < hi)
        {
            usize mid = (lo + hi) / 2;
            if (kMirrors[mid].code < c)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo < sizeof(kMirrors) / sizeof(kMirrors[0]) && kMirrors[lo].code == c ? kMirrors[lo].mirror : c;
    }

    StringView ScriptCode(Script script) { return script < kScriptCount ? StringView(kScriptCodes[script], 4) : StringView(); }

    Script FindScript(StringView code)
    {
        for (u32 i = 0; i < kScriptCount; i++)
            if (StringView(kScriptCodes[i], 4) == code)
                return Script(i);
        return kScriptUnknown;
    }

    u32 ScriptCount() { return kScriptCount; }

    StringView UnicodeVersion() { return StringView(kUnicodeVersion); }

    char32_t DecodeUtf8(StringView text, usize& at)
    {
        auto byte = [&](usize i) { return u8(text[i]); };
        u8 b0 = byte(at);
        u32 need = b0 < 0x80 ? 0 : (b0 & 0xE0) == 0xC0 ? 1 : (b0 & 0xF0) == 0xE0 ? 2 : (b0 & 0xF8) == 0xF0 ? 3 : 4;
        if (need == 0)
            return char32_t(text[at++] & 0x7F);
        if (need == 4 || at + need >= text.Size())
        {
            at++;
            return 0xFFFD;
        }
        char32_t c = b0 & (0x3F >> need);
        for (u32 i = 1; i <= need; i++)
        {
            u8 b = byte(at + i);
            if ((b & 0xC0) != 0x80)
            {
                at++;
                return 0xFFFD;
            }
            c = (c << 6) | (b & 0x3F);
        }
        constexpr char32_t kMin[] = {0, 0x80, 0x800, 0x10000};
        if (c < kMin[need] || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
        {
            at++;
            return 0xFFFD;
        }
        at += need + 1;
        return c;
    }
}
