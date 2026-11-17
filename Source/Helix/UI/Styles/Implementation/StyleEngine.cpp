#include <Helix/UI/Styles/StyleEngine.h>

#include <Helix/Core/Hashing/Hash.h>

#include <algorithm>

namespace helix::ui
{
    namespace
    {
        bool SameLength(const Length& a, const Length& b) { return a.unit == b.unit && a.value == b.value; }

        bool SameLayout(const LayoutStyle& a, const LayoutStyle& b)
        {
            if (a.direction != b.direction || a.wrap != b.wrap || a.justify != b.justify || a.alignItems != b.alignItems || a.alignSelf != b.alignSelf ||
                a.alignContent != b.alignContent || a.position != b.position || a.display != b.display || a.grow != b.grow || a.shrink != b.shrink ||
                a.aspectRatio != b.aspectRatio || !SameLength(a.basis, b.basis) || !SameLength(a.width, b.width) || !SameLength(a.height, b.height) ||
                !SameLength(a.minWidth, b.minWidth) || !SameLength(a.minHeight, b.minHeight) || !SameLength(a.maxWidth, b.maxWidth) ||
                !SameLength(a.maxHeight, b.maxHeight))
                return false;
            for (u32 k = 0; k < 4; k++)
                if (!SameLength(a.inset[k], b.inset[k]) || !SameLength(a.margin[k], b.margin[k]) || !SameLength(a.padding[k], b.padding[k]) ||
                    a.border[k] != b.border[k])
                    return false;
            return true;
        }

        struct Values
        {
            StyleValue v[kMaxValues];
            u32 count = 0;

            bool operator==(const Values& o) const
            {
                if (count != o.count)
                    return false;
                for (u32 i = 0; i < count; i++)
                    if (!(v[i] == o.v[i]))
                        return false;
                return true;
            }
        };

        Values Get(const PropertyInfo& p, const ComputedStyle& style)
        {
            Values out;
            out.count = GetValues(p, style, out.v);
            return out;
        }

        void Set(const PropertyInfo& p, ComputedStyle& style, const StyleValue* values, u32 count) { p.set(style, Span<const StyleValue>(values, count)); }

        // Each value between the two (the same count; else the end's at the end).
        void Between(const PropertyInfo& p, ComputedStyle& style, const StyleValue* from, const StyleValue* to, u32 count, f32 t)
        {
            StyleValue v[kMaxValues];
            for (u32 i = 0; i < count; i++)
                v[i] = Interpolate(from[i], to[i], t);
            Set(p, style, v, count);
        }

        Atom PropertyAtom(PropertyId id)
        {
            static Array<Atom>* atoms = [] {
                auto* a = new Array<Atom>();
                for (const PropertyInfo& p : Properties())
                    a->Push(Intern(p.name));
                return a;
            }();
            return (*atoms)[id];
        }

        void Bloom(u16* counters, Atom atom, i32 delta)
        {
            u64 h = MixHash(u64(atom) + 1);
            counters[h & 4095] = u16(i32(counters[h & 4095]) + delta);
            counters[(h >> 12) & 4095] = u16(i32(counters[(h >> 12) & 4095]) + delta);
        }

        bool InBloom(const u16* counters, Atom atom)
        {
            u64 h = MixHash(u64(atom) + 1);
            return counters[h & 4095] && counters[(h >> 12) & 4095];
        }

        struct Match
        {
            u32 specificity;
            u32 sheet;
            u32 key;
            const StyleRule* rule;
        };
    }

    StyleEngine::Slot& StyleEngine::At(ElementHandle e)
    {
        if (slots_.Size() < tree_.Capacity())
            slots_.Resize(tree_.Capacity());
        Slot& s = slots_[e.index];
        if (s.generation != e.generation)
        {
            s = Slot();
            s.generation = e.generation;
        }
        return s;
    }

    const ComputedStyle& StyleEngine::Computed(ElementHandle e) const
    {
        static const ComputedStyle initial;
        if (!tree_.IsAlive(e) || e.index >= slots_.Size() || slots_[e.index].generation != e.generation)
            return initial;
        return slots_[e.index].shown;
    }

    void StyleEngine::AddStyleSheet(ElementHandle owner, std::shared_ptr<const StyleSheetData> sheet)
    {
        if (!tree_.IsAlive(owner) || !sheet)
            return;
        At(owner).sheets.Push(Move(sheet));
        tree_.MarkStyleDirty(owner);
    }

