#include <Helix/UI/Text/Shaping/Shaper.h>

namespace helix::ui
{
    namespace
    {
        constexpr u32 kNone = ~0u;
        constexpr u32 kMaxNesting = 64;
        constexpr u8 kAttachMark = 1, kAttachCursive = 2;
        using font::kNoIndex;
        using font::Tag;

        // Mask bits: the global features, then the Arabic forms.
        constexpr u32 kGlobal = 1, kIsol = 2, kFina = 4, kMedi = 8, kInit = 16;

        struct FeatureBit
        {
            u32 tag;
            u32 bit;
            bool gpos;
        };
        constexpr FeatureBit kDefaults[] = {
            {Tag("ccmp"), kGlobal, false}, {Tag("locl"), kGlobal, false}, {Tag("isol"), kIsol, false}, {Tag("fina"), kFina, false},
            {Tag("medi"), kMedi, false},   {Tag("init"), kInit, false},   {Tag("rlig"), kGlobal, false}, {Tag("rclt"), kGlobal, false},
            {Tag("calt"), kGlobal, false}, {Tag("liga"), kGlobal, false}, {Tag("clig"), kGlobal, false}, {Tag("kern"), kGlobal, true},
            {Tag("mark"), kGlobal, true},  {Tag("mkmk"), kGlobal, true},  {Tag("curs"), kGlobal, true},  {Tag("dist"), kGlobal, true},
            {Tag("abvm"), kGlobal, true},  {Tag("blwm"), kGlobal, true},
        };

        bool IsVariationSelector(char32_t c) { return (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF); }

        bool IsMark(char32_t c)
        {
            GeneralCategory g = PropertiesOf(c).category;
            return g == GeneralCategory::Mn || g == GeneralCategory::Me || g == GeneralCategory::Mc;
        }

        // The joining forms of a run (one mask bit per code point; 0 for characters without forms).
        void JoiningForms(Span<const char32_t> text, u32 start, u32 end, Array<u32>& forms)
        {
            forms.Clear();
            forms.Resize(end - start);
            auto type = [&](u32 i) { return PropertiesOf(text[i]).joining; };
            auto joinsNext = [](JoiningType t) { return t == JoiningType::D || t == JoiningType::L || t == JoiningType::C; };
            auto joinsPrev = [](JoiningType t) { return t == JoiningType::D || t == JoiningType::R || t == JoiningType::C; };
            for (u32 i = start; i < end; i++)
            {
                JoiningType t = type(i);
                if (t != JoiningType::D && t != JoiningType::R && t != JoiningType::L)
                    continue;
                // The neighbors, transparent characters skipped.
                JoiningType prev = JoiningType::U, next = JoiningType::U;
                for (u32 k = i; k-- > start;)
                    if (JoiningType p = type(k); p != JoiningType::T)
                    {
                        prev = p;
                        break;
                    }
                for (u32 k = i + 1; k < end; k++)
                    if (JoiningType n = type(k); n != JoiningType::T)
                    {
                        next = n;
                        break;
                    }
                bool before = joinsPrev(t) && joinsNext(prev);
                bool after = joinsNext(t) && joinsPrev(next);
                forms[i - start] = before && after ? kMedi : before ? kFina : after ? kInit : kIsol;
            }
        }
    }

    u32 OpenTypeScript(Script script)
    {
        StringView code = ScriptCode(script);
        static const struct
        {
            const char* code;
            u32 tag;
        } kExceptions[] = {{"Zyyy", Tag("DFLT")}, {"Zinh", Tag("DFLT")}, {"Zzzz", Tag("DFLT")}, {"Hira", Tag("kana")},
                           {"Laoo", Tag("lao ")}, {"Yiii", Tag("yi  ")}, {"Nkoo", Tag("nko ")}, {"Vaii", Tag("vai ")}};
        for (const auto& e : kExceptions)
            if (code == StringView(e.code))
                return e.tag;
        if (code.Size() != 4)
            return Tag("DFLT");
        auto lower = [](char c) { return u32(u8(c >= 'A' && c <= 'Z' ? c + 32 : c)); };
        return lower(code[0]) << 24 | lower(code[1]) << 16 | lower(code[2]) << 8 | lower(code[3]);
    }

