#include <Helix/UI/Layout/Layout.h>

#include <Helix/Jobs/Jobs.h>

#include <cmath>
#include <limits>
#include <memory>

namespace helix::ui
{
    namespace
    {
        constexpr f32 kUndefined = std::numeric_limits<f32>::quiet_NaN();
        constexpr f32 kInfinity = std::numeric_limits<f32>::infinity();

        // NaN is the only value unequal to itself (std::isnan is a call on some compilers, and this runs per element).
        bool Defined(f32 v) { return v == v; }
        bool Same(f32 a, f32 b) { return a == b || (a != a && b != b); }
        f32 Max0(f32 v) { return v > 0 ? v : 0; }

        // A length against a base (undefined when auto, or a percentage of an undefined base).
        f32 Resolve(const Length& l, f32 base)
        {
            switch (l.unit)
            {
            case Length::Unit::Pixel: return l.value;
            case Length::Unit::Percent: return Defined(base) ? l.value * base * 0.01f : kUndefined;
            default: return kUndefined;
            }
        }

        f32 ResolveOr(const Length& l, f32 base, f32 fallback)
        {
            f32 v = Resolve(l, base);
            return Defined(v) ? v : fallback;
        }

        f32 Clamp(f32 v, f32 lo, f32 hi)
        {
            if (v > hi)
                v = hi;
            if (v < lo)
                v = lo;
            return v;
        }

        bool IsRow(FlexDirection d) { return d == FlexDirection::Row || d == FlexDirection::RowReverse; }
        bool IsReverse(FlexDirection d) { return d == FlexDirection::RowReverse || d == FlexDirection::ColumnReverse; }

        // The start and end edges of the main or cross axis.
        Edge StartEdge(bool horizontal) { return horizontal ? kLeft : kTop; }
        Edge EndEdge(bool horizontal) { return horizontal ? kRight : kBottom; }

        // Minimum and maximum sizes along an axis (auto: 0 and none).
        void MinMax(const LayoutStyle& s, bool horizontal, f32 base, f32& lo, f32& hi)
        {
            lo = ResolveOr(horizontal ? s.minWidth : s.minHeight, base, 0.0f);
            hi = ResolveOr(horizontal ? s.maxWidth : s.maxHeight, base, kInfinity);
            if (hi < lo)
                hi = lo;
        }

        struct Item
        {
            ElementHandle element;
            const LayoutStyle* style = nullptr;
            f32 basis = 0, hypothetical = 0, target = 0, cross = 0;
            f32 minMain = 0, maxMain = kInfinity, minCross = 0, maxCross = kInfinity;
            // Resolved margins of the main and cross axes (start, end); auto ones are 0 until they take free space.
            f32 marginMain[2] = {0, 0}, marginCross[2] = {0, 0};
            bool autoMain[2] = {false, false}, autoCross[2] = {false, false};
            f32 edgeMain = 0;
            f32 crossSize = kUndefined;
            Align align = Align::Stretch;
            bool frozen = false;
            // A content size still to measure for the basis, with the cross constraint it gets.
            bool measure = false;
            f32 measureCross = 0;
            MeasureMode measureMode = MeasureMode::Undefined;
        };

        struct Line
        {
            u32 first = 0, count = 0;
            f32 cross = 0, offset = 0;
        };

        // A child's place and size, laid out after its siblings' places are known.
        struct Placement
        {
            ElementHandle element;
            f32 ownerWidth, ownerHeight, width, height, x, y;
        };

        // Subtrees go to the jobs when this much work waits (an element and its children count).
        constexpr u32 kParallelWork = 128;
    }

    LayoutEngine::Slot& LayoutEngine::At(ElementHandle e)
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

    const LayoutEngine::Slot* LayoutEngine::Find(ElementHandle e) const
    {
        if (!tree_.IsAlive(e) || e.index >= slots_.Size() || slots_[e.index].generation != e.generation)
            return nullptr;
        return &slots_[e.index];
    }

    const LayoutStyle& LayoutEngine::Style(ElementHandle e)
    {
        static const LayoutStyle defaults;
        return tree_.IsAlive(e) ? At(e).style : defaults;
    }

    void LayoutEngine::SetStyle(ElementHandle e, const LayoutStyle& style)
    {
        if (!tree_.IsAlive(e))
            return;
        At(e).style = style;
        tree_.MarkLayoutDirty(e);
    }

    void LayoutEngine::SetMeasure(ElementHandle e, MeasureFunction function, void* user)
    {
        if (!tree_.IsAlive(e))
            return;
        Slot& s = At(e);
        s.measure = function;
        s.user = user;
        tree_.MarkLayoutDirty(e);
    }