    void StyleEngine::RemoveStyleSheet(ElementHandle owner, const StyleSheetData* sheet)
    {
        if (!tree_.IsAlive(owner))
            return;
        auto& sheets = At(owner).sheets;
        for (usize i = 0; i < sheets.Size(); i++)
            if (sheets[i].get() == sheet)
            {
                sheets.RemoveAt(i);
                tree_.MarkStyleDirty(owner);
                return;
            }
    }

    void StyleEngine::InsertStyleSheet(ElementHandle owner, u32 index, std::shared_ptr<const StyleSheetData> sheet)
    {
        if (!tree_.IsAlive(owner) || !sheet)
            return;
        auto& sheets = At(owner).sheets;
        sheets.Insert(index < sheets.Size() ? index : sheets.Size(), Move(sheet));
        tree_.MarkStyleDirty(owner);
    }

    Span<const std::shared_ptr<const StyleSheetData>> StyleEngine::StyleSheets(ElementHandle owner) const
    {
        if (!tree_.IsAlive(owner) || owner.index >= slots_.Size() || slots_[owner.index].generation != owner.generation)
            return {};
        const auto& sheets = slots_[owner.index].sheets;
        return Span<const std::shared_ptr<const StyleSheetData>>(sheets.Data(), sheets.Size());
    }

    void StyleEngine::ClearStyleSheets(ElementHandle owner)
    {
        if (!tree_.IsAlive(owner))
            return;
        At(owner).sheets.Clear();
        tree_.MarkStyleDirty(owner);
    }

    void StyleEngine::SetInlineStyle(ElementHandle e, StringView declarations, Array<StyleError>* errors)
    {
        if (!tree_.IsAlive(e))
            return;
        Slot& s = At(e);
        s.inlineStyle.Clear();
        Array<StyleError> found;
        ParseDeclarations(declarations, s.inlineStyle, found);
        if (errors)
            *errors = Move(found);
        tree_.MarkStyleDirty(e);
    }

    bool StyleEngine::SetInlineProperty(ElementHandle e, PropertyId property, StringView value, String* error)
    {
        if (!tree_.IsAlive(e) || property == kNoProperty)
            return false;
        Declaration d;
        d.property = property;
        String reason;
        if (!ParseValue(property, value, d.values, reason))
        {
            if (error)
                *error = Move(reason);
            return false;
        }
        ClearInlineProperty(e, property);
        At(e).inlineStyle.Push(Move(d));
        tree_.MarkStyleDirty(e);
        return true;
    }

    void StyleEngine::SetInlineValues(ElementHandle e, PropertyId property, Span<const StyleValue> values)
    {
        if (!tree_.IsAlive(e) || property == kNoProperty)
            return;
        Declaration d;
        d.property = property;
        for (const StyleValue& v : values)
            d.values.Push(v);
        ClearInlineProperty(e, property);
        At(e).inlineStyle.Push(Move(d));
        tree_.MarkStyleDirty(e);
    }

    void StyleEngine::SetInlineInitial(ElementHandle e, PropertyId property)
    {
        if (!tree_.IsAlive(e) || property == kNoProperty)
            return;
        ClearInlineProperty(e, property);
        Declaration d;
        d.property = property;
        d.initial = true;
        At(e).inlineStyle.Push(Move(d));
        tree_.MarkStyleDirty(e);
    }

    void StyleEngine::ClearInlineProperty(ElementHandle e, PropertyId property)
    {
        if (!tree_.IsAlive(e))
            return;
        Array<Declaration>& list = At(e).inlineStyle;
        for (usize i = 0; i < list.Size();)
            if (list[i].property == property)
                list.RemoveAt(i);
            else
                i++;
        tree_.MarkStyleDirty(e);
    }

    bool StyleEngine::InlineProperty(ElementHandle e, PropertyId property, Array<StyleValue>& out, bool* initial) const
    {
        out.Clear();
        if (!tree_.IsAlive(e) || e.index >= slots_.Size() || slots_[e.index].generation != e.generation)
            return false;
        const Array<Declaration>& list = slots_[e.index].inlineStyle;
        for (usize i = list.Size(); i-- > 0;)
            if (list[i].property == property)
            {
                for (const StyleValue& v : list[i].values)
                    out.Push(v);
                if (initial)
                    *initial = list[i].initial;
                return true;
            }
        return false;
    }

