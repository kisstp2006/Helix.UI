#include <Helix/UI/Text/Bidi/Bidi.h>

namespace helix::ui
{
    namespace
    {
        constexpr u32 kNone = ~0u;
        constexpr u8 kMaxDepth = 125;
        using B = BidiClass;

        bool IsInitiator(B t) { return t == B::LRI || t == B::RLI || t == B::FSI; }
        bool IsRemoved(B t) { return t == B::LRE || t == B::RLE || t == B::LRO || t == B::RLO || t == B::PDF || t == B::BN; }
        bool IsNeutral(B t) { return t == B::B || t == B::S || t == B::WS || t == B::ON || IsInitiator(t) || t == B::PDI; }
        B Direction(u32 level) { return (level & 1) ? B::R : B::L; }

        // The strong direction N0 and N1 see: EN and AN count as R; B::ON for none.
        B StrongForNeutrals(B t)
        {
            if (t == B::L)
                return B::L;
            if (t == B::R || t == B::EN || t == B::AN)
                return B::R;
            return B::ON;
        }

        // Brackets compare by canonical equivalence (the angle brackets U+2329 and U+232A are U+3008 and U+3009).
        char32_t CanonicalBracket(char32_t c) { return c == 0x2329 ? 0x3008 : c == 0x232A ? 0x3009 : c; }
    }

    void Bidi::Resolve(Span<const char32_t> text, BidiDirection direction)
    {
        Array<BidiClass> classes;
        classes.Resize(text.Size());
        for (usize i = 0; i < text.Size(); i++)
            classes[i] = PropertiesOf(text[i]).bidi;
        Resolve(text, Span<const BidiClass>(classes.Data(), classes.Size()), direction);
    }

    void Bidi::Resolve(Span<const char32_t> text, Span<const BidiClass> classes, BidiDirection direction)
    {
        u32 n = u32(classes.Size());
        text_.Clear();
        for (char32_t c : text)
            text_.Push(c);
        initial_.Clear();
        for (BidiClass c : classes)
            initial_.Push(c);
        types_ = initial_;
        levels_.Clear();
        levels_.Resize(n);
        paragraphLevels_.Clear();
        paragraphLevels_.Resize(n);
        removed_.Clear();
        removed_.Resize(n);
        matchingPdi_.Clear();
        matchingPdi_.Resize(n);
        matchingInitiator_.Clear();
        matchingInitiator_.Resize(n);
        // P1: paragraphs end after each paragraph separator.
        u32 start = 0;
        for (u32 i = 0; i < n; i++)
            if (initial_[i] == B::B)
            {
                ResolveParagraph(start, i + 1, direction);
                start = i + 1;
            }
        if (start < n)
            ResolveParagraph(start, n, direction);
    }

    u8 Bidi::FirstStrongLevel(u32 start, u32 end, u8 fallback) const
    {
        for (u32 i = start; i < end; i++)
        {
            B t = initial_[i];
            if (t == B::L)
                return 0;
            if (t == B::R || t == B::AL)
                return 1;
            if (IsInitiator(t))
            {
                if (matchingPdi_[i] == kNone)
                    return fallback;
                i = matchingPdi_[i];
            }
        }
        return fallback;
    }