    void Itemize(Span<const char32_t> text, u32 start, u32 end, const Bidi& bidi, Span<const Face* const> faces, Array<ShapingRun>& out)
    {
        out.Clear();
        if (start >= end)
            return;
        u32 n = end - start;
        Array<Script> scripts;
        Array<u32> faceOf;
        scripts.Resize(n);
        faceOf.Resize(n);
        // Scripts: Common and Inherited take the script before them (the first ones the script after them).
        Script last = kScriptCommon;
        for (u32 i = 0; i < n; i++)
        {
            Script s = PropertiesOf(text[start + i]).script;
            if (s == kScriptCommon || s == kScriptInherited || s == kScriptUnknown)
                s = last;
            scripts[i] = s;
            last = s;
        }
        Script next = kScriptCommon;
        for (u32 i = n; i-- > 0;)
        {
            if (scripts[i] == kScriptCommon)
                scripts[i] = next;
            next = scripts[i];
        }
        // Faces: the first that has the character; marks, joiners and selectors stay with their base; Common characters
        // (spaces, punctuation) stay in the face before them when it has them.
        for (u32 i = 0; i < n; i++)
        {
            char32_t c = text[start + i];
            const CharProperties& p = PropertiesOf(c);
            bool follows = i > 0 && (IsMark(c) || IsVariationSelector(c) || c == 0x200D || c == 0x200C || text[start + i - 1] == 0x200D);
            if (follows || (i > 0 && p.script == kScriptCommon && faces[faceOf[i - 1]]->Has(c)))
            {
                faceOf[i] = faceOf[i - 1];
                continue;
            }
            u32 chosen = 0;
            for (u32 f = 0; f < faces.Size(); f++)
                if (faces[f]->Has(c))
                {
                    chosen = f;
                    break;
                }
            faceOf[i] = chosen;
        }
        Span<const u8> levels = bidi.Levels();
        for (u32 i = 0; i < n; i++)
        {
            u8 level = start + i < levels.Size() ? levels[start + i] : 0;
            if (out.Empty() || out.Back().script != scripts[i] || out.Back().level != level || out.Back().face != faceOf[i])
            {
                ShapingRun r;
                r.start = start + i;
                r.end = start + i + 1;
                r.script = scripts[i];
                r.level = level;
                r.face = faceOf[i];
                out.Push(r);
            }
            else
                out.Back().end = start + i + 1;
        }
    }

    u8 Shaper::ClassOf(u32 glyph, u32 cluster) const
    {
        if (face_->HasGdef())
            if (u16 c = face_->Gdef().GlyphClass(glyph))
                return u8(c);
        return IsMark(text_[cluster]) ? 3 : 1;
    }

    bool Shaper::Skipped(const font::LayoutLookup& lookup, u32 index) const
    {
        const Item& it = items_[index];
        u16 flags = lookup.flags;
        if (it.glyphClass == 1 && (flags & 2))
            return true;
        if (it.glyphClass == 2 && (flags & 4))
            return true;
        if (it.glyphClass == 3)
        {
            if (flags & 8)
                return true;
            if (flags & 0x10)
                return !face_->Gdef().InMarkSet(lookup.markFilteringSet, it.glyph);
            if (u16 type = flags >> 8; type && face_->Gdef().MarkAttachClass(it.glyph) != type)
                return true;
        }
        return false;
    }

    u32 Shaper::Next(const font::LayoutLookup& lookup, u32 index) const
    {
        for (u32 j = index + 1; j < items_.Size(); j++)
            if (!Skipped(lookup, j))
                return j;
        return kNone;
    }

    u32 Shaper::Prev(const font::LayoutLookup& lookup, u32 index) const
    {
        for (u32 j = index; j-- > 0;)
            if (!Skipped(lookup, j))
                return j;
        return kNone;
    }

    void Shaper::Replace(u32 at, Span<const u16> glyphs)
    {
        if (glyphs.Empty())
        {
            items_.RemoveAt(at);
            return;
        }
        Item base = items_[at];
        items_[at].glyph = glyphs[0];
        items_[at].glyphClass = ClassOf(glyphs[0], base.cluster);
        for (usize k = 1; k < glyphs.Size(); k++)
        {
            Item extra = base;
            extra.glyph = glyphs[k];
            extra.glyphClass = ClassOf(glyphs[k], base.cluster);
            items_.Insert(at + u32(k), extra);
        }
    }

