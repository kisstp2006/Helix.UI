#include <Helix/UI/Engine/Implementation/GuiState.h>

#include <Helix/Core/Logging/Log.h>

#include <cmath>
#include <memory>

// GuiLayout: the Layout pass records each call as an entry (its smallest and largest size, whether it stretches, its
// margins) in groups; after the pass the groups are laid out (widths top down, then the heights that depend on them);
// the other passes make the same calls and get the entries' rects in order. Areas, scroll views and windows start new
// coordinates (their contents are in their own space, as the clips they push).
namespace helix::ui::gui
{
    using namespace detail;

    namespace detail
    {
        struct Node
        {
            enum Kind : u8
            {
                Leaf,
                Group,
                Space,
                Flexible,
            };
            Kind kind = Leaf;
            f32 minW = 0, maxW = 0, minH = 0, maxH = 0;
            // How much of the room left over an entry takes (a weight: a flexible space takes nearly all of it).
            f32 stretchW = 0, stretchH = 0;
            // Fixed by an option or the style (no stretching on that axis).
            bool fixedW = false, fixedH = false;
            i32 margin[4] = {0, 0, 0, 0};
            Rect rect;
            // A wrapped leaf: its height from its width (its style and content), or an aspect.
            const GuiStyle* style = nullptr;
            String text;
            engine::Texture2D* image = nullptr;
            f32 aspect = 0;
            // Groups.
            bool vertical = true;
            i32 padding[4] = {0, 0, 0, 0};
            Array<u32> children;
            u32 cursor = 0;
            // An area: its rect fixed, not in its parent's sizes; a scroll view: its content is its one child.
            bool area = false, scroll = false;
            Rect fixed;
            bool alwaysH = false, alwaysV = false;
            const GuiStyle* hbar = nullptr;
            const GuiStyle* vbar = nullptr;
            const GuiStyle* background = nullptr;
            Vector2 scrollPosition;
        };

        struct LayoutCache
        {
            Array<Node> nodes;
            Array<u32> stack;
            bool recording = false;
            Rect lastRect;
        };
    }

    namespace
    {
        LogChannel LayoutLog{"UI"};

        struct Current
        {
            LayoutCache* cache = nullptr;
        };

        Current& C()
        {
            static Current c;
            return c;
        }

        void Apply(Node& n, Options options)
        {
            for (GuiLayoutOption* o : options)
            {
                if (!o)
                    continue;
                f32 v = o->value;
                switch (o->kind)
                {
                case GuiLayoutOption::Kind::FixedWidth:
                    n.minW = n.maxW = v, n.stretchW = 0, n.fixedW = true;
                    break;
                case GuiLayoutOption::Kind::FixedHeight:
                    n.minH = n.maxH = v, n.stretchH = 0, n.fixedH = true;
                    break;
                case GuiLayoutOption::Kind::MinWidth:
                    n.minW = v, n.maxW = n.maxW < v ? v : n.maxW;
                    break;
                case GuiLayoutOption::Kind::MaxWidth:
                    n.maxW = v, n.minW = n.minW > v ? v : n.minW, n.stretchW = 0;
                    break;
                case GuiLayoutOption::Kind::MinHeight:
                    n.minH = v, n.maxH = n.maxH < v ? v : n.maxH;
                    break;
                case GuiLayoutOption::Kind::MaxHeight:
                    n.maxH = v, n.minH = n.minH > v ? v : n.minH, n.stretchH = 0;
                    break;
                case GuiLayoutOption::Kind::StretchWidth:
                    n.stretchW = v;
                    break;
                case GuiLayoutOption::Kind::StretchHeight:
                    n.stretchH = v;
                    break;
                }
            }
        }

        // The options as they were given (kept for the passes after Layout: a group's fixed sizes).
        void Margins(Node& n, const GuiStyle* style)
        {
            if (!style)
                return;
            const RectOffset* m = style->margin();
            n.margin[0] = m->left(), n.margin[1] = m->right(), n.margin[2] = m->top(), n.margin[3] = m->bottom();
        }

        LayoutCache* Cache()
        {
            LayoutCache* c = C().cache;
            if (!c)
                HX_LOG_ERROR(LayoutLog, "GUI: a GuiLayout call outside a layout (useGuiLayout is off, or outside OnGui).");
            return c;
        }

        Node& Top(LayoutCache& c) { return c.nodes[c.stack.Back()]; }

        // Layout: a new entry in the current group; the other passes: the group's next entry (of the same kind).
        Node* Take(Node::Kind kind)
        {
            LayoutCache* c = Cache();
            if (!c || c->stack.Empty())
                return nullptr;
            if (c->recording)
            {
                Node n;
                n.kind = kind;
                u32 index = u32(c->nodes.Size());
                c->nodes.Push(Move(n));
                c->nodes[c->stack.Back()].children.Push(index);
                return &c->nodes[index];
            }
            Node& group = Top(*c);
            if (group.cursor >= group.children.Size() || c->nodes[group.children[group.cursor]].kind != kind)
            {
                State& s = S();
                if (!s.layoutMismatchSaid)
                    HX_LOG_ERROR(LayoutLog, "GUI: the GuiLayout calls of a pass differ from its Layout pass's (calls must not change between them).");
                s.layoutMismatchSaid = true;
                return nullptr;
            }
            return &c->nodes[group.children[group.cursor++]];
        }

        // --------------------------------------------------------------------------------------------- the solver

        bool Excluded(const Node& n) { return n.area; }

        void CalcWidth(LayoutCache& c, u32 index);

