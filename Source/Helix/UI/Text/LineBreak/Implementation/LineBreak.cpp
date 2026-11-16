#include <Helix/UI/Text/LineBreak/LineBreak.h>

namespace helix::ui
{
    namespace
    {
        using C = LineBreakClass;
        constexpr char32_t kDottedCircle = 0x25CC;

        // A character after LB9: a base with its combining marks, with the base's properties.
        struct Unit
        {
            C cls;
            GeneralCategory category;
            bool eastAsian;
            bool pictographic;
            char32_t code;
            // The first and last character of the unit.
            u32 first, last;
        };

        bool In(C c, std::initializer_list<C> set)
        {
            for (C s : set)
                if (c == s)
                    return true;
            return false;
        }

        C Resolve(char32_t code, const CharProperties& p, LineBreakStrictness strictness)
        {
            switch (p.lineBreak)
            {
            case C::AI:
            case C::SG:
            case C::XX:
                return C::AL;
            case C::SA:
                return p.category == GeneralCategory::Mn || p.category == GeneralCategory::Mc ? C::CM : C::AL;
            case C::CJ:
                return strictness == LineBreakStrictness::Strict ? C::NS : C::ID;
            case C::NS:
                return strictness == LineBreakStrictness::Loose && (p.width == EastAsianWidth::W || p.width == EastAsianWidth::F) ? C::ID : C::NS;
            default:
                (void)code;
                return p.lineBreak;
            }
        }
    }