    void Shaper::PositionValue(Item& item, u32 value)
    {
        if (value == kNoIndex)
            return;
        const font::ValueRecord& v = face_->Gpos().values[value];
        item.xOffset += v.xPlacement;
        item.yOffset += v.yPlacement;
        item.xAdvance += v.xAdvance;
        item.yAdvance += v.yAdvance;
    }

    bool Shaper::ApplyContext(const font::Layout& layout, const font::LayoutLookup& lookup, const font::LayoutSubtable& st, u32 at, bool gpos)
    {
        u32 first = items_[at].glyph;
        u32 ci = layout.CoverageIndex(st.coverage, first);
        if (ci == kNoIndex)
            return false;
        bool chained = gpos ? st.type == 8 : st.type == 6;
        font::LayoutList rules;
        if (st.format == 1)
        {
            if (ci >= st.data.count)
                return false;
            rules = layout.lists[st.data.begin + ci];
        }
        else if (st.format == 2)
        {
            u16 cls = layout.ClassOf(st.classDef1, first);
            if (cls >= st.data.count)
                return false;
            rules = layout.lists[st.data.begin + cls];
        }
        else
            rules = st.data;
        // How a value of a rule matches a glyph: the glyph itself, its class, or a coverage holding it.
        auto matches = [&](u16 value, u32 glyph, u32 classDef) {
            if (st.format == 1)
                return value == glyph;
            if (st.format == 2)
                return layout.ClassOf(classDef, glyph) == value;
            return layout.CoverageIndex(value, glyph) != kNoIndex;
        };
        Array<u32> positions;
        for (u32 r = 0; r < rules.count; r++)
        {
            const font::SequenceRule& rule = layout.rules[rules.begin + r];
            positions.Clear();
            positions.Push(at);
            bool ok = true;
            // Format 3 lists the first glyph's coverage too.
            u32 inputStart = st.format == 3 ? 1 : 0;
            u32 j = at;
            for (u32 k = inputStart; ok && k < rule.input.count; k++)
            {
                j = Next(lookup, j);
                ok = j != kNone && matches(layout.u16s[rule.input.begin + k], items_[j].glyph, st.classDef1);
                positions.Push(j);
            }
            j = at;
            for (u32 k = 0; ok && k < rule.backtrack.count; k++)
            {
                j = Prev(lookup, j);
                ok = j != kNone && matches(layout.u16s[rule.backtrack.begin + k], items_[j].glyph, chained ? st.classDef2 : st.classDef1);
            }
            j = positions.Back();
            for (u32 k = 0; ok && k < rule.lookahead.count; k++)
            {
                j = Next(lookup, j);
                ok = j != kNone && matches(layout.u16s[rule.lookahead.begin + k], items_[j].glyph, chained ? st.classDef3 : st.classDef1);
            }
            if (!ok)
                continue;
            // The rule's lookups at their positions; positions after a change move with it.
            if (depth_ < kMaxNesting)
            {
                depth_++;
                for (u32 k = 0; k < rule.records.count; k++)
                {
                    const font::LookupRecord& rec = layout.records[rule.records.begin + k];
                    if (rec.sequenceIndex >= positions.Size() || rec.lookupIndex >= layout.lookups.Size())
                        continue;
                    u32 p = positions[rec.sequenceIndex];
                    if (p >= items_.Size() || Skipped(layout.lookups[rec.lookupIndex], p))
                        continue;
                    i64 before = i64(items_.Size());
                    ApplyAt(layout, rec.lookupIndex, p, gpos);
                    i64 delta = i64(items_.Size()) - before;
                    if (delta != 0)
                        for (u32& q : positions)
                            if (q > p)
                                q = u32(i64(q) + delta);
                }
                depth_--;
            }
            next_ = positions.Back() + 1;
            return true;
        }
        return false;
    }

