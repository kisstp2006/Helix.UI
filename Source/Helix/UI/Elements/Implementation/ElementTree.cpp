#include <Helix/UI/Elements/ElementTree.h>

namespace helix::ui
{
    ElementHandle ElementTree::Create(Atom type)
    {
        u32 i;
        if (!free_.Empty())
        {
            i = free_.Back();
            free_.Pop();
        }
        else
        {
            i = u32(generation_.Size());
            generation_.Push(0);
            live_.Push(0);
            parent_.Push(kNone), firstChild_.Push(kNone), lastChild_.Push(kNone), next_.Push(kNone), previous_.Push(kNone);
            childCount_.Push(0);
            type_.Push(kNoAtom), name_.Push(kNoAtom);
            text_.Emplace();
            classes_.Emplace();
            states_.Push(PseudoStates::None);
            dirty_.Push(Dirty::None);
        }
        live_[i] = 1;
        parent_[i] = firstChild_[i] = lastChild_[i] = next_[i] = previous_[i] = kNone;
        childCount_[i] = 0;
        type_[i] = type;
        name_[i] = kNoAtom;
        text_[i].Clear();
        classes_[i].Clear();
        states_[i] = PseudoStates::None;
        dirty_[i] = Dirty::Style | Dirty::Layout;
        alive_++;
        version_++;
        return {i, generation_[i]};
    }

    bool ElementTree::IsAlive(ElementHandle e) const
    {
        return e.index < generation_.Size() && live_[e.index] && generation_[e.index] == e.generation;
    }

    ElementHandle ElementTree::HandleAt(u32 index) const
    {
        return index < generation_.Size() && live_[index] ? ElementHandle{index, generation_[index]} : ElementHandle{};
    }

    void ElementTree::Destroy(ElementHandle e)
    {
        if (!IsAlive(e))
            return;
        Detach(e);
        // The subtree's slots, children before their parent is freed.
        Array<u32> slots;
        ForEachInSubtree(e, [&](ElementHandle h, u32) { slots.Push(h.index); });
        for (u32 i : slots)
        {
            live_[i] = 0;
            generation_[i]++;
            classes_[i].Clear();
            free_.Push(i);
            alive_--;
        }
        version_++;
    }

    void ElementTree::Unlink(u32 child)
    {
        u32 p = parent_[child];
        if (p == kNone)
            return;
        if (previous_[child] != kNone)
            next_[previous_[child]] = next_[child];
        else
            firstChild_[p] = next_[child];
        if (next_[child] != kNone)
            previous_[next_[child]] = previous_[child];
        else
            lastChild_[p] = previous_[child];
        parent_[child] = next_[child] = previous_[child] = kNone;
        childCount_[p]--;
    }

    void ElementTree::LinkBefore(u32 parent, u32 child, u32 before)
    {
        parent_[child] = parent;
        if (before == kNone)
        {
            previous_[child] = lastChild_[parent];
            next_[child] = kNone;
            if (lastChild_[parent] != kNone)
                next_[lastChild_[parent]] = child;
            else
                firstChild_[parent] = child;
            lastChild_[parent] = child;
        }
        else
        {
            previous_[child] = previous_[before];
            next_[child] = before;
            if (previous_[before] != kNone)
                next_[previous_[before]] = child;
            else
                firstChild_[parent] = child;
            previous_[before] = child;
        }
        childCount_[parent]++;
    }

    bool ElementTree::IsAncestor(ElementHandle ancestor, ElementHandle e) const
    {
        if (!IsAlive(ancestor) || !IsAlive(e))
            return false;
        for (u32 at = parent_[e.index]; at != kNone; at = parent_[at])
            if (at == ancestor.index)
                return true;
        return false;
    }

    bool ElementTree::Append(ElementHandle parent, ElementHandle child) { return Insert(parent, ~0u, child); }

    bool ElementTree::Insert(ElementHandle parent, u32 index, ElementHandle child)
    {
        if (!IsAlive(parent) || !IsAlive(child) || parent == child || IsAncestor(child, parent))
            return false;
        u32 old = parent_[child.index];
        if (old != kNone)
        {
            MarkLayoutDirty(HandleAt(old));
            Unlink(child.index);
        }
        u32 before = kNone;
        if (index < childCount_[parent.index])
        {
            before = firstChild_[parent.index];
            for (u32 k = 0; k < index; k++)
                before = next_[before];
        }
        LinkBefore(parent.index, child.index, before);
        version_++;
        MarkStyleDirty(child);
        MarkLayoutDirty(child);
        return true;
    }