    void StyleEngine::Update(ElementHandle root, f32 dt)
    {
        if (!tree_.IsAlive(root))
            return;
        computed_ = 0;
        customResolved_.Clear();
        active_.Clear();
        activeOwners_.Clear();
        std::fill(bloom_, bloom_ + 4096, u16(0));
        // Tree order: the sheets and ancestor atoms in effect are kept on the way down.
        struct Walker
        {
            StyleEngine& engine;
            // A sheet in effect, the sheets it imports first (their rules come before its own).
            void Activate(const StyleSheetData* sheet, ElementHandle owner, u32 depth)
            {
                if (!sheet || depth > 16)
                    return;
                for (const auto& imported : sheet->Imported())
                    Activate(imported.get(), owner, depth + 1);
                engine.active_.Push(sheet);
                engine.activeOwners_.Push(owner);
            }
            void Walk(ElementHandle e, const ComputedStyle* parent, bool force)
            {
                ElementTree& tree = engine.tree_;
                Slot& s = engine.At(e);
                usize sheetsBefore = engine.active_.Size();
                for (const auto& sheet : s.sheets)
                    Activate(sheet.get(), e, 0);
                Dirty flags = tree.DirtyFlags(e);
                bool recompute = force || Any(flags & Dirty::Style) || !s.computedOnce;
                if (recompute)
                    engine.Compute(e, parent);
                if (recompute || Any(flags & Dirty::DescendantStyle))
                {
                    Atom type = tree.Type(e), name = tree.Name(e);
                    Span<const Atom> classes = tree.Classes(e);
                    Bloom(engine.bloom_, type, 1);
                    if (name != kNoAtom)
                        Bloom(engine.bloom_, name, 1);
                    for (Atom c : classes)
                        Bloom(engine.bloom_, c, 1);
                    const ComputedStyle* mine = &engine.At(e).shown;
                    for (ElementHandle c = tree.FirstChild(e); c.IsValid(); c = tree.NextSibling(c))
                        Walk(c, mine, recompute);
                    Bloom(engine.bloom_, type, -1);
                    if (name != kNoAtom)
                        Bloom(engine.bloom_, name, -1);
                    for (Atom c : classes)
                        Bloom(engine.bloom_, c, -1);
                }
                tree.ClearDirty(e, Dirty::Style | Dirty::DescendantStyle);
                engine.active_.Resize(sheetsBefore);
                engine.activeOwners_.Resize(sheetsBefore);
            }
        };
        Walker{*this}.Walk(root, tree_.Parent(root).IsValid() ? &Computed(tree_.Parent(root)) : nullptr, false);

        // Transitions.
        animating_ = 0;
        for (u32 i = 0; i < slots_.Size(); i++)
        {
            Slot& s = slots_[i];
            if (s.running.Empty())
                continue;
            ElementHandle e = tree_.HandleAt(i);
            if (!e.IsValid() || e.generation != s.generation)
            {
                s.running.Clear();
                continue;
            }
            Animate(e, s, dt);
            animating_ += s.running.Empty() ? 0 : 1;
        }
    }

    bool StyleEngine::MatchPart(const SelectorPart& p, ElementHandle e) const
    {
        if (p.type != kNoAtom && tree_.Type(e) != p.type)
            return false;
        if (p.name != kNoAtom && tree_.Name(e) != p.name)
            return false;
        for (Atom c : p.classes)
            if (!tree_.HasClass(e, c))
                return false;
        PseudoStates states = tree_.States(e);
        // :root is the top of the sheet's reach: the element it is added to (or an element of no parent).
        if (e == matchRoot_ || !tree_.Parent(e).IsValid())
            states = states | PseudoStates::Root;
        return (states & p.required) == p.required && !Any(states & p.forbidden);
    }

    bool StyleEngine::MatchFrom(const Selector& s, u32 part, ElementHandle e) const
    {
        if (!MatchPart(s.parts[part], e))
            return false;
        if (part + 1 == s.parts.Size())
            return true;
        Combinator relation = s.parts[part].left;
        ElementHandle parent = tree_.Parent(e);
        if (relation == Combinator::Child)
            return parent.IsValid() && MatchFrom(s, part + 1, parent);
        for (ElementHandle a = parent; a.IsValid(); a = tree_.Parent(a))
            if (MatchFrom(s, part + 1, a))
                return true;
        return false;
    }

    bool StyleEngine::Matches(const Selector& s, ElementHandle e) const
    {
        for (Atom a : s.ancestorAtoms)
            if (!InBloom(bloom_, a))
                return false;
        return MatchFrom(s, 0, e);
    }

    StringView StyleEngine::Variable(ElementHandle e, Atom name) const
    {
        for (ElementHandle a = e; a.IsValid(); a = tree_.Parent(a))
        {
            if (a.index >= slots_.Size())
                continue;
            const Slot& s = slots_[a.index];
            for (usize i = s.varNames.Size(); i-- > 0;)
                if (s.varNames[i] == name)
                    return s.varValues[i].View();
        }
        return {};
    }