    const LayoutBox& LayoutEngine::Box(ElementHandle e) const
    {
        static const LayoutBox none;
        const Slot* s = Find(e);
        return s ? s->box : none;
    }

    Rect LayoutEngine::WorldRect(ElementHandle e) const
    {
        const Slot* s = Find(e);
        return s ? s->pixel : Rect();
    }

    void LayoutEngine::Update(ElementHandle root, f32 width, f32 height) { Update(root, width, MeasureMode::Exactly, height, MeasureMode::Exactly); }

    void LayoutEngine::Update(ElementHandle root, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode)
    {
        if (!tree_.IsAlive(root))
            return;
        pass_++;
        computed_.store(0, std::memory_order_relaxed);
        Slot& s = At(root);
        s.box.x = s.box.y = 0;
        Compute(root, width, height, width, widthMode, height, heightMode, true);
        ClearLayoutDirty(root);
        UpdateWorld(root, Vector2(0, 0), true);
    }

    void LayoutEngine::UpdateWorld(ElementHandle e, Vector2 origin, bool force)
    {
        // A subtree served from the cache whose origin did not move keeps its world positions.
        Slot& s = At(e);
        Vector2 world(origin.x + s.box.x, origin.y + s.box.y);
        bool moved = world.x != s.world.x || world.y != s.world.y;
        if (!force && !moved && s.laidPass != pass_)
            return;
        s.world = world;
        // Rounding the edges (not the size) keeps neighbours touching; text rounds up so it never wraps anew.
        f32 x0 = std::round(world.x), y0 = std::round(world.y);
        f32 right = world.x + s.box.width, bottom = world.y + s.box.height;
        f32 x1 = s.measure ? std::ceil(right - 0.001f) : std::round(right);
        f32 y1 = s.measure ? std::ceil(bottom - 0.001f) : std::round(bottom);
        s.pixel = Rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
        for (ElementHandle c = tree_.FirstChild(e); c.IsValid(); c = tree_.NextSibling(c))
            UpdateWorld(c, world, moved);
    }

    void LayoutEngine::ClearLayoutDirty(ElementHandle root)
    {
        if (!Any(tree_.DirtyFlags(root) & Dirty::Layout))
            return;
        tree_.ClearDirty(root, Dirty::Layout);
        for (ElementHandle c = tree_.FirstChild(root); c.IsValid(); c = tree_.NextSibling(c))
            ClearLayoutDirty(c);
    }

    const LayoutEngine::Cache* LayoutEngine::Cached(const Slot& s, ElementHandle e, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode,
                                                    f32 height, MeasureMode heightMode, bool place) const
    {
        // A result of this pass, or of an earlier one when nothing under the element changed, for the same constraints.
        bool clean = !Any(tree_.DirtyFlags(e) & Dirty::Layout);
        auto matches = [&](const Cache& c) {
            return c.valid && (clean || c.pass == pass_) && c.widthMode == widthMode && c.heightMode == heightMode && Same(c.width, width) &&
                   Same(c.height, height) && Same(c.ownerWidth, ownerWidth) && Same(c.ownerHeight, ownerHeight);
        };
        if (matches(s.laidOut))
            return &s.laidOut;
        if (!place)
            for (const Cache& m : s.measured)
                if (matches(m))
                    return &m;
        return nullptr;
    }

    bool LayoutEngine::IsCached(ElementHandle e, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height, MeasureMode heightMode,
                                bool place)
    {
        const Slot& s = At(e);
        return s.style.display == Display::None || Cached(s, e, ownerWidth, ownerHeight, width, widthMode, height, heightMode, place) != nullptr;
    }

    template <typename F>
    void LayoutEngine::RunAll(u32 count, u32 work, const F& f)
    {
        // Enough subtrees to compute go to the jobs once (their own children then run on the thread they are on).
        bool expected = false;
        if (work >= kParallelWork && jobs::IsInitialized() && jobs::WorkerCount() > 0 && parallel_.compare_exchange_strong(expected, true))
        {
            jobs::Complete(jobs::ParallelFor(count, 4, f));
            parallel_.store(false);
        }
        else
            for (u32 i = 0; i < count; i++)
                f(i);
    }

