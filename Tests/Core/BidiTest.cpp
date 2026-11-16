#include <Helix/Core/Strings/Format.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Text/Bidi/Bidi.h>

#include <cstdio>
#include <cstdlib>

using namespace helix;
using namespace helix::ui;

namespace
{
    // The lines of a file of ThirdParty/Unicode.
    bool ReadLines(const char* name, Array<StringView>& lines, Array<u8>& bytes)
    {
        if (!platform::ReadFile(platform::JoinPath(HELIX_UI_UNICODE_DIR, name), bytes))
            return false;
        StringView all(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
        usize start = 0;
        for (usize i = 0; i <= all.Size(); i++)
            if (i == all.Size() || all[i] == '\n')
            {
                StringView line = all.Substr(start, i - start);
                if (!line.Empty() && line[line.Size() - 1] == '\r')
                    line = line.Substr(0, line.Size() - 1);
                lines.Push(line);
                start = i + 1;
            }
        return true;
    }

    void Split(StringView text, char by, Array<StringView>& out)
    {
        out.Clear();
        usize start = 0;
        for (usize i = 0; i <= text.Size(); i++)
            if (i == text.Size() || text[i] == by)
            {
                StringView part = text.Substr(start, i - start);
                while (!part.Empty() && (part[0] == ' ' || part[0] == '\t'))
                    part = part.Substr(1);
                while (!part.Empty() && (part[part.Size() - 1] == ' ' || part[part.Size() - 1] == '\t'))
                    part = part.Substr(0, part.Size() - 1);
                if (!part.Empty() || by != ' ')
                    out.Push(part);
                start = i + 1;
            }
    }

    BidiClass ClassByName(StringView name)
    {
        static const char* kNames[] = {"L", "R", "AL", "EN", "ES", "ET", "AN", "CS", "NSM", "BN", "B", "S", "WS", "ON", "LRE", "LRO", "RLE", "RLO",
                                       "PDF", "LRI", "RLI", "FSI", "PDI"};
        for (u32 i = 0; i < u32(BidiClass::Count); i++)
            if (name == StringView(kNames[i]))
                return BidiClass(i);
        return BidiClass::Count;
    }

    // The resolved levels after L1 (x for removed characters) and the visual order without them, as the files write them.
    void Results(const Bidi& bidi, String& levels, String& order)
    {
        u32 n = bidi.Size();
        Array<u8> line;
        bidi.LineLevels(0, n, line);
        levels.Clear();
        Array<u8> kept;
        Array<u32> keptIndex;
        for (u32 i = 0; i < n; i++)
        {
            if (!levels.Empty())
                levels += " ";
            if (bidi.Removed(i))
                levels += "x";
            else
            {
                levels += Format("{}", u32(line[i]));
                kept.Push(line[i]);
                keptIndex.Push(i);
            }
        }
        Array<u32> visual;
        Bidi::Reorder(Span<const u8>(kept.Data(), kept.Size()), visual);
        order.Clear();
        for (u32 v : visual)
        {
            if (!order.Empty())
                order += " ";
            order += Format("{}", keptIndex[v]);
        }
    }
}

HX_TEST(Bidi, EveryCaseOfBidiTest)
{
    Array<u8> bytes;
    Array<StringView> lines;
    HX_REQUIRE(ReadLines("BidiTest.txt", lines, bytes));
    String levels, order, gotLevels, gotOrder;
    Array<StringView> fields, names;
    Array<BidiClass> classes;
    u32 cases = 0, failed = 0;
    Bidi bidi;
    for (StringView line : lines)
    {
        if (line.Empty() || line[0] == '#')
            continue;
        if (line.StartsWith("@Levels:"))
        {
            Split(line.Substr(8), ' ', fields);
            levels.Clear();
            for (StringView f : fields)
                levels += levels.Empty() ? String(f) : Format(" {}", f);
            continue;
        }
        if (line.StartsWith("@Reorder:"))
        {
            Split(line.Substr(9), ' ', fields);
            order.Clear();
            for (StringView f : fields)
                order += order.Empty() ? String(f) : Format(" {}", f);
            continue;
        }
        Split(line, ';', fields);
        if (fields.Size() != 2)
            continue;
        Split(fields[0], ' ', names);
        classes.Clear();
        for (StringView name : names)
            classes.Push(ClassByName(name));
        u32 set = u32(std::atoi(String(fields[1]).Data()));
        constexpr BidiDirection kDirections[] = {BidiDirection::Auto, BidiDirection::LeftToRight, BidiDirection::RightToLeft};
        for (u32 d = 0; d < 3; d++)
        {
            if (!(set & (1u << d)))
                continue;
            cases++;
            bidi.Resolve(Span<const char32_t>(), Span<const BidiClass>(classes.Data(), classes.Size()), kDirections[d]);
            Results(bidi, gotLevels, gotOrder);
            if (gotLevels != levels || gotOrder != order)
            {
                if (failed++ < 10)
                    std::printf("    %.*s (direction %u): levels %s / %s, order %s / %s\n", int(fields[0].Size()), fields[0].Data(), d, gotLevels.Data(),
                                levels.Data(), gotOrder.Data(), order.Data());
            }
        }
    }
    std::printf("    BidiTest: %u cases, %u failed\n", cases, failed);
    HX_CHECK(cases > 400000 && failed == 0);
}

HX_TEST(Bidi, EveryCaseOfBidiCharacterTest)
{
    Array<u8> bytes;
    Array<StringView> lines;
    HX_REQUIRE(ReadLines("BidiCharacterTest.txt", lines, bytes));
    Array<StringView> fields, codes;
    Array<char32_t> text;
    String gotLevels, gotOrder;
    u32 cases = 0, failed = 0;
    Bidi bidi;
    for (StringView line : lines)
    {
        if (line.Empty() || line[0] == '#')
            continue;
        Split(line, ';', fields);
        if (fields.Size() < 5)
            continue;
        Split(fields[0], ' ', codes);
        text.Clear();
        for (StringView c : codes)
            text.Push(char32_t(std::strtoul(String(c).Data(), nullptr, 16)));
        u32 d = u32(std::atoi(String(fields[1]).Data()));
        BidiDirection direction = d == 0 ? BidiDirection::LeftToRight : d == 1 ? BidiDirection::RightToLeft : BidiDirection::Auto;
        u32 para = u32(std::atoi(String(fields[2]).Data()));
        cases++;
        bidi.Resolve(Span<const char32_t>(text.Data(), text.Size()), direction);
        Results(bidi, gotLevels, gotOrder);
        if (bidi.ParagraphLevel() != para || gotLevels != fields[3] || gotOrder != fields[4])
        {
            if (failed++ < 10)
                std::printf("    %.*s (direction %u): paragraph %u / %u, levels %s / %.*s, order %s / %.*s\n", int(fields[0].Size()), fields[0].Data(), d,
                            u32(bidi.ParagraphLevel()), para, gotLevels.Data(), int(fields[3].Size()), fields[3].Data(), gotOrder.Data(), int(fields[4].Size()),
                            fields[4].Data());
        }
    }
    std::printf("    BidiCharacterTest: %u cases, %u failed\n", cases, failed);
    HX_CHECK(cases > 90000 && failed == 0);
}

HX_TEST(Bidi, MixedTextLevelsAndOrder)
{
    // "abc ABC 123" with ABC Hebrew letters: the Hebrew word and the number after it read right to left as a whole.
    const char32_t text[] = {'a', 'b', 'c', ' ', 0x5D0, 0x5D1, 0x5D2, ' ', '1', '2', '3'};
    Bidi bidi;
    bidi.Resolve(Span<const char32_t>(text, 11), BidiDirection::Auto);
    HX_CHECK(bidi.ParagraphLevel() == 0);
    Array<u32> order;
    bidi.LineOrder(0, 11, order);
    const u32 expected[] = {0, 1, 2, 3, 8, 9, 10, 7, 6, 5, 4};
    bool same = order.Size() == 11;
    for (u32 i = 0; same && i < 11; i++)
        same = order[i] == expected[i];
    HX_CHECK(same);
    // Brackets mirror at odd levels; their pairs are found by N0.
    HX_CHECK(MirrorOf('(') == ')' && MirrorOf(0x2264) == 0x2265 && MirrorOf('a') == 'a');
    BracketType type;
    HX_CHECK(BracketPairOf('[', type) == ']' && type == BracketType::Open && BracketPairOf('a', type) == 0 && type == BracketType::None);
}