    bool StyleEngine::Substitute(ElementHandle e, StringView raw, String& text) const
    {
        // var(--name[, fallback]) replaced by the variable's text (or the fallback).
        usize at = 0;
        for (;;)
        {
            usize v = raw.Find("var(", at);
            if (v == StringView::npos)
            {
                text.Append(raw.Substr(at));
                return true;
            }
            text.Append(raw.Substr(at, v - at));
            usize close = raw.Find(')', v);
            if (close == StringView::npos)
                return false;
            StringView inside = raw.Substr(v + 4, close - v - 4);
            usize comma = inside.Find(',');
            StringView name = (comma == StringView::npos ? inside : inside.Substr(0, comma)).Trimmed();
            StringView fallback = comma == StringView::npos ? StringView() : inside.Substr(comma + 1).Trimmed();
            StringView value = Variable(e, FindAtom(name));
            text.Append(value.Empty() ? fallback : value);
            at = close + 1;
        }
    }

    bool StyleEngine::CustomProperty(ElementHandle e, StringView name, String& out) const
    {
        out.Clear();
        if (!tree_.IsAlive(e) || e.index >= slots_.Size() || slots_[e.index].generation != e.generation)
            return false;
        Atom atom = FindAtom(name);
        if (atom == kNoAtom)
            return false;
        const Slot& s = slots_[e.index];
        for (usize i = s.varNames.Size(); i-- > 0;)
            if (s.varNames[i] == atom)
            {
                String text;
                if (!Substitute(e, s.varValues[i].View(), text))
                    return false;
                out = String(text.View().Trimmed());
                return true;
            }
        return false;
    }

    void StyleEngine::CustomPropertyNames(ElementHandle e, Array<Atom>& out) const
    {
        out.Clear();
        if (!tree_.IsAlive(e) || e.index >= slots_.Size() || slots_[e.index].generation != e.generation)
            return;
        for (Atom a : slots_[e.index].varNames)
        {
            bool seen = false;
            for (Atom b : out)
                seen = seen || a == b;
            if (!seen)
                out.Push(a);
        }
    }

    void StyleEngine::Apply(ComputedStyle& style, const Declaration& d, ElementHandle e)
    {
        if (d.shorthand != kNoAtom)
        {
            // A shorthand holding var(): its longhands from the replaced text.
            String text;
            Array<Declaration> longhands;
            String error;
            if (Substitute(e, d.raw.View(), text) && ExpandShorthand(d.shorthand, text.View(), longhands, error))
                for (const Declaration& l : longhands)
                    Apply(style, l, e);
            return;
        }
        if (d.custom != kNoAtom || d.property == kNoProperty)
            return;
        const PropertyInfo& p = Property(d.property);
        if (d.initial)
        {
            static const ComputedStyle initial;
            if (!p.list)
            {
                Values v = Get(p, initial);
                Set(p, style, v.v, v.count);
            }
            return;
        }
        if (!d.hasVar)
        {
            p.set(style, Span<const StyleValue>(d.values.Data(), d.values.Size()));
            return;
        }
        String text;
        if (!Substitute(e, d.raw.View(), text))
            return;
        Array<StyleValue> values;
        String error;
        if (ParseValue(d.property, text.View(), values, error))
            p.set(style, Span<const StyleValue>(values.Data(), values.Size()));
    }