    bool Shaper::ApplySubtable(const font::Layout& layout, const font::LayoutLookup& lookup, const font::LayoutSubtable& st, u32 at, bool gpos)
    {
        Item& it = items_[at];
        next_ = at + 1;
        if (!gpos)
        {
            switch (st.type)
            {
            case 1:
            {
                u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
                if (ci == kNoIndex)
                    return false;
                u32 g;
                if (st.format == 1)
                    g = u32(it.glyph + st.delta) & 0xFFFF;
                else if (ci < st.data.count)
                    g = layout.u16s[st.data.begin + ci];
                else
                    return false;
                it.glyph = g;
                it.glyphClass = ClassOf(g, it.cluster);
                return true;
            }
            case 2:
            case 3:
            {
                u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
                if (ci == kNoIndex || ci >= st.data.count)
                    return false;
                font::LayoutList list = layout.lists[st.data.begin + ci];
                if (st.type == 3)
                {
                    // An alternate: the first one.
                    if (list.count == 0)
                        return false;
                    list.count = 1;
                }
                Replace(at, Span<const u16>(layout.u16s.Data() + list.begin, list.count));
                next_ = at + list.count;
                return true;
            }
            case 4:
            {
                u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
                if (ci == kNoIndex || ci >= st.data.count)
                    return false;
                font::LayoutList set = layout.lists[st.data.begin + ci];
                Array<u32> positions;
                for (u32 k = 0; k < set.count; k++)
                {
                    const font::LigatureEntry& lig = layout.ligatures[set.begin + k];
                    positions.Clear();
                    positions.Push(at);
                    u32 j = at;
                    bool ok = true;
                    for (u32 c = 0; ok && c < lig.components.count; c++)
                    {
                        j = Next(lookup, j);
                        ok = j != kNone && items_[j].glyph == layout.u16s[lig.components.begin + c];
                        positions.Push(j);
                    }
                    if (!ok)
                        continue;
                    u16 id = ++nextLigature_;
                    if (id == 0)
                        id = ++nextLigature_;
                    u32 cluster = items_[at].cluster;
                    // The marks between the components belong to the ligature, to the component before them.
                    for (u32 c = 1; c < positions.Size(); c++)
                        for (u32 m = positions[c - 1] + 1; m < positions[c]; m++)
                        {
                            items_[m].ligature = id;
                            items_[m].component = u16(c - 1);
                            items_[m].cluster = cluster;
                        }
                    items_[at].glyph = lig.glyph;
                    items_[at].glyphClass = face_->HasGdef() && face_->Gdef().GlyphClass(lig.glyph) ? u8(face_->Gdef().GlyphClass(lig.glyph)) : 2;
                    items_[at].ligature = id;
                    items_[at].component = 0;
                    for (u32 c = u32(positions.Size()); c-- > 1;)
                        items_.RemoveAt(positions[c]);
                    // Marks after the ligature hang on its last component.
                    for (u32 m = at + 1; m < items_.Size() && items_[m].glyphClass == 3 && items_[m].ligature == 0; m++)
                    {
                        items_[m].ligature = id;
                        items_[m].component = u16(lig.components.count);
                    }
                    return true;
                }
                return false;
            }
            case 5:
            case 6: return ApplyContext(layout, lookup, st, at, false);
            case 8:
            {
                u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
                if (ci == kNoIndex || ci >= st.data2.count)
                    return false;
                const font::SequenceRule& rule = layout.rules[st.data.begin];
                u32 j = at;
                for (u32 k = 0; k < rule.backtrack.count; k++)
                {
                    j = Prev(lookup, j);
                    if (j == kNone || layout.CoverageIndex(layout.u16s[rule.backtrack.begin + k], items_[j].glyph) == kNoIndex)
                        return false;
                }
                j = at;
                for (u32 k = 0; k < rule.lookahead.count; k++)
                {
                    j = Next(lookup, j);
                    if (j == kNone || layout.CoverageIndex(layout.u16s[rule.lookahead.begin + k], items_[j].glyph) == kNoIndex)
                        return false;
                }
                it.glyph = layout.u16s[st.data2.begin + ci];
                it.glyphClass = ClassOf(it.glyph, it.cluster);
                return true;
            }
            default: return false;
            }
        }
        switch (st.type)
        {
        case 1:
        {
            u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
            if (ci == kNoIndex)
                return false;
            if (st.format == 1)
                PositionValue(it, st.data.count ? st.data.begin : kNoIndex);
            else if (ci < st.data.count)
                PositionValue(it, st.data.begin + ci);
            else
                return false;
            return true;
        }
        case 2:
        {
            u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
            if (ci == kNoIndex)
                return false;
            u32 j = Next(lookup, at);
            if (j == kNone)
                return false;
            u32 v1 = kNoIndex, v2 = kNoIndex;
            if (st.format == 1)
            {
                if (ci >= st.data.count)
                    return false;
                font::LayoutList set = layout.lists[st.data.begin + ci];
                bool found = false;
                for (u32 k = 0; k < set.count && !found; k++)
                    if (const font::PairValue& p = layout.pairs[set.begin + k]; p.second == items_[j].glyph)
                    {
                        v1 = p.value1;
                        v2 = p.value2;
                        found = true;
                    }
                if (!found)
                    return false;
            }
            else
            {
                u32 c1 = layout.ClassOf(st.classDef1, it.glyph), c2 = layout.ClassOf(st.classDef2, items_[j].glyph);
                if (c1 >= st.count1 || c2 >= st.count2)
                    return false;
                u32 index = st.data.begin + 2 * (c1 * st.count2 + c2);
                v1 = index;
                v2 = index + 1;
            }
            PositionValue(items_[at], v1);
            PositionValue(items_[j], v2);
            next_ = st.valueFormat2 ? j + 1 : j;
            return true;
        }
        case 3:
        {
            u32 ci = layout.CoverageIndex(st.coverage, it.glyph);
            if (ci == kNoIndex || 2 * ci + 1 >= st.data.count)
                return false;
            const font::Anchor& exit = layout.anchors[st.data.begin + 2 * ci + 1];
            u32 j = Next(lookup, at);
            if (!exit.present || j == kNone)
                return false;
            u32 cj = layout.CoverageIndex(st.coverage, items_[j].glyph);
            if (cj == kNoIndex || 2 * cj >= st.data.count)
                return false;
            const font::Anchor& entry = layout.anchors[st.data.begin + 2 * cj];
            if (!entry.present)
                return false;
            Item& a = items_[at];
            Item& b = items_[j];
            if (!rightToLeft_)
            {
                a.xAdvance = exit.x + a.xOffset;
                i32 d = entry.x + b.xOffset;
                b.xAdvance -= d;
                b.xOffset -= d;
            }
            else
            {
                i32 d = exit.x + a.xOffset;
                a.xAdvance -= d;
                a.xOffset -= d;
                b.xAdvance = entry.x + b.xOffset;
            }
            // The child hangs vertically on the parent: the later glyph on the earlier one unless the lookup says
            // right to left.
            u32 child = j, parent = at;
            i32 y = exit.y - entry.y;
            if (lookup.flags & 1)
            {
                child = at;
                parent = j;
                y = -y;
            }
            items_[child].yOffset = y;
            items_[child].attach = parent;
            items_[child].attachKind = kAttachCursive;
            next_ = j;
            return true;
        }
        case 4:
        case 5:
        case 6:
        {
            u32 mi = layout.CoverageIndex(st.coverage, it.glyph);
            if (mi == kNoIndex || mi >= st.data.count)
                return false;
            // The glyph it attaches to: the base or ligature before the marks, or the mark right before it.
            u32 j = kNone;
            if (st.type == 6)
            {
                j = Prev(lookup, at);
                if (j == kNone || items_[j].glyphClass != 3)
                    return false;
            }
            else
                for (u32 k = at; k-- > 0;)
                    if (items_[k].glyphClass != 3)
                    {
                        j = k;
                        break;
                    }
            if (j == kNone)
                return false;
            u32 bi = layout.CoverageIndex(st.coverage2, items_[j].glyph);
            if (bi == kNoIndex)
                return false;
            const font::MarkRecord& mark = layout.marks[st.data.begin + mi];
            if (mark.markClass >= st.count1 || mark.anchor == kNoIndex)
                return false;
            u32 anchor;
            if (st.type == 5)
            {
                if (bi >= st.data2.count)
                    return false;
                font::LayoutList list = layout.lists[st.data2.begin + bi];
                u32 components = st.count1 ? list.count / st.count1 : 0;
                if (components == 0)
                    return false;
                u32 c = it.ligature && it.ligature == items_[j].ligature ? it.component : components - 1;
                c = c < components ? c : components - 1;
                anchor = list.begin + c * st.count1 + mark.markClass;
            }
            else
            {
                if ((bi + 1) * st.count1 > st.data2.count)
                    return false;
                anchor = st.data2.begin + bi * st.count1 + mark.markClass;
            }
            const font::Anchor& base = layout.anchors[anchor];
            const font::Anchor& own = layout.anchors[mark.anchor];
            if (!base.present || !own.present)
                return false;
            it.xOffset = base.x - own.x;
            it.yOffset = base.y - own.y;
            it.attach = j;
            it.attachKind = kAttachMark;
            return true;
        }
        case 7:
        case 8: return ApplyContext(layout, lookup, st, at, true);
        default: return false;
        }
    }