    void Bidi::ResolveParagraph(u32 start, u32 end, BidiDirection direction)
    {
        // BD9: the isolate initiators and their matching PDIs.
        Array<u32> open;
        for (u32 i = start; i < end; i++)
        {
            matchingPdi_[i] = matchingInitiator_[i] = kNone;
            if (IsInitiator(initial_[i]))
                open.Push(i);
            else if (initial_[i] == B::PDI && !open.Empty())
            {
                u32 m = open.Back();
                open.Pop();
                matchingPdi_[m] = i;
                matchingInitiator_[i] = m;
            }
        }
        // P2, P3.
        u8 para = direction == BidiDirection::Auto ? FirstStrongLevel(start, end, 0) : u8(direction);
        for (u32 i = start; i < end; i++)
            paragraphLevels_[i] = para;

        // X1 to X8: the directional status stack.
        struct Entry
        {
            u8 level;
            B override;
            bool isolate;
        };
        Entry stack[kMaxDepth + 2];
        u32 depth = 1;
        stack[0] = {para, B::ON, false};
        u32 overflowIsolates = 0, overflowEmbeddings = 0, validIsolates = 0;
        for (u32 i = start; i < end; i++)
        {
            B t = types_[i];
            const Entry& top = stack[depth - 1];
            switch (t)
            {
            case B::RLE:
            case B::LRE:
            case B::RLO:
            case B::LRO:
            case B::RLI:
            case B::LRI:
            case B::FSI:
            {
                bool isolate = IsInitiator(t);
                bool rtl = t == B::RLE || t == B::RLO || t == B::RLI;
                if (t == B::FSI)
                    rtl = FirstStrongLevel(i + 1, matchingPdi_[i] == kNone ? end : matchingPdi_[i], 0) == 1;
                levels_[i] = top.level;
                if (isolate && top.override != B::ON)
                    types_[i] = top.override;
                u8 level = rtl ? u8((top.level + 1) | 1) : u8((top.level + 2) & ~1);
                if (level <= kMaxDepth && overflowIsolates == 0 && overflowEmbeddings == 0)
                {
                    if (isolate)
                        validIsolates++;
                    stack[depth++] = {level, t == B::LRO ? B::L : t == B::RLO ? B::R : B::ON, isolate};
                    if (!isolate)
                        levels_[i] = level;
                }
                else if (isolate)
                    overflowIsolates++;
                else if (overflowIsolates == 0)
                    overflowEmbeddings++;
                break;
            }
            case B::PDI:
                if (overflowIsolates > 0)
                    overflowIsolates--;
                else if (validIsolates > 0)
                {
                    overflowEmbeddings = 0;
                    while (!stack[depth - 1].isolate)
                        depth--;
                    depth--;
                    validIsolates--;
                }
                levels_[i] = stack[depth - 1].level;
                if (stack[depth - 1].override != B::ON)
                    types_[i] = stack[depth - 1].override;
                break;
            case B::PDF:
                if (overflowIsolates > 0)
                {
                }
                else if (overflowEmbeddings > 0)
                    overflowEmbeddings--;
                else if (!top.isolate && depth >= 2)
                    depth--;
                levels_[i] = stack[depth - 1].level;
                break;
            case B::B:
                levels_[i] = para;
                break;
            case B::BN:
                levels_[i] = top.level;
                break;
            default:
                levels_[i] = top.level;
                if (top.override != B::ON)
                    types_[i] = top.override;
                break;
            }
        }
        // X9.
        for (u32 i = start; i < end; i++)
            removed_[i] = IsRemoved(initial_[i]) ? 1 : 0;

        // X10: level runs, chained into isolating run sequences through the isolates; sos and eos from the explicit
        // levels (the implicit ones of earlier sequences must not leak into them).
        Array<u8> explicitLevels;
        explicitLevels.Resize(end - start);
        for (u32 i = start; i < end; i++)
            explicitLevels[i - start] = levels_[i];
        auto levelAt = [&](u32 i) { return explicitLevels[i - start]; };
        Array<u32> runStart, runOf;
        Array<u32> order;
        runOf.Resize(end - start);
        for (u32 i = start; i < end; i++)
        {
            if (removed_[i])
                continue;
            if (order.Empty() || levels_[order.Back()] != levels_[i])
                runStart.Push(u32(order.Size()));
            runOf[i - start] = u32(runStart.Size() - 1);
            order.Push(i);
        }
        runStart.Push(u32(order.Size()));
        Array<u32> sequence;
        for (u32 r = 0; r + 1 < runStart.Size(); r++)
        {
            u32 first = order[runStart[r]];
            if (initial_[first] == B::PDI && matchingInitiator_[first] != kNone)
                continue;
            sequence.Clear();
            u32 run = r;
            for (;;)
            {
                for (u32 k = runStart[run]; k < runStart[run + 1]; k++)
                    sequence.Push(order[k]);
                u32 last = sequence.Back();
                if (!IsInitiator(initial_[last]) || matchingPdi_[last] == kNone)
                    break;
                run = runOf[matchingPdi_[last] - start];
            }
            u8 level = levelAt(sequence[0]);
            i64 prev = i64(sequence[0]) - 1;
            while (prev >= i64(start) && removed_[u32(prev)])
                prev--;
            u8 before = prev >= i64(start) ? levelAt(u32(prev)) : para;
            u32 last = sequence.Back();
            u8 after = para;
            if (!IsInitiator(initial_[last]))
            {
                u32 next = last + 1;
                while (next < end && removed_[next])
                    next++;
                if (next < end)
                    after = levelAt(next);
            }
            ResolveSequence(sequence, Direction(before > level ? before : level), Direction(after > level ? after : level), level);
        }
        // Removed characters take the level of the one before them.
        for (u32 i = start; i < end; i++)
            if (removed_[i])
                levels_[i] = i > start ? levels_[i - 1] : para;
    }