        void GroupWidths(LayoutCache& c, Node& g)
        {
            f32 minW = 0, maxW = 0;
            i32 prevMargin = 0;
            bool first = true;
            for (u32 k : g.children)
            {
                CalcWidth(c, k);
                Node& ch = c.nodes[k];
                if (Excluded(ch))
                    continue;
                if (g.vertical)
                {
                    f32 m = f32(ch.margin[0] + ch.margin[1]);
                    minW = ch.minW + m > minW ? ch.minW + m : minW;
                    maxW = ch.maxW + m > maxW ? ch.maxW + m : maxW;
                }
                else
                {
                    // The margins between two entries: the larger of them.
                    f32 gap = first ? f32(ch.margin[0]) : f32(prevMargin > ch.margin[0] ? prevMargin : ch.margin[0]);
                    minW += gap + ch.minW;
                    maxW += gap + ch.maxW;
                    prevMargin = ch.margin[1];
                    first = false;
                }
            }
            if (!g.vertical && !first)
                minW += f32(prevMargin), maxW += f32(prevMargin);
            f32 pad = f32(g.padding[0] + g.padding[1]);
            if (!g.fixedW)
            {
                f32 lo = minW + pad, hi = maxW + pad;
                g.minW = g.minW > lo ? g.minW : lo;
                g.maxW = g.maxW > hi ? g.maxW : hi;
            }
        }

        void CalcWidth(LayoutCache& c, u32 index)
        {
            Node& n = c.nodes[index];
            if (n.kind != Node::Group)
                return;
            if (n.scroll)
            {
                // A scroll view: its content's widths, with room for a vertical bar; it can be narrower (it scrolls).
                u32 content = n.children.Empty() ? ~0u : n.children[0];
                f32 cmax = 0;
                if (content != ~0u)
                {
                    CalcWidth(c, content);
                    Node& ch = c.nodes[content];
                    cmax = ch.maxW + f32(ch.margin[0] + ch.margin[1]);
                }
                f32 bar = n.vbar ? n.vbar->fixedWidth() + f32(n.vbar->margin()->left()) : 0;
                if (!n.fixedW)
                {
                    n.minW = n.minW > bar ? n.minW : bar;
                    n.maxW = n.maxW > cmax + bar ? n.maxW : cmax + bar;
                }
                return;
            }
            GroupWidths(c, n);
        }

        void SetHorizontal(LayoutCache& c, u32 index, f32 x, f32 width);

        void GroupHorizontal(LayoutCache& c, Node& g)
        {
            f32 x0 = g.rect.x + f32(g.padding[0]), room = g.rect.width - f32(g.padding[0] + g.padding[1]);
            // An area's or a window's content is in its own space.
            if (g.area)
                x0 = f32(g.padding[0]);
            Array<u32> kids;
            for (u32 k : g.children)
                if (!Excluded(c.nodes[k]))
                    kids.Push(k);
            if (g.vertical)
            {
                for (u32 k : kids)
                {
                    Node& ch = c.nodes[k];
                    f32 avail = room - f32(ch.margin[0] + ch.margin[1]);
                    f32 w = ch.stretchW > 0 ? (avail > ch.minW ? avail : ch.minW) : (avail < ch.minW ? ch.minW : avail > ch.maxW ? ch.maxW : avail);
                    SetHorizontal(c, k, x0 + f32(ch.margin[0]), w);
                }
            }
            else
            {
                f32 minSum = 0, maxSum = 0, gaps = 0;
                i32 prev = 0;
                f32 weights = 0;
                for (usize i = 0; i < kids.Size(); i++)
                {
                    Node& ch = c.nodes[kids[i]];
                    gaps += i == 0 ? f32(ch.margin[0]) : f32(prev > ch.margin[0] ? prev : ch.margin[0]);
                    prev = ch.margin[1];
                    minSum += ch.minW;
                    maxSum += ch.maxW;
                    weights += ch.stretchW;
                }
                if (!kids.Empty())
                    gaps += f32(prev);
                f32 avail = room - gaps;
                f32 t = maxSum > minSum ? (avail - minSum) / (maxSum - minSum) : 1;
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                f32 extra = avail > maxSum && weights > 0 ? avail - maxSum : 0;
                f32 at = x0;
                prev = 0;
                for (usize i = 0; i < kids.Size(); i++)
                {
                    Node& ch = c.nodes[kids[i]];
                    at += i == 0 ? f32(ch.margin[0]) : f32(prev > ch.margin[0] ? prev : ch.margin[0]);
                    prev = ch.margin[1];
                    f32 w = ch.minW + (ch.maxW - ch.minW) * t + (weights > 0 ? extra * ch.stretchW / weights : 0);
                    SetHorizontal(c, kids[i], std::round(at), std::round(w));
                    at += w;
                }
            }
            // Areas: their own rects, their contents in their space.
            for (u32 k : g.children)
                if (Excluded(c.nodes[k]))
                {
                    Node& a = c.nodes[k];
                    SetHorizontal(c, k, a.fixed.x, a.fixed.width);
                }
        }

        void SetHorizontal(LayoutCache& c, u32 index, f32 x, f32 width)
        {
            Node& n = c.nodes[index];
            n.rect.x = x;
            n.rect.width = width;
            if (n.kind != Node::Group)
                return;
            if (n.scroll)
            {
                if (n.children.Empty())
                    return;
                Node& ch = c.nodes[n.children[0]];
                f32 bar = n.vbar ? n.vbar->fixedWidth() + f32(n.vbar->margin()->left()) : 0;
                // The content as wide as the view (less a bar it may need), or its smallest width.
                f32 w = width - (n.alwaysV ? bar : 0);
                w = w > ch.minW ? w : ch.minW;
                SetHorizontal(c, n.children[0], f32(ch.margin[0]), w - f32(ch.margin[0] + ch.margin[1]));
                return;
            }
            GroupHorizontal(c, n);
        }