    bool Shaper::ApplyAt(const font::Layout& layout, u32 lookup, u32 at, bool gpos)
    {
        const font::LayoutLookup& lk = layout.lookups[lookup];
        for (u32 s = 0; s < lk.subtables.count; s++)
            if (ApplySubtable(layout, lk, layout.subtables[lk.subtables.begin + s], at, gpos))
                return true;
        return false;
    }

    void Shaper::ApplyLookup(u32 lookup, bool gpos, u32 mask)
    {
        const font::Layout& layout = gpos ? face_->Gpos() : face_->Gsub();
        const font::LayoutLookup& lk = layout.lookups[lookup];
        if (!gpos && lk.type == 8)
        {
            for (u32 i = u32(items_.Size()); i-- > 0;)
                if ((items_[i].mask & mask) && face_->Digest(false, lookup).May(items_[i].glyph) && !Skipped(lk, i))
                    ApplyAt(layout, lookup, i, false);
            return;
        }
        const LookupDigest& digest = face_->Digest(gpos, lookup);
        for (u32 i = 0; i < items_.Size();)
        {
            if ((items_[i].mask & mask) && digest.May(items_[i].glyph) && !Skipped(lk, i) && ApplyAt(layout, lookup, i, gpos))
                i = next_ > i ? next_ : i + 1;
            else
                i++;
        }
    }