    void FindLineBreaks(Span<const char32_t> text, Array<LineBreak>& out, LineBreakStrictness strictness)
    {
        u32 n = u32(text.Size());
        out.Clear();
        out.Resize(n + 1);
        for (u32 i = 0; i <= n; i++)
            out[i] = LineBreak::Prohibited;
        if (n == 0)
            return;
        out[n] = LineBreak::Mandatory;

        // LB1, LB9, LB10: units of a base and its combining marks (ZWJ is one too).
        Array<Unit> units;
        for (u32 i = 0; i < n; i++)
        {
            const CharProperties& p = PropertiesOf(text[i]);
            C c = Resolve(text[i], p, strictness);
            if ((c == C::CM || c == C::ZWJ) && !units.Empty() && !In(units.Back().cls, {C::BK, C::CR, C::LF, C::NL, C::SP, C::ZW}))
            {
                units.Back().last = i;
                continue;
            }
            Unit u;
            u.first = u.last = i;
            u.code = text[i];
            if (c == C::CM || c == C::ZWJ)
            {
                u.cls = C::AL;
                u.category = GeneralCategory::Lu;
                u.eastAsian = false;
                u.pictographic = false;
            }
            else
            {
                u.cls = c;
                u.category = p.category;
                u.eastAsian = p.width == EastAsianWidth::F || p.width == EastAsianWidth::W || p.width == EastAsianWidth::H;
                u.pictographic = p.pictographic != 0;
            }
            units.Push(u);
        }

        u32 count = u32(units.Size());
        auto cls = [&](i64 k) { return k >= 0 && k < i64(count) ? units[u32(k)].cls : C::Count; };
        // The unit before k's run of spaces (k itself when it is no space).
        auto beforeSpaces = [&](i64 k) {
            while (k >= 0 && units[u32(k)].cls == C::SP)
                k--;
            return k;
        };
        auto akLike = [&](i64 k) { return k >= 0 && k < i64(count) && (In(units[u32(k)].cls, {C::AK, C::AS}) || units[u32(k)].code == kDottedCircle); };
        // NU (SY | IS)* ending at k.
        auto numberBefore = [&](i64 k) {
            while (k >= 0 && In(units[u32(k)].cls, {C::SY, C::IS}))
                k--;
            return k >= 0 && units[u32(k)].cls == C::NU;
        };

        for (u32 q = 1; q < count; q++)
        {
            u32 p = q - 1;
            const Unit& a = units[p];
            const Unit& b = units[q];
            C A = a.cls, B = b.cls;
            LineBreak result = LineBreak::Allowed;
            auto decide = [&]() -> LineBreak {
                // LB4, LB5.
                if (A == C::BK)
                    return LineBreak::Mandatory;
                if (A == C::CR && B == C::LF)
                    return LineBreak::Prohibited;
                if (In(A, {C::CR, C::LF, C::NL}))
                    return LineBreak::Mandatory;
                // LB6, LB7.
                if (In(B, {C::BK, C::CR, C::LF, C::NL, C::SP, C::ZW}))
                    return LineBreak::Prohibited;
                // LB8.
                if (cls(beforeSpaces(p)) == C::ZW)
                    return LineBreak::Allowed;
                // LB8a: after a zero width joiner (the last character before the position).
                if (PropertiesOf(text[a.last]).lineBreak == C::ZWJ)
                    return LineBreak::Prohibited;
                // LB11, LB12, LB12a.
                if (A == C::WJ || B == C::WJ || A == C::GL)
                    return LineBreak::Prohibited;
                if (B == C::GL && !In(A, {C::SP, C::HY, C::HH}))
                    return LineBreak::Prohibited;
                // LB13, LB14.
                if (In(B, {C::CL, C::CP, C::EX, C::SY}))
                    return LineBreak::Prohibited;
                i64 s = beforeSpaces(p);
                if (cls(s) == C::OP)
                    return LineBreak::Prohibited;
                // LB15a.
                if (cls(s) == C::QU && units[u32(s)].category == GeneralCategory::Pi &&
                    (s == 0 || In(cls(s - 1), {C::BK, C::CR, C::LF, C::NL, C::OP, C::QU, C::GL, C::SP, C::ZW})))
                    return LineBreak::Prohibited;
                // LB15b.
                if (B == C::QU && b.category == GeneralCategory::Pf &&
                    (q + 1 == count ||
                     In(cls(q + 1), {C::SP, C::GL, C::WJ, C::CL, C::QU, C::CP, C::EX, C::IS, C::SY, C::BK, C::CR, C::LF, C::NL, C::ZW})))
                    return LineBreak::Prohibited;
                // LB15c, LB15d.
                if (A == C::SP && B == C::IS && cls(q + 1) == C::NU)
                    return LineBreak::Allowed;
                if (B == C::IS)
                    return LineBreak::Prohibited;
                // LB16, LB17.
                if (In(cls(s), {C::CL, C::CP}) && B == C::NS)
                    return LineBreak::Prohibited;
                if (cls(s) == C::B2 && B == C::B2)
                    return LineBreak::Prohibited;
                // LB18.
                if (A == C::SP)
                    return LineBreak::Allowed;
                // LB19.
                if (B == C::QU && b.category != GeneralCategory::Pi)
                    return LineBreak::Prohibited;
                if (A == C::QU && a.category != GeneralCategory::Pf)
                    return LineBreak::Prohibited;
                // LB19a.
                if (B == C::QU && (!a.eastAsian || q + 1 == count || !units[q + 1].eastAsian))
                    return LineBreak::Prohibited;
                if (A == C::QU && (!b.eastAsian || p == 0 || !units[p - 1].eastAsian))
                    return LineBreak::Prohibited;
                // LB20.
                if (A == C::CB || B == C::CB)
                    return LineBreak::Allowed;
                // LB20a.
                if (In(A, {C::HY, C::HH}) && In(B, {C::AL, C::HL}) &&
                    (p == 0 || In(cls(i64(p) - 1), {C::BK, C::CR, C::LF, C::NL, C::SP, C::ZW, C::CB, C::GL})))
                    return LineBreak::Prohibited;
                // LB21, LB21a, LB21b, LB22.
                if (In(B, {C::BA, C::HH, C::HY, C::NS}) || A == C::BB)
                    return LineBreak::Prohibited;
                if (p > 0 && cls(i64(p) - 1) == C::HL && In(A, {C::HY, C::HH}) && B != C::HL)
                    return LineBreak::Prohibited;
                if ((A == C::SY && B == C::HL) || B == C::IN)
                    return LineBreak::Prohibited;
                // LB23, LB23a, LB24.
                if ((In(A, {C::AL, C::HL}) && B == C::NU) || (A == C::NU && In(B, {C::AL, C::HL})))
                    return LineBreak::Prohibited;
                if ((A == C::PR && In(B, {C::ID, C::EB, C::EM})) || (In(A, {C::ID, C::EB, C::EM}) && B == C::PO))
                    return LineBreak::Prohibited;
                if ((In(A, {C::PR, C::PO}) && In(B, {C::AL, C::HL})) || (In(A, {C::AL, C::HL}) && In(B, {C::PR, C::PO})))
                    return LineBreak::Prohibited;
                // LB25.
                if (In(B, {C::PO, C::PR}) && (numberBefore(p) || (In(A, {C::CL, C::CP}) && numberBefore(i64(p) - 1))))
                    return LineBreak::Prohibited;
                if (In(A, {C::PO, C::PR}) && B == C::OP && (cls(q + 1) == C::NU || (cls(q + 1) == C::IS && cls(q + 2) == C::NU)))
                    return LineBreak::Prohibited;
                if (B == C::NU && (In(A, {C::PO, C::PR, C::HY, C::IS}) || numberBefore(p)))
                    return LineBreak::Prohibited;
                // LB26, LB27.
                if ((A == C::JL && In(B, {C::JL, C::JV, C::H2, C::H3})) || (In(A, {C::JV, C::H2}) && In(B, {C::JV, C::JT})) ||
                    (In(A, {C::JT, C::H3}) && B == C::JT))
                    return LineBreak::Prohibited;
                if ((In(A, {C::JL, C::JV, C::JT, C::H2, C::H3}) && B == C::PO) || (A == C::PR && In(B, {C::JL, C::JV, C::JT, C::H2, C::H3})))
                    return LineBreak::Prohibited;
                // LB28.
                if (In(A, {C::AL, C::HL}) && In(B, {C::AL, C::HL}))
                    return LineBreak::Prohibited;
                // LB28a.
                if (A == C::AP && akLike(q))
                    return LineBreak::Prohibited;
                if (akLike(p) && In(B, {C::VF, C::VI}))
                    return LineBreak::Prohibited;
                if (A == C::VI && akLike(i64(p) - 1) && (B == C::AK || b.code == kDottedCircle))
                    return LineBreak::Prohibited;
                if (akLike(p) && akLike(q) && cls(q + 1) == C::VF)
                    return LineBreak::Prohibited;
                // LB29, LB30.
                if (A == C::IS && In(B, {C::AL, C::HL}))
                    return LineBreak::Prohibited;
                if (In(A, {C::AL, C::HL, C::NU}) && B == C::OP && !b.eastAsian)
                    return LineBreak::Prohibited;
                if (A == C::CP && !a.eastAsian && In(B, {C::AL, C::HL, C::NU}))
                    return LineBreak::Prohibited;
                // LB30a: regional indicators in pairs.
                if (A == C::RI && B == C::RI)
                {
                    u32 run = 0;
                    for (i64 k = p; k >= 0 && units[u32(k)].cls == C::RI; k--)
                        run++;
                    return (run & 1) ? LineBreak::Prohibited : LineBreak::Allowed;
                }
                // LB30b.
                if (B == C::EM && (A == C::EB || (a.pictographic && a.category == GeneralCategory::Cn)))
                    return LineBreak::Prohibited;
                // LB31.
                return LineBreak::Allowed;
            };
            result = decide();
            out[b.first] = result;
        }
    }
}