        void CalcHeight(LayoutCache& c, u32 index)
        {
            Node& n = c.nodes[index];
            if (n.kind == Node::Leaf && !n.fixedH)
            {
                if (n.aspect > 0)
                    n.minH = n.maxH = std::round(n.rect.width / n.aspect);
                else if (n.style && n.style->isHeightDependantOnWidth())
                {
                    GuiContent& content = GuiContent::Temp(n.text.View(), n.image);
                    f32 h = n.style->CalcHeight(&content, n.rect.width);
                    n.minH = n.maxH = h;
                }
                return;
            }
            if (n.kind != Node::Group)
                return;
            if (n.scroll)
            {
                f32 cmax = 0;
                if (!n.children.Empty())
                {
                    CalcHeight(c, n.children[0]);
                    Node& ch = c.nodes[n.children[0]];
                    cmax = ch.maxH + f32(ch.margin[2] + ch.margin[3]);
                }
                f32 bar = n.hbar ? n.hbar->fixedHeight() + f32(n.hbar->margin()->top()) : 0;
                if (!n.fixedH)
                {
                    n.minH = n.minH > bar ? n.minH : bar;
                    n.maxH = n.maxH > cmax + bar ? n.maxH : cmax + bar;
                }
                return;
            }
            f32 minH = 0, maxH = 0;
            i32 prev = 0;
            bool first = true;
            for (u32 k : n.children)
            {
                CalcHeight(c, k);
                Node& ch = c.nodes[k];
                if (Excluded(ch))
                    continue;
                if (!n.vertical)
                {
                    f32 m = f32(ch.margin[2] + ch.margin[3]);
                    minH = ch.minH + m > minH ? ch.minH + m : minH;
                    maxH = ch.maxH + m > maxH ? ch.maxH + m : maxH;
                }
                else
                {
                    f32 gap = first ? f32(ch.margin[2]) : f32(prev > ch.margin[2] ? prev : ch.margin[2]);
                    minH += gap + ch.minH;
                    maxH += gap + ch.maxH;
                    prev = ch.margin[3];
                    first = false;
                }
            }
            if (n.vertical && !first)
                minH += f32(prev), maxH += f32(prev);
            f32 pad = f32(n.padding[2] + n.padding[3]);
            if (!n.fixedH)
            {
                f32 lo = minH + pad, hi = maxH + pad;
                n.minH = n.minH > lo ? n.minH : lo;
                n.maxH = n.maxH > hi ? n.maxH : hi;
            }
        }

        void SetVertical(LayoutCache& c, u32 index, f32 y, f32 height)
        {
            Node& n = c.nodes[index];
            n.rect.y = y;
            n.rect.height = height;
            if (n.kind != Node::Group)
                return;
            if (n.scroll)
            {
                if (n.children.Empty())
                    return;
                Node& ch = c.nodes[n.children[0]];
                SetVertical(c, n.children[0], f32(ch.margin[2]), ch.minH);
                return;
            }
            f32 y0 = (n.area ? 0 : n.rect.y) + f32(n.padding[2]), room = n.rect.height - f32(n.padding[2] + n.padding[3]);
            Array<u32> kids;
            for (u32 k : n.children)
                if (!Excluded(c.nodes[k]))
                    kids.Push(k);
            if (!n.vertical)
            {
                for (u32 k : kids)
                {
                    Node& ch = c.nodes[k];
                    f32 avail = room - f32(ch.margin[2] + ch.margin[3]);
                    f32 h = ch.stretchH > 0 ? (avail > ch.minH ? avail : ch.minH) : (avail < ch.minH ? ch.minH : avail > ch.maxH ? ch.maxH : avail);
                    SetVertical(c, k, y0 + f32(ch.margin[2]), h);
                }
            }
            else
            {
                f32 minSum = 0, maxSum = 0, gaps = 0;
                i32 prev = 0;
                f32 weights = 0;
                for (usize i = 0; i < kids.Size(); i++)
                {
                    Node& ch = c.nodes[kids[i]];
                    gaps += i == 0 ? f32(ch.margin[2]) : f32(prev > ch.margin[2] ? prev : ch.margin[2]);
                    prev = ch.margin[3];
                    minSum += ch.minH;
                    maxSum += ch.maxH;
                    weights += ch.stretchH;
                }
                if (!kids.Empty())
                    gaps += f32(prev);
                f32 avail = room - gaps;
                f32 t = maxSum > minSum ? (avail - minSum) / (maxSum - minSum) : 1;
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                f32 extra = avail > maxSum && weights > 0 ? avail - maxSum : 0;
                f32 at = y0;
                prev = 0;
                for (usize i = 0; i < kids.Size(); i++)
                {
                    Node& ch = c.nodes[kids[i]];
                    at += i == 0 ? f32(ch.margin[2]) : f32(prev > ch.margin[2] ? prev : ch.margin[2]);
                    prev = ch.margin[3];
                    f32 h = ch.minH + (ch.maxH - ch.minH) * t + (weights > 0 ? extra * ch.stretchH / weights : 0);
                    SetVertical(c, kids[i], std::round(at), std::round(h));
                    at += h;
                }
            }
            for (u32 k : n.children)
                if (Excluded(c.nodes[k]))
                {
                    Node& a = c.nodes[k];
                    SetVertical(c, k, a.fixed.y, a.fixed.height);
                }
        }

        void Solve(LayoutCache& c)
        {
            if (c.nodes.Empty())
                return;
            Node& root = c.nodes[0];
            Rect area = root.fixed;
            CalcWidth(c, 0);
            SetHorizontal(c, 0, area.x, area.width);
            CalcHeight(c, 0);
            SetVertical(c, 0, area.y, area.height);
        }

        // --------------------------------------------------------------------------------------------- entries