    Vector2 LayoutEngine::Compute(ElementHandle e, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height,
                                  MeasureMode heightMode, bool place)
    {
        Slot& s = At(e);
        const LayoutStyle& st = s.style;
        if (st.display == Display::None)
        {
            s.box = LayoutBox();
            return Vector2(0, 0);
        }
        if (const Cache* c = Cached(s, e, ownerWidth, ownerHeight, width, widthMode, height, heightMode, place))
        {
            if (place)
            {
                s.box.width = c->resultWidth;
                s.box.height = c->resultHeight;
            }
            return Vector2(c->resultWidth, c->resultHeight);
        }
        computed_.fetch_add(1, std::memory_order_relaxed);

        // Edges: paddings and margins in percent are of the owner's width.
        for (u32 k = 0; k < 4; k++)
        {
            s.box.padding[k] = Max0(ResolveOr(st.padding[k], ownerWidth, 0.0f));
            s.box.border[k] = Max0(st.border[k]);
            s.box.margin[k] = ResolveOr(st.margin[k], ownerWidth, 0.0f);
        }
        Vector2 size;
        bool hasFlowChildren = false;
        for (ElementHandle c = tree_.FirstChild(e); c.IsValid() && !hasFlowChildren; c = tree_.NextSibling(c))
            hasFlowChildren = true;
        if (!hasFlowChildren)
        {
            f32 edgeX = s.box.padding[kLeft] + s.box.padding[kRight] + s.box.border[kLeft] + s.box.border[kRight];
            f32 edgeY = s.box.padding[kTop] + s.box.padding[kBottom] + s.box.border[kTop] + s.box.border[kBottom];
            Vector2 content(0, 0);
            if (s.measure)
            {
                f32 innerW = widthMode == MeasureMode::Undefined ? kUndefined : Max0(width - edgeX);
                f32 innerH = heightMode == MeasureMode::Undefined ? kUndefined : Max0(height - edgeY);
                content = s.measure(s.user, e, innerW, widthMode, innerH, heightMode);
            }
            f32 minW, maxW, minH, maxH;
            MinMax(st, true, ownerWidth, minW, maxW);
            MinMax(st, false, ownerHeight, minH, maxH);
            auto fit = [](MeasureMode mode, f32 given, f32 wanted) {
                return mode == MeasureMode::Exactly ? given : mode == MeasureMode::AtMost && wanted > given ? given : wanted;
            };
            size.x = Max0(fit(widthMode, width, Clamp(content.x + edgeX, minW, maxW)));
            size.y = Max0(fit(heightMode, height, Clamp(content.y + edgeY, minH, maxH)));
            size.x = Clamp(size.x, edgeX, kInfinity);
            size.y = Clamp(size.y, edgeY, kInfinity);
        }
        else
            size = ComputeContainer(e, s, ownerWidth, ownerHeight, width, widthMode, height, heightMode, place);

        Cache& c = place ? s.laidOut : s.measured[s.nextMeasured++ % Slot::kMeasured];
        c.width = width, c.height = height, c.ownerWidth = ownerWidth, c.ownerHeight = ownerHeight;
        c.widthMode = widthMode, c.heightMode = heightMode;
        c.resultWidth = size.x, c.resultHeight = size.y;
        c.valid = true;
        c.pass = pass_;
        if (place)
        {
            s.laidPass = pass_;
            s.box.width = size.x;
            s.box.height = size.y;
        }
        return size;
    }

