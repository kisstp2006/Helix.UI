#include <Helix/Core/Strings/Format.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/LineBreak/LineBreak.h>

#include <cstdio>
#include <cstdlib>

using namespace helix;
using namespace helix::ui;

namespace
{
    // The opportunities as the file writes them: '/' for a break (the file's division sign), 'x' for none.
    String Marks(const Array<LineBreak>& breaks)
    {
        String s;
        for (LineBreak b : breaks)
            s += b == LineBreak::Prohibited ? "x" : "/";
        return s;
    }
}

HX_TEST(LineBreak, EveryCaseOfLineBreakTest)
{
    Array<u8> bytes;
    HX_REQUIRE(platform::ReadFile(platform::JoinPath(HELIX_UI_UNICODE_DIR, "LineBreakTest.txt"), bytes));
    StringView all(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    u32 cases = 0, failed = 0;
    Array<char32_t> text;
    Array<LineBreak> breaks;
    usize start = 0;
    for (usize i = 0; i <= all.Size(); i++)
    {
        if (i < all.Size() && all[i] != '\n')
            continue;
        StringView line = all.Substr(start, i - start);
        start = i + 1;
        for (usize h = 0; h < line.Size(); h++)
            if (line[h] == '#')
            {
                line = line.Substr(0, h);
                break;
            }
        // The division sign (C3 B7) and the multiplication sign (C3 97) between hex code points.
        String expected;
        text.Clear();
        for (usize k = 0; k < line.Size();)
        {
            u8 c = u8(line[k]);
            if (c == 0xC3 && k + 1 < line.Size())
            {
                expected += u8(line[k + 1]) == 0xB7 ? "/" : "x";
                k += 2;
            }
            else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
            {
                usize e = k;
                while (e < line.Size() && line[e] != ' ' && line[e] != '\t')
                    e++;
                text.Push(char32_t(std::strtoul(String(line.Substr(k, e - k)).Data(), nullptr, 16)));
                k = e;
            }
            else
                k++;
        }
        if (text.Empty())
            continue;
        cases++;
        FindLineBreaks(Span<const char32_t>(text.Data(), text.Size()), breaks);
        String got = Marks(breaks);
        if (got != expected && failed++ < 12)
            std::printf("    %.*s: %s / %s\n", int(line.Size()), line.Data(), got.Data(), expected.Data());
    }
    std::printf("    LineBreakTest: %u cases, %u failed\n", cases, failed);
    HX_CHECK(cases > 19000 && failed == 0);
}

HX_TEST(LineBreak, WordsSpacesAndCjkStrictness)
{
    // "a b-c": after the space and after the hyphen.
    const char32_t latin[] = {'a', ' ', 'b', '-', 'c'};
    Array<LineBreak> b;
    FindLineBreaks(Span<const char32_t>(latin, 5), b);
    HX_CHECK(b[0] == LineBreak::Prohibited && b[1] == LineBreak::Prohibited && b[2] == LineBreak::Allowed && b[3] == LineBreak::Prohibited &&
             b[4] == LineBreak::Allowed && b[5] == LineBreak::Mandatory);
    // A line feed is a mandatory break.
    const char32_t lines[] = {'a', '\n', 'b'};
    FindLineBreaks(Span<const char32_t>(lines, 3), b);
    HX_CHECK(b[1] == LineBreak::Prohibited && b[2] == LineBreak::Mandatory);
    // Ideographs break anywhere; a small kana (CJ) may not start a line when strict, may when normal.
    const char32_t cjk[] = {0x6F22, 0x5B57, 0x30A1};
    FindLineBreaks(Span<const char32_t>(cjk, 3), b);
    HX_CHECK(b[1] == LineBreak::Allowed && b[2] == LineBreak::Prohibited);
    FindLineBreaks(Span<const char32_t>(cjk, 3), b, LineBreakStrictness::Normal);
    HX_CHECK(b[2] == LineBreak::Allowed);
    // An ideographic full stop never starts a line.
    const char32_t stop[] = {0x6F22, 0x3002};
    FindLineBreaks(Span<const char32_t>(stop, 2), b, LineBreakStrictness::Loose);
    HX_CHECK(b[1] == LineBreak::Prohibited);
}