        Rect Leaf(f32 minW, f32 maxW, f32 minH, f32 maxH, const GuiStyle* style, const GuiContent* content, Options options, f32 aspect = 0)
        {
            Node* n = Take(Node::Leaf);
            if (!n)
                return Rect(0, 0, 0, 0);
            LayoutCache& c = *C().cache;
            if (c.recording)
            {
                n->minW = minW, n->maxW = maxW, n->minH = minH, n->maxH = maxH;
                n->stretchW = (style ? style->stretchWidth() && style->fixedWidth() == 0 : true) ? 1.0f : 0.0f;
                n->stretchH = (style ? style->stretchHeight() && style->fixedHeight() == 0 : false) ? 1.0f : 0.0f;
                n->fixedW = style && style->fixedWidth() != 0;
                n->fixedH = style && style->fixedHeight() != 0;
                n->aspect = aspect;
                Margins(*n, style);
                if (style && style->isHeightDependantOnWidth() && content)
                {
                    n->style = style;
                    n->text = content->text();
                    n->image = content->image();
                }
                Apply(*n, options);
                c.lastRect = Rect(0, 0, 1, 1);
                return c.lastRect;
            }
            c.lastRect = n->rect;
            return n->rect;
        }

        Rect ContentRect(const GuiContent& content, const GuiStyle& style, Options options)
        {
            Vector2 size = style.CalcSize(const_cast<GuiContent*>(&content));
            f32 minW = size.x, maxW = size.x;
            if (style.wordWrap())
                style.CalcMinMaxWidth(const_cast<GuiContent*>(&content), minW, maxW);
            return Leaf(minW, maxW, size.y, size.y, &style, &content, options);
        }

        void BeginGroup(bool vertical, const GuiContent& content, const GuiStyle* style, Options options)
        {
            Node* n = Take(Node::Group);
            if (!n)
                return;
            LayoutCache& c = *C().cache;
            u32 index = u32(n - c.nodes.Data());
            if (c.recording)
            {
                n->vertical = vertical;
                n->stretchW = (style ? style->stretchWidth() : true) ? 1.0f : 0.0f;
                n->stretchH = (style ? style->stretchHeight() : false) ? 1.0f : 0.0f;
                Margins(*n, style);
                if (style)
                {
                    const RectOffset* p = style->padding();
                    n->padding[0] = p->left(), n->padding[1] = p->right(), n->padding[2] = p->top(), n->padding[3] = p->bottom();
                    if (style->fixedWidth() != 0)
                        n->minW = n->maxW = style->fixedWidth(), n->fixedW = true, n->stretchW = 0;
                    if (style->fixedHeight() != 0)
                        n->minH = n->maxH = style->fixedHeight(), n->fixedH = true, n->stretchH = 0;
                }
                Apply(*n, options);
            }
            else
            {
                n->cursor = 0;
                if (style && style != GuiStyle::none() && Repainting())
                    style->DrawState(n->rect, content, false, false, false, false);
            }
            c.stack.Push(index);
        }

        void EndGroup()
        {
            LayoutCache* c = Cache();
            if (!c || c->stack.Size() <= 1)
            {
                if (c)
                    HX_LOG_ERROR(LayoutLog, "GUI: a GuiLayout End without its Begin.");
                return;
            }
            c->lastRect = c->recording ? Rect(0, 0, 1, 1) : Top(*c).rect;
            c->stack.Pop();
        }

        GuiStyle* Or(GuiStyle* style, GuiStyle* fallback) { return style ? style : fallback; }
    }

    namespace detail
    {
        LayoutCache* NewLayoutCache() { return new LayoutCache(); }
        void DeleteLayoutCache(LayoutCache* c) { delete c; }

        void BeginLayout(LayoutCache* cache, const Rect& area, const GuiStyle* style)
        {
            if (!cache)
                return;
            C().cache = cache;
            cache->stack.Clear();
            cache->recording = Layouting();
            if (cache->recording)
            {
                cache->nodes.Clear();
                Node root;
                root.kind = Node::Group;
                root.vertical = true;
                root.area = true;
                root.fixed = area;
                root.fixedW = root.fixedH = true;
                root.minW = root.maxW = area.width;
                root.minH = root.maxH = area.height;
                if (style)
                {
                    const RectOffset* p = style->padding();
                    root.padding[0] = p->left(), root.padding[1] = p->right(), root.padding[2] = p->top(), root.padding[3] = p->bottom();
                }
                cache->nodes.Push(Move(root));
            }
            else
            {
                for (Node& n : cache->nodes)
                    n.cursor = 0;
                if (!cache->nodes.Empty())
                {
                    // The area may have changed since Layout (a window dragged): only its place.
                    cache->nodes[0].fixed.x = area.x, cache->nodes[0].fixed.y = area.y;
                }
            }
            if (!cache->nodes.Empty())
                cache->stack.Push(0);
        }

        void EndLayout(LayoutCache* cache)
        {
            if (!cache)
                return;
            if (cache->recording)
            {
                if (cache->stack.Size() > 1)
                    HX_LOG_ERROR(LayoutLog, "GUI: a GuiLayout group was not ended.");
                Solve(*cache);
            }
            cache->stack.Clear();
            cache->recording = false;
            C().cache = nullptr;
        }

        Vector2 LayoutCacheMinSize(LayoutCache* cache)
        {
            if (!cache || cache->nodes.Empty())
                return Vector2(0, 0);
            // The content's smallest size inside the root's padding (the root itself is fixed to its area).
            Node root = cache->nodes[0];
            Node probe = root;
            probe.fixedW = probe.fixedH = false;
            probe.minW = probe.maxW = probe.minH = probe.maxH = 0;
            cache->nodes[0] = probe;
            CalcWidth(*cache, 0);
            CalcHeight(*cache, 0);
            Vector2 size(cache->nodes[0].minW, cache->nodes[0].minH);
            cache->nodes[0] = root;
            Solve(*cache);
            return size;
        }

        void ResetLayoutForTests()
        {
            C().cache = nullptr;
            S().layoutMismatchSaid = false;
        }
    }

    // ------------------------------------------------------------------------------------------------- GuiLayout

    namespace
    {
        GuiSkin& K() { return Skin(); }
        const GuiContent& Text(StringView t) { return GuiContent::Temp(t); }
        const GuiContent& Image(engine::Texture2D* i) { return GuiContent::Temp(StringView(), i); }
        const GuiContent& Of(GuiContent* c) { return c ? *c : *GuiContent::none(); }
    }