    Vector2 LayoutEngine::ComputeContainer(ElementHandle e, Slot& s, f32 ownerWidth, f32 ownerHeight, f32 width, MeasureMode widthMode, f32 height,
                                           MeasureMode heightMode, bool place)
    {
        const LayoutStyle& st = s.style;
        const bool row = IsRow(st.direction);
        const f32 edgeX = s.box.padding[kLeft] + s.box.padding[kRight] + s.box.border[kLeft] + s.box.border[kRight];
        const f32 edgeY = s.box.padding[kTop] + s.box.padding[kBottom] + s.box.border[kTop] + s.box.border[kBottom];
        const f32 innerW = widthMode == MeasureMode::Undefined ? kUndefined : Max0(width - edgeX);
        const f32 innerH = heightMode == MeasureMode::Undefined ? kUndefined : Max0(height - edgeY);
        const f32 mainInner = row ? innerW : innerH;
        const f32 crossInner = row ? innerH : innerW;
        const MeasureMode mainMode = row ? widthMode : heightMode;
        const MeasureMode crossMode = row ? heightMode : widthMode;
        const f32 edgeMain = row ? edgeX : edgeY;
        const f32 edgeCross = row ? edgeY : edgeX;
        const Edge mainStart = StartEdge(row), mainEnd = EndEdge(row), crossStart = StartEdge(!row), crossEnd = EndEdge(!row);
        // Percentages of the children are of this element's inner size where it is known.
        const f32 percentW = innerW, percentH = innerH;

        // The flow items and their hypothetical main sizes.
        // One list per nesting level, kept from call to call (each level's list stays where it is while deeper levels grow).
        static thread_local Array<std::unique_ptr<Array<Item>>> itemStack;
        static thread_local Array<std::unique_ptr<Array<Line>>> lineStack;
        static thread_local Array<std::unique_ptr<Array<Placement>>> placementStack;
        static thread_local u32 depth = 0;
        if (itemStack.Size() <= depth)
        {
            itemStack.Push(std::make_unique<Array<Item>>());
            lineStack.Push(std::make_unique<Array<Line>>());
            placementStack.Push(std::make_unique<Array<Placement>>());
        }
        Array<Item>& items = *itemStack[depth];
        Array<Line>& lines = *lineStack[depth];
        Array<Placement>& placements = *placementStack[depth];
        placements.Clear();
        items.Clear();
        depth++;
        for (ElementHandle c = tree_.FirstChild(e); c.IsValid(); c = tree_.NextSibling(c))
        {
            Slot& cs = At(c);
            const LayoutStyle& style = cs.style;
            if (style.display == Display::None)
            {
                if (place)
                    cs.box = LayoutBox();
                continue;
            }
            if (style.position == Position::Absolute)
                continue;
            Item& it = items.Emplace();
            it.element = c;
            it.style = &style;
            it.align = style.alignSelf == Align::Auto ? st.alignItems : style.alignSelf;
            for (u32 k = 0; k < 2; k++)
            {
                const Length& mm = style.margin[k == 0 ? mainStart : mainEnd];
                const Length& mc = style.margin[k == 0 ? crossStart : crossEnd];
                it.autoMain[k] = mm.IsAuto();
                it.autoCross[k] = mc.IsAuto();
                it.marginMain[k] = ResolveOr(mm, percentW, 0.0f);
                it.marginCross[k] = ResolveOr(mc, percentW, 0.0f);
            }
            f32 pad[4];
            for (u32 k = 0; k < 4; k++)
                pad[k] = Max0(ResolveOr(style.padding[k], percentW, 0.0f)) + Max0(style.border[k]);
            it.edgeMain = row ? pad[kLeft] + pad[kRight] : pad[kTop] + pad[kBottom];
            f32 childW = Resolve(style.width, percentW), childH = Resolve(style.height, percentH);
            if (style.aspectRatio > 0)
            {
                if (Defined(childW) && !Defined(childH))
                    childH = childW / style.aspectRatio;
                else if (Defined(childH) && !Defined(childW))
                    childW = childH * style.aspectRatio;
            }
            MinMax(style, row, row ? percentW : percentH, it.minMain, it.maxMain);
            MinMax(style, !row, row ? percentH : percentW, it.minCross, it.maxCross);
            f32 childMain = row ? childW : childH;
            it.crossSize = row ? childH : childW;
            if (Defined(it.crossSize))
                it.crossSize = Clamp(it.crossSize, it.minCross, it.maxCross);
            // A ratio with neither size given: the main size from the cross size the item is stretched to.
            if (style.aspectRatio > 0 && !Defined(childMain) && !Defined(it.crossSize) && Defined(crossInner) && crossMode == MeasureMode::Exactly &&
                it.align == Align::Stretch && !it.autoCross[0] && !it.autoCross[1])
            {
                f32 cross = Clamp(Max0(crossInner - it.marginCross[0] - it.marginCross[1]), it.minCross, it.maxCross);
                childMain = row ? cross * style.aspectRatio : cross / style.aspectRatio;
            }

            f32 basis = style.basis.IsAuto() ? kUndefined : Resolve(style.basis, mainInner);
            if (!Defined(basis))
                basis = childMain;
            if (!Defined(basis))
            {
                // The content's size along the main axis.
                f32 crossMargins = it.marginCross[0] + it.marginCross[1];
                f32 crossGiven = kUndefined;
                MeasureMode crossGivenMode = MeasureMode::Undefined;
                if (Defined(it.crossSize))
                    crossGiven = it.crossSize, crossGivenMode = MeasureMode::Exactly;
                else if (Defined(crossInner))
                {
                    bool stretch = it.align == Align::Stretch && crossMode == MeasureMode::Exactly && !it.autoCross[0] && !it.autoCross[1];
                    crossGiven = Max0(crossInner - crossMargins);
                    crossGivenMode = stretch ? MeasureMode::Exactly : MeasureMode::AtMost;
                }
                it.measure = true;
                it.measureCross = crossGiven;
                it.measureMode = crossGivenMode;
                continue;
            }
            it.basis = basis > it.edgeMain ? basis : it.edgeMain;
            it.hypothetical = Clamp(it.basis, Max0(it.minMain) > it.edgeMain ? it.minMain : it.edgeMain, it.maxMain);
        }
        // The content sizes: independent of each other, so many of them go to the jobs.
        {
            auto measure = [&](u32 i) {
                Item& it = items[i];
                if (!it.measure)
                    return;
                Vector2 measured = row ? Compute(it.element, percentW, percentH, kUndefined, MeasureMode::Undefined, it.measureCross, it.measureMode, false)
                                       : Compute(it.element, percentW, percentH, it.measureCross, it.measureMode, kUndefined, MeasureMode::Undefined, false);
                f32 basis = row ? measured.x : measured.y;
                it.basis = basis > it.edgeMain ? basis : it.edgeMain;
                it.hypothetical = Clamp(it.basis, Max0(it.minMain) > it.edgeMain ? it.minMain : it.edgeMain, it.maxMain);
            };
            u32 work = 0;
            for (const Item& it : items)
                if (it.measure && !IsCached(it.element, percentW, percentH, row ? kUndefined : it.measureCross, row ? MeasureMode::Undefined : it.measureMode,
                                            row ? it.measureCross : kUndefined, row ? it.measureMode : MeasureMode::Undefined, false))
                    work += 1 + tree_.ChildCount(it.element);
            RunAll(u32(items.Size()), work, measure);
        }

        // The element's own limits along both axes (for sizes that are not given exactly).
        f32 ownMinMain, ownMaxMain, ownMinCross, ownMaxCross;
        MinMax(st, row, row ? ownerWidth : ownerHeight, ownMinMain, ownMaxMain);
        MinMax(st, !row, row ? ownerHeight : ownerWidth, ownMinCross, ownMaxCross);

        // Lines.
        lines.Clear();
        {
            Line line;
            f32 used = 0;
            for (u32 i = 0; i < items.Size(); i++)
            {
                f32 outer = items[i].hypothetical + items[i].marginMain[0] + items[i].marginMain[1];
                if (st.wrap != FlexWrap::NoWrap && Defined(mainInner) && line.count > 0 && used + outer > mainInner)
                {
                    lines.Push(line);
                    line = Line{i, 0};
                    used = 0;
                }
                line.count++;
                used += outer;
            }
            if (line.count > 0 || lines.Empty())
                lines.Push(line);
        }

        // Flexible lengths per line, then the items' cross sizes. A measurement with the main size given whose items know
        // their cross sizes needs only the lines' cross sizes.
        bool crossKnown = true;
        for (const Item& it : items)
            crossKnown = crossKnown && Defined(it.crossSize);
        const bool linesOnly = !place && mainMode == MeasureMode::Exactly && crossKnown;
        f32 usedMain = 0;
        for (Line& line : lines)
        {
            if (linesOnly)
            {
                line.cross = 0;
                for (u32 i = line.first; i < line.first + line.count; i++)
                {
                    f32 outer = items[i].crossSize + items[i].marginCross[0] + items[i].marginCross[1];
                    if (outer > line.cross)
                        line.cross = outer;
                }
                continue;
            }
            f32 used = 0;
            for (u32 i = line.first; i < line.first + line.count; i++)
                used += items[i].hypothetical + items[i].marginMain[0] + items[i].marginMain[1];
            f32 available = used;
            if (mainMode == MeasureMode::Exactly)
                available = mainInner;
            else if (mainMode == MeasureMode::AtMost && used > mainInner)
                available = mainInner;
            f32 minInner = Max0(ownMinMain - edgeMain), maxInner = Max0(ownMaxMain - edgeMain);
            if (mainMode != MeasureMode::Exactly)
                available = Clamp(available, minInner, maxInner);
            bool growing = available > used;
            for (u32 i = line.first; i < line.first + line.count; i++)
            {
                Item& it = items[i];
                f32 factor = growing ? it.style->grow : it.style->shrink;
                it.target = it.hypothetical;
                it.frozen = factor <= 0 || (growing && it.basis > it.hypothetical) || (!growing && it.basis < it.hypothetical) || available == used;
            }
            f32 initialFree = available - used;
            for (u32 round = 0; round <= line.count; round++)
            {
                f32 frozenSpace = 0, unfrozenBasis = 0, factors = 0, scaled = 0;
                u32 open = 0;
                for (u32 i = line.first; i < line.first + line.count; i++)
                {
                    Item& it = items[i];
                    f32 margins = it.marginMain[0] + it.marginMain[1];
                    if (it.frozen)
                        frozenSpace += it.target + margins;
                    else
                    {
                        unfrozenBasis += it.basis + margins;
                        factors += growing ? it.style->grow : it.style->shrink;
                        scaled += it.style->shrink * it.basis;
                        open++;
                    }
                }
                if (open == 0)
                    break;
                f32 free = available - frozenSpace - unfrozenBasis;
                if (factors < 1)
                {
                    f32 limited = initialFree * factors;
                    if (std::fabs(limited) < std::fabs(free))
                        free = limited;
                }
                f32 violation = 0;
                for (u32 i = line.first; i < line.first + line.count; i++)
                {
                    Item& it = items[i];
                    if (it.frozen)
                        continue;
                    f32 wanted = it.basis;
                    if (growing && factors > 0)
                        wanted = it.basis + free * it.style->grow / factors;
                    else if (!growing && scaled > 0)
                        wanted = it.basis + free * (it.style->shrink * it.basis) / scaled;
                    f32 lo = it.minMain > it.edgeMain ? it.minMain : it.edgeMain;
                    f32 clamped = Clamp(wanted, lo, it.maxMain);
                    violation += clamped - wanted;
                    it.target = clamped;
                }
                for (u32 i = line.first; i < line.first + line.count; i++)
                {
                    Item& it = items[i];
                    if (it.frozen)
                        continue;
                    f32 lo = it.minMain > it.edgeMain ? it.minMain : it.edgeMain;
                    bool atMin = it.target <= lo && lo > 0;
                    bool atMax = it.target >= it.maxMain;
                    if (violation == 0 || (violation > 0 && atMin) || (violation < 0 && atMax))
                        it.frozen = true;
                }
                if (violation == 0)
                    break;
            }
            // The cross sizes the items want for their final main sizes.
            line.cross = 0;
            f32 lineMain = 0;
            for (u32 i = line.first; i < line.first + line.count; i++)
            {
                Item& it = items[i];
                lineMain += it.target + it.marginMain[0] + it.marginMain[1];
                bool stretched = it.align == Align::Stretch && crossMode == MeasureMode::Exactly && lines.Size() == 1 && !it.autoCross[0] &&
                                 !it.autoCross[1];
                if (Defined(it.crossSize))
                    it.cross = it.crossSize;
                else if (stretched)
                    it.cross = Clamp(Max0(crossInner - it.marginCross[0] - it.marginCross[1]), it.minCross, it.maxCross);
                else
                {
                    f32 crossMargins = it.marginCross[0] + it.marginCross[1];
                    f32 given = Defined(crossInner) ? Max0(crossInner - crossMargins) : kUndefined;
                    MeasureMode mode = Defined(crossInner) ? MeasureMode::AtMost : MeasureMode::Undefined;
                    Vector2 measured = row ? Compute(it.element, percentW, percentH, it.target, MeasureMode::Exactly, given, mode, false)
                                           : Compute(it.element, percentW, percentH, given, mode, it.target, MeasureMode::Exactly, false);
                    it.cross = Clamp(row ? measured.y : measured.x, it.minCross, it.maxCross);
                }
                f32 outer = it.cross + it.marginCross[0] + it.marginCross[1];
                if (outer > line.cross)
                    line.cross = outer;
            }
            if (lineMain > usedMain)
                usedMain = lineMain;
        }
        if (lines.Size() == 1 && crossMode == MeasureMode::Exactly)
            lines[0].cross = crossInner;
        f32 totalCross = 0;
        for (const Line& line : lines)
            totalCross += line.cross;

        // The element's size.
        f32 mainSize = mainMode == MeasureMode::Exactly ? (row ? width : height) : Clamp(usedMain + edgeMain, ownMinMain, ownMaxMain);
        if (mainMode == MeasureMode::AtMost && mainSize > (row ? width : height))
            mainSize = row ? width : height;
        f32 crossSize = crossMode == MeasureMode::Exactly ? (row ? height : width) : Clamp(totalCross + edgeCross, ownMinCross, ownMaxCross);
        if (crossMode == MeasureMode::AtMost && crossSize > (row ? height : width))
            crossSize = row ? height : width;
        mainSize = Max0(mainSize);
        crossSize = Max0(crossSize);
        Vector2 size = row ? Vector2(mainSize, crossSize) : Vector2(crossSize, mainSize);
        if (!place)
        {
            depth--;
            return size;
        }

        // Placing: the lines across, then each line's items along and across.
        const f32 finalMain = Max0(mainSize - edgeMain);
        const f32 finalCross = Max0(crossSize - edgeCross);
        const f32 originMain = row ? s.box.padding[kLeft] + s.box.border[kLeft] : s.box.padding[kTop] + s.box.border[kTop];
        const f32 originCross = row ? s.box.padding[kTop] + s.box.border[kTop] : s.box.padding[kLeft] + s.box.border[kLeft];
        if (lines.Size() == 1 && st.wrap == FlexWrap::NoWrap)
            lines[0].cross = finalCross;
        {
            f32 free = finalCross - totalCross;
            f32 at = 0, between = 0;
            u32 n = u32(lines.Size());
            if (st.wrap != FlexWrap::NoWrap)
                switch (st.alignContent)
                {
                case Align::FlexEnd: at = free; break;
                case Align::Center: at = free * 0.5f; break;
                case Align::SpaceBetween: between = n > 1 && free > 0 ? free / f32(n - 1) : 0; break;
                case Align::SpaceAround: between = free > 0 ? free / f32(n) : 0, at = between * 0.5f; break;
                case Align::SpaceEvenly: between = free > 0 ? free / f32(n + 1) : 0, at = between; break;
                case Align::Stretch:
                    if (free > 0)
                        for (Line& line : lines)
                            line.cross += free / f32(n);
                    break;
                default: break;
                }
            for (Line& line : lines)
            {
                line.offset = at;
                at += line.cross + between;
            }
            if (st.wrap == FlexWrap::WrapReverse)
                for (Line& line : lines)
                    line.offset = finalCross - line.offset - line.cross;
        }
        const bool reverse = IsReverse(st.direction);
        for (const Line& line : lines)
        {
            f32 used = 0;
            u32 autos = 0;
            for (u32 i = line.first; i < line.first + line.count; i++)
            {
                const Item& it = items[i];
                used += it.target + it.marginMain[0] + it.marginMain[1];
                autos += u32(it.autoMain[0]) + u32(it.autoMain[1]);
            }
            f32 free = finalMain - used;
            f32 autoShare = autos > 0 && free > 0 ? free / f32(autos) : 0;
            if (autos > 0 && free > 0)
                free = 0;
            f32 at = 0, between = 0;
            u32 n = line.count;
            switch (st.justify)
            {
            case Justify::FlexEnd: at = free; break;
            case Justify::Center: at = free * 0.5f; break;
            case Justify::SpaceBetween: between = n > 1 && free > 0 ? free / f32(n - 1) : 0; break;
            case Justify::SpaceAround: between = n > 0 && free > 0 ? free / f32(n) : 0, at = between * 0.5f; break;
            case Justify::SpaceEvenly: between = free > 0 ? free / f32(n + 1) : 0, at = between; break;
            default: break;
            }
            for (u32 i = line.first; i < line.first + line.count; i++)
            {
                Item& it = items[i];
                f32 startMargin = it.autoMain[0] ? autoShare : it.marginMain[0];
                f32 endMargin = it.autoMain[1] ? autoShare : it.marginMain[1];
                f32 mainPos = at + startMargin;
                at += startMargin + it.target + endMargin + between;
                if (reverse)
                    mainPos = finalMain - mainPos - it.target;

                // Across the line: stretched, or placed by its alignment (auto margins first).
                f32 crossItem = it.cross;
                f32 crossSpace = line.cross - it.marginCross[0] - it.marginCross[1];
                if (it.align == Align::Stretch && !Defined(it.crossSize) && !it.autoCross[0] && !it.autoCross[1])
                    crossItem = Clamp(Max0(crossSpace), it.minCross, it.maxCross);
                f32 crossFree = crossSpace - crossItem;
                f32 crossPos = it.marginCross[0];
                if (it.autoCross[0] && it.autoCross[1])
                    crossPos += crossFree * 0.5f;
                else if (it.autoCross[0])
                    crossPos += crossFree;
                else if (!it.autoCross[1])
                {
                    if (it.align == Align::Center)
                        crossPos += crossFree * 0.5f;
                    else if (it.align == Align::FlexEnd)
                        crossPos += crossFree;
                }
                crossPos += line.offset;

                Slot& cs = At(it.element);
                f32 w = row ? it.target : crossItem, h = row ? crossItem : it.target;
                f32 x = row ? originMain + mainPos : originCross + crossPos;
                f32 y = row ? originCross + crossPos : originMain + mainPos;
                // A relatively positioned element moves by its insets.
                const LayoutStyle& style = *it.style;
                f32 dx = ResolveOr(style.inset[kLeft], percentW, kUndefined), dy = ResolveOr(style.inset[kTop], percentH, kUndefined);
                if (!Defined(dx))
                    dx = -ResolveOr(style.inset[kRight], percentW, 0.0f);
                if (!Defined(dy))
                    dy = -ResolveOr(style.inset[kBottom], percentH, 0.0f);
                (void)cs;
                placements.Push({it.element, row ? finalMain : finalCross, row ? finalCross : finalMain, w, h, x + dx, y + dy});
            }
        }
        // The children's subtrees: independent now that their places are known, so many of them go to the jobs.
        auto layOut = [&](u32 i) {
            const Placement& p = placements[i];
            Compute(p.element, p.ownerWidth, p.ownerHeight, p.width, MeasureMode::Exactly, p.height, MeasureMode::Exactly, true);
        };
        u32 work = 0;
        for (const Placement& p : placements)
            if (!IsCached(p.element, p.ownerWidth, p.ownerHeight, p.width, MeasureMode::Exactly, p.height, MeasureMode::Exactly, true))
                work += 1 + tree_.ChildCount(p.element);
        RunAll(u32(placements.Size()), work, layOut);
        for (const Placement& p : placements)
        {
            Slot& cs = At(p.element);
            cs.box.x = p.x;
            cs.box.y = p.y;
        }
        for (ElementHandle c = tree_.FirstChild(e); c.IsValid(); c = tree_.NextSibling(c))
        {
            const LayoutStyle& style = At(c).style;
            if (style.display != Display::None && style.position == Position::Absolute)
                PlaceAbsolute(c, s, size.x, size.y);
        }
        depth--;
        return size;
    }