    void Bidi::ResolveSequence(const Array<u32>& indices, BidiClass sos, BidiClass eos, u8 level)
    {
        u32 n = u32(indices.Size());
        Array<B> t;
        t.Resize(n);
        for (u32 k = 0; k < n; k++)
            t[k] = types_[indices[k]];

        // W1.
        for (u32 k = 0; k < n; k++)
            if (t[k] == B::NSM)
            {
                if (k == 0)
                    t[k] = sos;
                else
                    t[k] = IsInitiator(t[k - 1]) || t[k - 1] == B::PDI ? B::ON : t[k - 1];
            }
        // W2, W3.
        B strong = sos;
        for (u32 k = 0; k < n; k++)
        {
            if (t[k] == B::L || t[k] == B::R || t[k] == B::AL)
                strong = t[k];
            if (t[k] == B::EN && strong == B::AL)
                t[k] = B::AN;
        }
        for (u32 k = 0; k < n; k++)
            if (t[k] == B::AL)
                t[k] = B::R;
        // W4.
        for (u32 k = 1; k + 1 < n; k++)
        {
            if (t[k] == B::ES && t[k - 1] == B::EN && t[k + 1] == B::EN)
                t[k] = B::EN;
            else if (t[k] == B::CS && t[k - 1] == B::EN && t[k + 1] == B::EN)
                t[k] = B::EN;
            else if (t[k] == B::CS && t[k - 1] == B::AN && t[k + 1] == B::AN)
                t[k] = B::AN;
        }
        // W5.
        for (u32 k = 0; k < n;)
        {
            if (t[k] != B::ET)
            {
                k++;
                continue;
            }
            u32 e = k;
            while (e < n && t[e] == B::ET)
                e++;
            if ((k > 0 && t[k - 1] == B::EN) || (e < n && t[e] == B::EN))
                for (u32 j = k; j < e; j++)
                    t[j] = B::EN;
            k = e;
        }
        // W6.
        for (u32 k = 0; k < n; k++)
            if (t[k] == B::ES || t[k] == B::ET || t[k] == B::CS)
                t[k] = B::ON;
        // W7.
        strong = sos;
        for (u32 k = 0; k < n; k++)
        {
            if (t[k] == B::L || t[k] == B::R)
                strong = t[k];
            if (t[k] == B::EN && strong == B::L)
                t[k] = B::L;
        }

        B embedding = Direction(level);
        // N0: bracket pairs (BD16), at most 63 open at a time.
        if (!text_.Empty())
        {
            struct Open
            {
                char32_t closing;
                u32 at;
            };
            struct Pair
            {
                u32 open, close;
            };
            Open stack[63];
            u32 depth = 0;
            Array<Pair> pairs;
            for (u32 k = 0; k < n; k++)
            {
                if (t[k] != B::ON)
                    continue;
                char32_t c = text_[indices[k]];
                BracketType type;
                char32_t pair = BracketPairOf(c, type);
                if (type == BracketType::Open)
                {
                    if (depth == 63)
                        break;
                    stack[depth++] = {CanonicalBracket(pair), k};
                }
                else if (type == BracketType::Close)
                {
                    char32_t canonical = CanonicalBracket(c);
                    for (u32 s = depth; s-- > 0;)
                        if (stack[s].closing == canonical)
                        {
                            pairs.Push({stack[s].at, k});
                            depth = s;
                            break;
                        }
                }
            }
            for (u32 i = 1; i < pairs.Size(); i++)
                for (u32 j = i; j > 0 && pairs[j - 1].open > pairs[j].open; j--)
                {
                    Pair p = pairs[j];
                    pairs[j] = pairs[j - 1];
                    pairs[j - 1] = p;
                }
            for (const Pair& p : pairs)
            {
                bool same = false, opposite = false;
                for (u32 k = p.open + 1; k < p.close; k++)
                {
                    B d = StrongForNeutrals(t[k]);
                    same = same || d == embedding;
                    opposite = opposite || (d != B::ON && d != embedding);
                }
                B set = B::ON;
                if (same)
                    set = embedding;
                else if (opposite)
                {
                    B context = sos;
                    for (u32 k = p.open; k-- > 0;)
                        if (B d = StrongForNeutrals(t[k]); d != B::ON)
                        {
                            context = d;
                            break;
                        }
                    set = context;
                }
                if (set == B::ON)
                    continue;
                for (u32 at : {p.open, p.close})
                {
                    t[at] = set;
                    for (u32 k = at + 1; k < n && initial_[indices[k]] == B::NSM; k++)
                        t[k] = set;
                }
            }
        }
        // N1, N2.
        for (u32 k = 0; k < n;)
        {
            if (!IsNeutral(t[k]))
            {
                k++;
                continue;
            }
            u32 e = k;
            while (e < n && IsNeutral(t[e]))
                e++;
            B leading = k == 0 ? sos : StrongForNeutrals(t[k - 1]);
            B trailing = e == n ? eos : StrongForNeutrals(t[e]);
            B set = leading == trailing ? leading : embedding;
            for (u32 j = k; j < e; j++)
                t[j] = set;
            k = e;
        }
        // I1, I2.
        for (u32 k = 0; k < n; k++)
        {
            u8 l = level;
            if ((l & 1) == 0)
                l += t[k] == B::R ? 1 : (t[k] == B::AN || t[k] == B::EN) ? 2 : 0;
            else if (t[k] == B::L || t[k] == B::EN || t[k] == B::AN)
                l += 1;
            levels_[indices[k]] = l;
            types_[indices[k]] = t[k];
        }
    }