    void GuiLayout::Label(StringView text, Options options) { Label(&GuiContent::Temp(text), K().label(), options); }
    void GuiLayout::Label(engine::Texture2D* image, Options options) { Label(&GuiContent::Temp(StringView(), image), K().label(), options); }
    void GuiLayout::Label(GuiContent* content, Options options) { Label(content, K().label(), options); }
    void GuiLayout::Label(StringView text, GuiStyle* style, Options options) { Label(&GuiContent::Temp(text), style, options); }

    void GuiLayout::Label(GuiContent* content, GuiStyle* style, Options options)
    {
        GuiStyle& st = *Or(style, K().label());
        GuiContent c(content);
        Rect r = ContentRect(c, st, options);
        Gui::LabelContent(r, c, st);
    }

    void GuiLayout::Box(StringView text, Options options) { Box(&GuiContent::Temp(text), K().box(), options); }
    void GuiLayout::Box(GuiContent* content, Options options) { Box(content, K().box(), options); }
    void GuiLayout::Box(StringView text, GuiStyle* style, Options options) { Box(&GuiContent::Temp(text), style, options); }

    void GuiLayout::Box(GuiContent* content, GuiStyle* style, Options options)
    {
        GuiStyle& st = *Or(style, K().box());
        GuiContent c(content);
        Rect r = ContentRect(c, st, options);
        Gui::BoxContent(r, c, st);
    }

    namespace
    {
        bool LayoutButton(const GuiContent& content, GuiStyle& style, Options options, bool repeat)
        {
            GuiContent c(const_cast<GuiContent*>(&content));
            Rect r = ContentRect(c, style, options);
            return Gui::ButtonContent(r, c, style, repeat);
        }
    }

    bool GuiLayout::Button(StringView text, Options options) { return LayoutButton(Text(text), *K().button(), options, false); }
    bool GuiLayout::Button(engine::Texture2D* image, Options options) { return LayoutButton(Image(image), *K().button(), options, false); }
    bool GuiLayout::Button(GuiContent* content, Options options) { return LayoutButton(Of(content), *K().button(), options, false); }
    bool GuiLayout::Button(StringView text, GuiStyle* style, Options options) { return LayoutButton(Text(text), *Or(style, K().button()), options, false); }
    bool GuiLayout::Button(GuiContent* content, GuiStyle* style, Options options) { return LayoutButton(Of(content), *Or(style, K().button()), options, false); }
    bool GuiLayout::RepeatButton(StringView text, Options options) { return LayoutButton(Text(text), *K().button(), options, true); }
    bool GuiLayout::RepeatButton(GuiContent* content, Options options) { return LayoutButton(Of(content), *K().button(), options, true); }
    bool GuiLayout::RepeatButton(StringView text, GuiStyle* style, Options options) { return LayoutButton(Text(text), *Or(style, K().button()), options, true); }

    bool GuiLayout::Toggle(bool value, StringView text, Options options) { return Toggle(value, text, K().toggle(), options); }

    bool GuiLayout::Toggle(bool value, GuiContent* content, Options options)
    {
        GuiContent c(content);
        GuiStyle& st = *K().toggle();
        Rect r = ContentRect(c, st, options);
        return Gui::ToggleContent(r, value, c, st);
    }

    bool GuiLayout::Toggle(bool value, StringView text, GuiStyle* style, Options options)
    {
        GuiContent c(text);
        GuiStyle& st = *Or(style, K().toggle());
        Rect r = ContentRect(c, st, options);
        return Gui::ToggleContent(r, value, c, st);
    }

    namespace
    {
        i32 LayoutGrid(i32 selected, Span<GuiContent* const> contents, i32 xCount, Options options, bool toolbar, GuiStyle* style = nullptr,
                       Gui::ToolbarButtonSize buttonSize = Gui::ToolbarButtonSize::Fixed, Span<const bool> enabled = {})
        {
            GuiStyle& st = style ? *style : *K().button();
            // The cells' widest and tallest content, times the columns and rows.
            f32 w = 0, h = 0;
            for (GuiContent* c : contents)
            {
                Vector2 size = st.CalcSize(c);
                w = size.x > w ? size.x : w;
                h = size.y > h ? size.y : h;
            }
            i32 count = i32(contents.Size());
            i32 columns = toolbar ? count : (xCount > 0 ? xCount : 1);
            i32 rows = columns > 0 ? (count + columns - 1) / columns : 1;
            f32 spacing = toolbar ? 0.0f : f32(st.margin()->left() > st.margin()->right() ? st.margin()->left() : st.margin()->right());
            f32 tw = w * f32(columns) + spacing * f32(columns - 1), th = h * f32(rows) + spacing * f32(rows - 1);
            if (toolbar && buttonSize == Gui::ToolbarButtonSize::FitToContents)
            {
                tw = 0;
                for (GuiContent* c : contents)
                    tw += st.CalcSize(c).x;
            }
            Rect r = Leaf(tw, tw, th, th, &st, nullptr, options);
            return Gui::GridContent(r, selected, contents, xCount, &st, toolbar, buttonSize, enabled);
        }

        template <class F>
        i32 TextGrid(Span<const String> texts, F&& f)
        {
            std::unique_ptr<GuiContent[]> made(new GuiContent[texts.Size() > 0 ? texts.Size() : 1]);
            Array<GuiContent*> ptrs;
            for (usize i = 0; i < texts.Size(); i++)
            {
                made[i].SetText(texts[i].View());
                ptrs.Push(&made[i]);
            }
            return f(Span<GuiContent* const>(ptrs.Data(), ptrs.Size()));
        }
    }