    void LayoutEngine::PlaceAbsolute(ElementHandle child, const Slot& parent, f32 width, f32 height)
    {
        Slot& cs = At(child);
        const LayoutStyle& style = cs.style;
        // The parent's padding box.
        f32 boxX = parent.box.border[kLeft], boxY = parent.box.border[kTop];
        f32 boxW = Max0(width - parent.box.border[kLeft] - parent.box.border[kRight]);
        f32 boxH = Max0(height - parent.box.border[kTop] - parent.box.border[kBottom]);
        f32 margin[4];
        for (u32 k = 0; k < 4; k++)
            margin[k] = ResolveOr(style.margin[k], boxW, 0.0f);
        f32 left = Resolve(style.inset[kLeft], boxW), right = Resolve(style.inset[kRight], boxW);
        f32 top = Resolve(style.inset[kTop], boxH), bottom = Resolve(style.inset[kBottom], boxH);
        f32 w = Resolve(style.width, boxW), h = Resolve(style.height, boxH);
        if (!Defined(w) && Defined(left) && Defined(right))
            w = Max0(boxW - left - right - margin[kLeft] - margin[kRight]);
        if (!Defined(h) && Defined(top) && Defined(bottom))
            h = Max0(boxH - top - bottom - margin[kTop] - margin[kBottom]);
        if (style.aspectRatio > 0)
        {
            if (Defined(w) && !Defined(h))
                h = w / style.aspectRatio;
            else if (Defined(h) && !Defined(w))
                w = h * style.aspectRatio;
        }
        if (!Defined(w) || !Defined(h))
        {
            Vector2 measured = Compute(child, boxW, boxH, Defined(w) ? w : kUndefined, Defined(w) ? MeasureMode::Exactly : MeasureMode::Undefined,
                                       Defined(h) ? h : kUndefined, Defined(h) ? MeasureMode::Exactly : MeasureMode::Undefined, false);
            if (!Defined(w))
                w = measured.x;
            if (!Defined(h))
                h = measured.y;
        }
        f32 lo, hi;
        MinMax(style, true, boxW, lo, hi);
        w = Clamp(w, lo, hi);
        MinMax(style, false, boxH, lo, hi);
        h = Clamp(h, lo, hi);
        Compute(child, boxW, boxH, w, MeasureMode::Exactly, h, MeasureMode::Exactly, true);
        f32 x = Defined(left) ? boxX + left + margin[kLeft]
                              : Defined(right) ? boxX + boxW - right - margin[kRight] - w : boxX + parent.box.padding[kLeft] + margin[kLeft];
        f32 y = Defined(top) ? boxY + top + margin[kTop]
                             : Defined(bottom) ? boxY + boxH - bottom - margin[kBottom] - h : boxY + parent.box.padding[kTop] + margin[kTop];
        cs.box.x = x;
        cs.box.y = y;
    }
}