    void StyleEngine::Compute(ElementHandle e, const ComputedStyle* parent)
    {
        computed_++;
        Slot& s = At(e);
        ComputedStyle before = s.target;
        ComputedStyle style;
        if (parent)
            for (PropertyId id = 0; id < Properties().Size(); id++)
            {
                const PropertyInfo& p = Property(id);
                if (p.inherited && p.copy)
                    p.copy(style, *parent);
                else if (p.inherited)
                {
                    Values v = Get(p, *parent);
                    Set(p, style, v.v, v.count);
                }
            }

        // The matching rules of the sheets in effect, by specificity, then the sheets' order (outer first), then the
        // rules' order.
        static thread_local Array<Match> matches;
        matches.Clear();
        Atom type = tree_.Type(e), name = tree_.Name(e);
        Span<const Atom> classes = tree_.Classes(e);
        for (u32 sheet = 0; sheet < active_.Size(); sheet++)
        {
            const StyleSheetData& ss = *active_[sheet];
            matchRoot_ = activeOwners_[sheet];
            ss.ForEachCandidate(type, name, classes, [&](u32 key) {
                const Selector& sel = ss.SelectorAt(key);
                if (Matches(sel, e))
                    matches.Push({sel.specificity, sheet, key, &ss.Rules()[key >> 16]});
            });
        }
        std::sort(matches.Data(), matches.Data() + matches.Size(), [](const Match& a, const Match& b) {
            if (a.specificity != b.specificity)
                return a.specificity < b.specificity;
            if (a.sheet != b.sheet)
                return a.sheet < b.sheet;
            return a.key < b.key;
        });

        // Custom properties first (later ones win), then the declarations in cascade order, then the inline style.
        s.varNames.Clear();
        s.varValues.Clear();
        auto declareVars = [&](const Array<Declaration>& list) {
            for (const Declaration& d : list)
                if (d.custom != kNoAtom)
                {
                    s.varNames.Push(d.custom);
                    s.varValues.Push(d.raw);
                }
        };
        for (const Match& m : matches)
            declareVars(m.rule->declarations);
        declareVars(s.inlineStyle);
        // Its custom properties were resolved (it has some, or had some before): scripts hear of it
        // (CustomStyleResolvedEvent).
        if (!s.varNames.Empty() || s.hadCustom)
            customResolved_.Push(e);
        s.hadCustom = !s.varNames.Empty();
        for (const Match& m : matches)
            for (const Declaration& d : m.rule->declarations)
                Apply(style, d, e);
        for (const Declaration& d : s.inlineStyle)
            Apply(style, d, e);

        s.target = style;
        if (s.computedOnce)
            StartTransitions(s, before);
        // What is shown: the target, with the running transitions' values on top (Animate moves them).
        LayoutStyle shownLayout = s.shown.layout;
        s.shown = s.target;
        for (const Running& r : s.running)
            Between(Property(r.property), s.shown, r.from, r.to, r.count, Ease(r.easing, r.duration > 0 ? (r.elapsed - r.delay) / r.duration : 1.0f));
        if (!s.computedOnce || !SameLayout(shownLayout, s.shown.layout))
            layout_.SetStyle(e, s.shown.layout);
        s.computedOnce = true;
    }

    void StyleEngine::StartTransitions(Slot& s, const ComputedStyle& before)
    {
        const ComputedStyle& after = s.target;
        if (after.transitionCount == 0)
            return;
        for (PropertyId id = 0; id < Properties().Size(); id++)
        {
            const PropertyInfo& p = Property(id);
            if (!Animatable(p))
                continue;
            Values from = Get(p, before), to = Get(p, after);
            if (from == to)
                continue;
            const TransitionSpec* spec = nullptr;
            for (u32 i = 0; i < after.transitionCount; i++)
                if (after.transitions[i].property == kNoAtom || after.transitions[i].property == PropertyAtom(id))
                    spec = &after.transitions[i];
            if (!spec || spec->duration <= 0)
                continue;
            // From what is shown now (a running transition's current value).
            Values current = Get(p, s.shown);
            Running next;
            next.property = id;
            next.count = to.count < current.count ? to.count : current.count;
            for (u32 i = 0; i < next.count; i++)
                next.from[i] = current.v[i], next.to[i] = to.v[i];
            next.elapsed = 0, next.duration = spec->duration, next.delay = spec->delay, next.easing = spec->easing;
            bool replaced = false;
            for (Running& r : s.running)
                if (r.property == id)
                {
                    r = next;
                    replaced = true;
                }
            if (!replaced)
                s.running.Push(next);
        }
    }

    void StyleEngine::Animate(ElementHandle e, Slot& s, f32 dt)
    {
        bool layoutChanged = false, inheritedChanged = false;
        for (usize i = 0; i < s.running.Size();)
        {
            Running& r = s.running[i];
            r.elapsed += dt;
            f32 t = r.duration > 0 ? (r.elapsed - r.delay) / r.duration : 1.0f;
            const PropertyInfo& p = Property(r.property);
            if (t >= 1)
                Set(p, s.shown, r.to, r.count);
            else
                Between(p, s.shown, r.from, r.to, r.count, Ease(r.easing, t));
            layoutChanged |= p.layout;
            inheritedChanged |= p.inherited;
            if (t >= 1)
                s.running.RemoveAt(i);
            else
                i++;
        }
        if (layoutChanged)
            layout_.SetStyle(e, s.shown.layout);
        if (inheritedChanged)
            for (ElementHandle c = tree_.FirstChild(e); c.IsValid(); c = tree_.NextSibling(c))
                tree_.MarkStyleDirty(c);
    }
}