    i32 GuiLayout::Toolbar(i32 selected, Span<const String> texts, Options options)
    {
        return TextGrid(texts, [&](Span<GuiContent* const> c) { return LayoutGrid(selected, c, 0, options, true); });
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<GuiContent* const> contents, Options options) { return LayoutGrid(selected, contents, 0, options, true); }
    i32 GuiLayout::Toolbar(i32 selected, Span<engine::Texture2D* const> images, Options options)
    {
        std::unique_ptr<GuiContent[]> made(new GuiContent[images.Size() > 0 ? images.Size() : 1]);
        Array<GuiContent*> ptrs;
        for (usize i = 0; i < images.Size(); i++)
        {
            made[i].SetImage(images[i]);
            ptrs.Push(&made[i]);
        }
        return LayoutGrid(selected, Span<GuiContent* const>(ptrs.Data(), ptrs.Size()), 0, options, true);
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<const String> texts, GuiStyle* style, Options options)
    {
        return TextGrid(texts, [&](Span<GuiContent* const> c) { return LayoutGrid(selected, c, 0, options, true, style); });
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<GuiContent* const> contents, GuiStyle* style, Options options)
    {
        return LayoutGrid(selected, contents, 0, options, true, style);
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<const String> texts, GuiStyle* style, Gui::ToolbarButtonSize buttonSize, Options options)
    {
        return TextGrid(texts, [&](Span<GuiContent* const> c) { return LayoutGrid(selected, c, 0, options, true, style, buttonSize); });
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<GuiContent* const> contents, GuiStyle* style, Gui::ToolbarButtonSize buttonSize, Options options)
    {
        return LayoutGrid(selected, contents, 0, options, true, style, buttonSize);
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<const String> texts, Span<const bool> enabled, GuiStyle* style, Gui::ToolbarButtonSize buttonSize,
                           Options options)
    {
        return TextGrid(texts, [&](Span<GuiContent* const> c) { return LayoutGrid(selected, c, 0, options, true, style, buttonSize, enabled); });
    }
    i32 GuiLayout::Toolbar(i32 selected, Span<GuiContent* const> contents, Span<const bool> enabled, GuiStyle* style, Gui::ToolbarButtonSize buttonSize,
                           Options options)
    {
        return LayoutGrid(selected, contents, 0, options, true, style, buttonSize, enabled);
    }
    i32 GuiLayout::SelectionGrid(i32 selected, Span<const String> texts, i32 xCount, Options options)
    {
        return TextGrid(texts, [&](Span<GuiContent* const> c) { return LayoutGrid(selected, c, xCount, options, false); });
    }
    i32 GuiLayout::SelectionGrid(i32 selected, Span<GuiContent* const> contents, i32 xCount, Options options)
    {
        return LayoutGrid(selected, contents, xCount, options, false);
    }

    namespace
    {
        String LayoutText(StringView text, i32 maxLength, GuiStyle& style, bool multiline, char16_t mask, Options options)
        {
            // A field is as tall as a line of its style; it stretches across.
            GuiContent c(text.Empty() ? StringView(" ") : text);
            Vector2 size = style.CalcSize(&c);
            f32 minW = size.x < 20 ? size.x : 20;
            Rect r = Leaf(minW, size.x, size.y, size.y, &style, multiline ? &c : nullptr, options);
            return Gui::TextContent(r, text, maxLength, style, multiline, mask);
        }
    }

    String GuiLayout::TextField(StringView text, Options options) { return LayoutText(text, -1, *K().textField(), false, 0, options); }
    String GuiLayout::TextField(StringView text, i32 maxLength, Options options) { return LayoutText(text, maxLength, *K().textField(), false, 0, options); }
    String GuiLayout::TextField(StringView text, GuiStyle* style, Options options) { return LayoutText(text, -1, *Or(style, K().textField()), false, 0, options); }
    String GuiLayout::PasswordField(StringView password, char16_t maskChar, Options options)
    {
        return LayoutText(password, -1, *K().textField(), false, maskChar ? maskChar : u'*', options);
    }
    String GuiLayout::TextArea(StringView text, Options options) { return LayoutText(text, -1, *K().textArea(), true, 0, options); }
    String GuiLayout::TextArea(StringView text, i32 maxLength, Options options) { return LayoutText(text, maxLength, *K().textArea(), true, 0, options); }

    namespace
    {
        f32 LayoutSlider(f32 value, f32 size, f32 a, f32 b, bool horiz, bool scrollbar, Options options)
        {
            GuiSkin& k = K();
            GuiStyle& st = scrollbar ? *(horiz ? k.horizontalScrollbar() : k.verticalScrollbar()) : *(horiz ? k.horizontalSlider() : k.verticalSlider());
            GuiStyle& thumb = scrollbar ? *(horiz ? k.horizontalScrollbarThumb() : k.verticalScrollbarThumb())
                                        : *(horiz ? k.horizontalSliderThumb() : k.verticalSliderThumb());
            // As long as the thumb at least; across, the style's fixed size or the thumb's.
            f32 tw = thumb.fixedWidth() != 0 ? thumb.fixedWidth() : 10, th = thumb.fixedHeight() != 0 ? thumb.fixedHeight() : 10;
            Rect r = horiz ? Leaf(tw, tw + 10, st.fixedHeight() != 0 ? st.fixedHeight() : th, st.fixedHeight() != 0 ? st.fixedHeight() : th, &st, nullptr, options)
                           : Leaf(st.fixedWidth() != 0 ? st.fixedWidth() : tw, st.fixedWidth() != 0 ? st.fixedWidth() : tw, th, th + 10, &st, nullptr, options);
            if (scrollbar)
                return horiz ? Gui::HorizontalScrollbar(r, value, size, a, b, &st) : Gui::VerticalScrollbar(r, value, size, a, b, &st);
            return Gui::Slider(r, value, 0, a, b, &st, &thumb, horiz, 0);
        }
    }