    void ElementTree::Detach(ElementHandle child)
    {
        if (!IsAlive(child) || parent_[child.index] == kNone)
            return;
        MarkLayoutDirty(HandleAt(parent_[child.index]));
        Unlink(child.index);
        version_++;
        MarkStyleDirty(child);
    }

    ElementHandle ElementTree::ChildAt(ElementHandle e, u32 index) const
    {
        if (!IsAlive(e) || index >= childCount_[e.index])
            return {};
        u32 at = firstChild_[e.index];
        for (u32 k = 0; k < index; k++)
            at = next_[at];
        return {at, generation_[at]};
    }

    u32 ElementTree::IndexInParent(ElementHandle e) const
    {
        if (!IsAlive(e) || parent_[e.index] == kNone)
            return ~0u;
        u32 k = 0;
        for (u32 at = previous_[e.index]; at != kNone; at = previous_[at])
            k++;
        return k;
    }

    void ElementTree::SetText(ElementHandle e, StringView text)
    {
        if (!IsAlive(e) || text_[e.index].View() == text)
            return;
        text_[e.index] = String(text);
        textVersion_++;
        MarkLayoutDirty(e);
    }

    void ElementTree::SetName(ElementHandle e, Atom name)
    {
        if (!IsAlive(e) || name_[e.index] == name)
            return;
        name_[e.index] = name;
        MarkStyleDirty(e);
    }

    Span<const Atom> ElementTree::Classes(ElementHandle e) const
    {
        if (!IsAlive(e))
            return {};
        const Array<Atom>& c = classes_[e.index];
        return Span<const Atom>(c.Data(), c.Size());
    }

    bool ElementTree::HasClass(ElementHandle e, Atom c) const { return IsAlive(e) && classes_[e.index].Contains(c); }

    void ElementTree::AddClass(ElementHandle e, Atom c)
    {
        if (!IsAlive(e) || c == kNoAtom || classes_[e.index].Contains(c))
            return;
        classes_[e.index].Push(c);
        MarkStyleDirty(e);
    }

    void ElementTree::RemoveClass(ElementHandle e, Atom c)
    {
        if (!IsAlive(e))
            return;
        Array<Atom>& list = classes_[e.index];
        for (usize k = 0; k < list.Size(); k++)
            if (list[k] == c)
            {
                list.RemoveAt(k);
                MarkStyleDirty(e);
                return;
            }
    }

    void ElementTree::ClearClasses(ElementHandle e)
    {
        if (!IsAlive(e) || classes_[e.index].Empty())
            return;
        classes_[e.index].Clear();
        MarkStyleDirty(e);
    }

    void ElementTree::SetStates(ElementHandle e, PseudoStates states)
    {
        if (!IsAlive(e) || states_[e.index] == states)
            return;
        states_[e.index] = states;
        MarkStyleDirty(e);
    }

    void ElementTree::MarkStyleDirty(ElementHandle e)
    {
        if (!IsAlive(e))
            return;
        dirty_[e.index] = dirty_[e.index] | Dirty::Style;
        for (u32 at = parent_[e.index]; at != kNone && !Any(dirty_[at] & Dirty::DescendantStyle); at = parent_[at])
            dirty_[at] = dirty_[at] | Dirty::DescendantStyle;
    }

    void ElementTree::MarkLayoutDirty(ElementHandle e)
    {
        if (!IsAlive(e))
            return;
        for (u32 at = e.index; at != kNone; at = parent_[at])
        {
            bool was = Any(dirty_[at] & Dirty::Layout);
            dirty_[at] = dirty_[at] | Dirty::Layout;
            if (was && at != e.index)
                return;
        }
    }

    void ElementTree::ClearDirty(ElementHandle e, Dirty flags)
    {
        if (IsAlive(e))
            dirty_[e.index] = dirty_[e.index] & ~flags;
    }
}