    void Bidi::LineLevels(u32 start, u32 end, Array<u8>& out) const
    {
        out.Clear();
        for (u32 i = start; i < end; i++)
            out.Push(levels_[i]);
        // L1: separators, and white space (isolates and removed characters too) before them and at the line's end.
        bool trailing = true;
        for (u32 i = end; i-- > start;)
        {
            B t = initial_[i];
            if (t == B::S || t == B::B)
            {
                out[i - start] = paragraphLevels_[i];
                trailing = true;
            }
            else if (trailing && (t == B::WS || IsInitiator(t) || t == B::PDI || IsRemoved(t)))
                out[i - start] = paragraphLevels_[i];
            else
                trailing = false;
        }
    }

    void Bidi::LineOrder(u32 start, u32 end, Array<u32>& out) const
    {
        Array<u8> levels;
        LineLevels(start, end, levels);
        Reorder(Span<const u8>(levels.Data(), levels.Size()), out);
    }

    void Bidi::Reorder(Span<const u8> levels, Array<u32>& out)
    {
        u32 n = u32(levels.Size());
        out.Clear();
        for (u32 i = 0; i < n; i++)
            out.Push(i);
        u8 highest = 0, lowestOdd = 255;
        for (u8 l : levels)
        {
            highest = l > highest ? l : highest;
            if ((l & 1) && l < lowestOdd)
                lowestOdd = l;
        }
        for (u32 l = highest; l >= lowestOdd && l > 0; l--)
            for (u32 i = 0; i < n;)
            {
                if (levels[out[i]] < l)
                {
                    i++;
                    continue;
                }
                u32 e = i;
                while (e < n && levels[out[e]] >= l)
                    e++;
                for (u32 a = i, b = e - 1; a < b; a++, b--)
                {
                    u32 x = out[a];
                    out[a] = out[b];
                    out[b] = x;
                }
                i = e;
            }
    }
}