    void Shaper::ResolveAttachments(bool rightToLeft)
    {
        Array<u8> done;
        done.Resize(items_.Size());
        auto resolve = [&](auto& self, u32 i) -> void {
            if (done[i])
                return;
            done[i] = 1;
            Item& it = items_[i];
            if (it.attach == kNone || it.attach >= items_.Size())
                return;
            u32 p = it.attach;
            self(self, p);
            const Item& parent = items_[p];
            it.yOffset += parent.yOffset;
            if (it.attachKind != kAttachMark)
                return;
            it.xOffset += parent.xOffset;
            if (!rightToLeft)
            {
                for (u32 k = p; k < i; k++)
                    it.xOffset -= items_[k].xAdvance;
            }
            else
                for (u32 k = p + 1; k <= i; k++)
                    it.xOffset += items_[k].xAdvance;
        };
        for (u32 i = 0; i < items_.Size(); i++)
            resolve(resolve, i);
    }

    void Shaper::Shape(const Face& face, Span<const char32_t> text, const ShapingRun& run, Array<ShapedGlyph>& out, u32 language,
                       Span<const FontFeature> features)
    {
        out.Clear();
        face_ = &face;
        text_ = text.Data();
        rightToLeft_ = run.RightToLeft();
        nextLigature_ = 0;
        depth_ = 0;
        items_.Clear();
        Array<u32> forms;
        JoiningForms(text, run.start, run.end, forms);
        for (u32 i = run.start; i < run.end; i++)
        {
            char32_t c = text[i];
            if (IsVariationSelector(c) && i > run.start)
                continue;
            char32_t selector = i + 1 < run.end && IsVariationSelector(text[i + 1]) ? text[i + 1] : 0;
            Item it{};
            it.glyph = face.Glyph(c, selector);
            it.cluster = i;
            it.mask = kGlobal | forms[i - run.start];
            it.glyphClass = ClassOf(it.glyph, i);
            it.attach = kNone;
            items_.Push(it);
        }

        // The features that apply: the defaults, those switched on, minus those switched off.
        auto enabled = [&](u32 tag, bool byDefault) {
            bool on = byDefault;
            for (const FontFeature& f : features)
                if (f.tag == tag)
                    on = f.enabled;
            return on;
        };
        auto bitOf = [&](u32 tag) -> u32 {
            for (const FeatureBit& d : kDefaults)
                if (d.tag == tag)
                    return enabled(tag, true) ? d.bit : 0;
            return enabled(tag, false) ? kGlobal : 0;
        };
        u32 scriptTag = OpenTypeScript(run.script);
        bool gposKern = false;
        for (int pass = 0; pass < 2; pass++)
        {
            bool gpos = pass == 1;
            const font::Layout& layout = gpos ? face.Gpos() : face.Gsub();
            if (gpos)
                for (Item& it : items_)
                {
                    it.xAdvance = face.Advance(it.glyph);
                    it.yAdvance = 0;
                }
            if (layout.Empty())
                continue;
            const font::LayoutScript* script = layout.FindScript(scriptTag);
            if (!script)
                script = layout.FindScript(Tag("DFLT"));
            if (!script)
                script = layout.FindScript(Tag("dflt"));
            if (!script)
                script = layout.FindScript(Tag("latn"));
            if (!script)
                continue;
            const font::LayoutLanguage* lang = nullptr;
            for (u32 k = 0; language && k < script->languages.count; k++)
                if (layout.languages[script->languages.begin + k].tag == language)
                    lang = &layout.languages[script->languages.begin + k];
            if (!lang && script->defaultLanguage != kNoIndex)
                lang = &layout.languages[script->defaultLanguage];
            if (!lang)
                continue;
            // The mask of each lookup: the bits of the features that use it.
            Array<u32> masks;
            masks.Resize(layout.lookups.Size());
            auto addFeature = [&](u32 index, bool required) {
                if (index >= layout.features.Size())
                    return;
                const font::LayoutFeature& f = layout.features[index];
                u32 bit = required ? kGlobal : bitOf(f.tag);
                if (!bit)
                    return;
                if (gpos && f.tag == Tag("kern") && f.lookups.count)
                    gposKern = true;
                for (u32 k = 0; k < f.lookups.count; k++)
                    if (u16 l = layout.indices[f.lookups.begin + k]; l < masks.Size())
                        masks[l] |= bit;
            };
            if (lang->requiredFeature != 0xFFFF)
                addFeature(lang->requiredFeature, true);
            for (u32 k = 0; k < lang->features.count; k++)
                addFeature(layout.indices[lang->features.begin + k], false);
            for (u32 l = 0; l < masks.Size(); l++)
                if (masks[l])
                    ApplyLookup(l, gpos, masks[l]);
        }
        // The kern table when GPOS does not kern.
        if (!gposKern && enabled(Tag("kern"), true) && !face.Kern().Pairs().Empty())
            for (u32 i = 0; i < items_.Size(); i++)
            {
                if (items_[i].glyphClass == 3)
                    continue;
                u32 j = i + 1;
                while (j < items_.Size() && items_[j].glyphClass == 3)
                    j++;
                if (j < items_.Size())
                    items_[i].xAdvance += face.Kern().Get(items_[i].glyph, items_[j].glyph);
            }
        // Marks take no room; attachments become offsets from the pen.
        for (Item& it : items_)
            if (it.glyphClass == 3)
                it.xAdvance = 0;
        ResolveAttachments(rightToLeft_);
        for (const Item& it : items_)
        {
            // Format characters (joiners, controls, selectors) the face has no glyph for are left out.
            if (it.glyph == 0 && PropertiesOf(text[it.cluster]).category == GeneralCategory::Cf)
                continue;
            ShapedGlyph g;
            g.glyph = it.glyph;
            g.cluster = it.cluster;
            g.xAdvance = PropertiesOf(text[it.cluster]).category == GeneralCategory::Cf ? 0 : it.xAdvance;
            g.yAdvance = it.yAdvance;
            g.xOffset = it.xOffset;
            g.yOffset = it.yOffset;
            g.glyphClass = it.glyphClass;
            out.Push(g);
        }
    }
}