    f32 GuiLayout::HorizontalSlider(f32 value, f32 leftValue, f32 rightValue, Options options)
    {
        return LayoutSlider(value, 0, leftValue, rightValue, true, false, options);
    }
    f32 GuiLayout::VerticalSlider(f32 value, f32 topValue, f32 bottomValue, Options options)
    {
        return LayoutSlider(value, 0, topValue, bottomValue, false, false, options);
    }
    f32 GuiLayout::HorizontalScrollbar(f32 value, f32 size, f32 leftValue, f32 rightValue, Options options)
    {
        return LayoutSlider(value, size, leftValue, rightValue, true, true, options);
    }
    f32 GuiLayout::VerticalScrollbar(f32 value, f32 size, f32 topValue, f32 bottomValue, Options options)
    {
        return LayoutSlider(value, size, topValue, bottomValue, false, true, options);
    }

    void GuiLayout::Space(f32 pixels)
    {
        LayoutCache* c = Cache();
        if (!c || c->stack.Empty())
            return;
        bool vertical = Top(*c).vertical;
        Node* n = Take(Node::Space);
        if (n && c->recording)
        {
            if (vertical)
                n->minH = n->maxH = pixels;
            else
                n->minW = n->maxW = pixels;
        }
    }

    void GuiLayout::FlexibleSpace()
    {
        LayoutCache* c = Cache();
        if (!c || c->stack.Empty())
            return;
        bool vertical = Top(*c).vertical;
        Node* n = Take(Node::Flexible);
        // It takes the room left over (a weight far above the entries' own).
        if (n && c->recording)
        {
            n->stretchW = vertical ? 0.0f : 10000.0f;
            n->stretchH = vertical ? 10000.0f : 0.0f;
        }
    }

    void GuiLayout::BeginHorizontal(Options options) { BeginGroup(false, *GuiContent::none(), nullptr, options); }
    void GuiLayout::BeginHorizontal(GuiStyle* style, Options options) { BeginGroup(false, *GuiContent::none(), style, options); }
    void GuiLayout::BeginHorizontal(StringView text, GuiStyle* style, Options options) { BeginGroup(false, GuiContent::Temp(text), style, options); }
    void GuiLayout::EndHorizontal() { EndGroup(); }
    void GuiLayout::BeginVertical(Options options) { BeginGroup(true, *GuiContent::none(), nullptr, options); }
    void GuiLayout::BeginVertical(GuiStyle* style, Options options) { BeginGroup(true, *GuiContent::none(), style, options); }
    void GuiLayout::BeginVertical(StringView text, GuiStyle* style, Options options) { BeginGroup(true, GuiContent::Temp(text), style, options); }
    void GuiLayout::EndVertical() { EndGroup(); }

    void GuiLayout::BeginArea(Rect screenRect) { BeginArea(screenRect, StringView(), GuiStyle::none()); }
    void GuiLayout::BeginArea(Rect screenRect, StringView text) { BeginArea(screenRect, text, GuiStyle::none()); }
    void GuiLayout::BeginArea(Rect screenRect, GuiStyle* style) { BeginArea(screenRect, StringView(), style); }

    void GuiLayout::BeginArea(Rect screenRect, StringView text, GuiStyle* style)
    {
        Node* n = Take(Node::Group);
        if (!n)
            return;
        LayoutCache& c = *C().cache;
        u32 index = u32(n - c.nodes.Data());
        const GuiStyle* st = style ? style : GuiStyle::none();
        if (c.recording)
        {
            n->vertical = true;
            n->area = true;
            n->fixed = screenRect;
            n->fixedW = n->fixedH = true;
            n->minW = n->maxW = screenRect.width;
            n->minH = n->maxH = screenRect.height;
            const RectOffset* p = st->padding();
            n->padding[0] = p->left(), n->padding[1] = p->right(), n->padding[2] = p->top(), n->padding[3] = p->bottom();
        }
        else
            n->cursor = 0;
        // In every pass, Layout too (its control id as in the others).
        Gui::BeginGroup(screenRect, &GuiContent::Temp(text), const_cast<GuiStyle*>(st));
        c.stack.Push(index);
    }

    void GuiLayout::EndArea()
    {
        LayoutCache* c = Cache();
        if (!c)
            return;
        EndGroup();
        Gui::EndGroup();
    }

    Vector2 GuiLayout::BeginScrollView(Vector2 scrollPosition, Options options) { return BeginScrollView(scrollPosition, false, false, nullptr, nullptr, options); }
    Vector2 GuiLayout::BeginScrollView(Vector2 scrollPosition, bool alwaysShowHorizontal, bool alwaysShowVertical, Options options)
    {
        return BeginScrollView(scrollPosition, alwaysShowHorizontal, alwaysShowVertical, nullptr, nullptr, options);
    }
    Vector2 GuiLayout::BeginScrollView(Vector2 scrollPosition, GuiStyle* horizontalScrollbar, GuiStyle* verticalScrollbar, Options options)
    {
        return BeginScrollView(scrollPosition, false, false, horizontalScrollbar, verticalScrollbar, options);
    }
    Vector2 GuiLayout::BeginScrollView(Vector2 scrollPosition, GuiStyle* style, Options options)
    {
        (void)style;
        return BeginScrollView(scrollPosition, false, false, nullptr, nullptr, options);
    }

    // The full form (the others go to it).
    Vector2 GuiLayout::BeginScrollView(Vector2 scrollPosition, bool alwaysShowHorizontal, bool alwaysShowVertical, GuiStyle* horizontalScrollbar,
                                       GuiStyle* verticalScrollbar, Options options)
    {
        Node* n = Take(Node::Group);
        if (!n)
            return scrollPosition;
        LayoutCache& c = *C().cache;
        u32 index = u32(n - c.nodes.Data());
        GuiSkin& k = K();
        if (c.recording)
        {
            n->scroll = true;
            n->stretchW = n->stretchH = 1;
            n->alwaysH = alwaysShowHorizontal, n->alwaysV = alwaysShowVertical;
            n->hbar = Or(horizontalScrollbar, k.horizontalScrollbar());
            n->vbar = Or(verticalScrollbar, k.verticalScrollbar());
            Apply(*n, options);
            c.stack.Push(index);
            // Its content: a vertical group in the view's space.
            Node content;
            content.kind = Node::Group;
            content.vertical = true;
            content.stretchW = 1;
            u32 ci = u32(c.nodes.Size());
            c.nodes.Push(Move(content));
            c.nodes[index].children.Push(ci);
            c.stack.Push(ci);
            // The view's controls ask for their ids as in the other passes.
            Gui::BeginScrollView(Rect(0, 0, 1, 1), scrollPosition, Rect(0, 0, 0, 0), alwaysShowHorizontal, alwaysShowVertical,
                                 Or(horizontalScrollbar, k.horizontalScrollbar()), Or(verticalScrollbar, k.verticalScrollbar()));
            return scrollPosition;
        }
        n->cursor = 0;
        Rect view(0, 0, 0, 0);
        if (!n->children.Empty())
        {
            Node& content = c.nodes[n->children[0]];
            content.cursor = 0;
            view = Rect(0, 0, content.rect.width + f32(content.margin[0] + content.margin[1]), content.rect.height + f32(content.margin[2] + content.margin[3]));
        }
        Vector2 result = Gui::BeginScrollView(n->rect, scrollPosition, view, alwaysShowHorizontal, alwaysShowVertical, Or(horizontalScrollbar, k.horizontalScrollbar()),
                                              Or(verticalScrollbar, k.verticalScrollbar()));
        c.stack.Push(index);
        if (!c.nodes[index].children.Empty())
            c.stack.Push(c.nodes[index].children[0]);
        return result;
    }

    void GuiLayout::EndScrollView()
    {
        LayoutCache* c = Cache();
        if (!c || c->stack.Size() < 3)
        {
            HX_LOG_ERROR(LayoutLog, "GUI: GuiLayout.EndScrollView without its Begin.");
            return;
        }
        bool recording = c->recording;
        c->stack.Pop();
        c->lastRect = recording ? Rect(0, 0, 1, 1) : Top(*c).rect;
        c->stack.Pop();
        Gui::EndScrollView(true);
    }

    Rect GuiLayout::WindowInternal(i32 id, Rect screenRect, GuiContent* title, GuiStyle* style, Options options)
    {
        // The options size the window (its content grows it in Layout).
        for (GuiLayoutOption* o : options)
        {
            if (!o)
                continue;
            if (o->kind == GuiLayoutOption::Kind::FixedWidth || (o->kind == GuiLayoutOption::Kind::MinWidth && screenRect.width < o->value))
                screenRect.width = o->value;
            if (o->kind == GuiLayoutOption::Kind::FixedHeight || (o->kind == GuiLayoutOption::Kind::MinHeight && screenRect.height < o->value))
                screenRect.height = o->value;
        }
        Rect r = Gui::WindowInternal(id, screenRect, title, style, false);
        State& s = S();
        if (Window* w = FindWindow(s.owner ? s.owner->key : 0, id))
        {
            w->layout = true;
            if (!w->cache)
                w->cache = NewLayoutCache();
        }
        return r;
    }

    GuiLayoutOption* GuiLayout::Width(f32 width) { return new GuiLayoutOption(GuiLayoutOption::Kind::FixedWidth, width); }
    GuiLayoutOption* GuiLayout::Height(f32 height) { return new GuiLayoutOption(GuiLayoutOption::Kind::FixedHeight, height); }
    GuiLayoutOption* GuiLayout::MinWidth(f32 minWidth) { return new GuiLayoutOption(GuiLayoutOption::Kind::MinWidth, minWidth); }
    GuiLayoutOption* GuiLayout::MaxWidth(f32 maxWidth) { return new GuiLayoutOption(GuiLayoutOption::Kind::MaxWidth, maxWidth); }
    GuiLayoutOption* GuiLayout::MinHeight(f32 minHeight) { return new GuiLayoutOption(GuiLayoutOption::Kind::MinHeight, minHeight); }
    GuiLayoutOption* GuiLayout::MaxHeight(f32 maxHeight) { return new GuiLayoutOption(GuiLayoutOption::Kind::MaxHeight, maxHeight); }
    GuiLayoutOption* GuiLayout::ExpandWidth(bool expand) { return new GuiLayoutOption(GuiLayoutOption::Kind::StretchWidth, expand ? 1.0f : 0.0f); }
    GuiLayoutOption* GuiLayout::ExpandHeight(bool expand) { return new GuiLayoutOption(GuiLayoutOption::Kind::StretchHeight, expand ? 1.0f : 0.0f); }

    // ------------------------------------------------------------------------------------------------- GuiLayoutUtility

    Rect GuiLayoutUtility::GetRect(f32 width, f32 height, Options options) { return Leaf(width, width, height, height, GuiStyle::none(), nullptr, options); }
    Rect GuiLayoutUtility::GetRect(f32 width, f32 height, GuiStyle* style, Options options)
    {
        return Leaf(width, width, height, height, style ? style : GuiStyle::none(), nullptr, options);
    }
    Rect GuiLayoutUtility::GetRect(f32 minWidth, f32 maxWidth, f32 minHeight, f32 maxHeight, Options options)
    {
        return Leaf(minWidth, maxWidth, minHeight, maxHeight, GuiStyle::none(), nullptr, options);
    }
    Rect GuiLayoutUtility::GetRect(GuiContent* content, GuiStyle* style, Options options)
    {
        return ContentRect(Of(content), style ? *style : *GuiStyle::none(), options);
    }

    Rect GuiLayoutUtility::GetLastRect()
    {
        LayoutCache* c = C().cache;
        return c ? c->lastRect : Rect(0, 0, 1, 1);
    }

    Rect GuiLayoutUtility::GetAspectRect(f32 aspect, Options options) { return Leaf(0, 0, 0, 0, GuiStyle::none(), nullptr, options, aspect > 0 ? aspect : 1); }
}
