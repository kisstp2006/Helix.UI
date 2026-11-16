#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/UI/Core/Atoms.h>

// The element tree of a UI: elements by stable handles, their structure in dense arrays indexed by the handle's slot
// (parent, first and last child, siblings), their type, name, classes and pseudo states, and what of them must be
// computed again (styles, layout). The systems that compute styles and layout keep their data in arrays of the same
// slots.
namespace helix::ui
{
    struct ElementHandle
    {
        u32 index = ~0u;
        u32 generation = 0;

        bool IsValid() const { return index != ~0u; }
        bool operator==(const ElementHandle& other) const { return index == other.index && generation == other.generation; }
    };

    // Pseudo states a selector may ask for.
    enum class PseudoStates : u32
    {
        None = 0,
        Hover = 1u << 0,
        Active = 1u << 1,
        Focus = 1u << 2,
        Checked = 1u << 3,
        Disabled = 1u << 4,
        Root = 1u << 5,
    };
    inline PseudoStates operator|(PseudoStates a, PseudoStates b) { return PseudoStates(u32(a) | u32(b)); }
    inline PseudoStates operator&(PseudoStates a, PseudoStates b) { return PseudoStates(u32(a) & u32(b)); }
    inline PseudoStates operator~(PseudoStates a) { return PseudoStates(~u32(a)); }
    inline bool Any(PseudoStates s) { return u32(s) != 0; }

    // What must be computed again.
    enum class Dirty : u8
    {
        None = 0,
        // The element's style and its descendants'.
        Style = 1u << 0,
        // A descendant's style.
        DescendantStyle = 1u << 1,
        // The element's layout and so its ancestors'.
        Layout = 1u << 2,
    };
    inline Dirty operator|(Dirty a, Dirty b) { return Dirty(u8(a) | u8(b)); }
    inline Dirty operator&(Dirty a, Dirty b) { return Dirty(u8(a) & u8(b)); }
    inline Dirty operator~(Dirty a) { return Dirty(u8(~u8(a))); }
    inline bool Any(Dirty d) { return u8(d) != 0; }

    class ElementTree
    {
    public:
        ElementHandle Create(Atom type);
        // Destroys the element and its descendants (taken from its parent first).
        void Destroy(ElementHandle element);
        bool IsAlive(ElementHandle element) const;

        // A child goes to the end (or to index) of its new parent's children, taken from its old parent first. An
        // element cannot become a child of itself or of its descendants: false then.
        bool Append(ElementHandle parent, ElementHandle child);
        bool Insert(ElementHandle parent, u32 index, ElementHandle child);
        void Detach(ElementHandle child);

        ElementHandle Parent(ElementHandle e) const { return Link(parent_, e); }
        ElementHandle FirstChild(ElementHandle e) const { return Link(firstChild_, e); }
        ElementHandle LastChild(ElementHandle e) const { return Link(lastChild_, e); }
        ElementHandle NextSibling(ElementHandle e) const { return Link(next_, e); }
        ElementHandle PreviousSibling(ElementHandle e) const { return Link(previous_, e); }
        u32 ChildCount(ElementHandle e) const { return IsAlive(e) ? childCount_[e.index] : 0; }
        ElementHandle ChildAt(ElementHandle e, u32 index) const;
        // The position among the parent's children (~0u without a parent).
        u32 IndexInParent(ElementHandle e) const;
        bool IsAncestor(ElementHandle ancestor, ElementHandle e) const;

        Atom Type(ElementHandle e) const { return IsAlive(e) ? type_[e.index] : kNoAtom; }
        Atom Name(ElementHandle e) const { return IsAlive(e) ? name_[e.index] : kNoAtom; }
        void SetName(ElementHandle e, Atom name);
        Span<const Atom> Classes(ElementHandle e) const;
        bool HasClass(ElementHandle e, Atom c) const;
        void AddClass(ElementHandle e, Atom c);
        void RemoveClass(ElementHandle e, Atom c);
        void ClearClasses(ElementHandle e);
        PseudoStates States(ElementHandle e) const { return IsAlive(e) ? states_[e.index] : PseudoStates::None; }
        void SetStates(ElementHandle e, PseudoStates states);

        Dirty DirtyFlags(ElementHandle e) const { return IsAlive(e) ? dirty_[e.index] : Dirty::None; }
        // The element's style and its subtree's (its ancestors learn that a descendant changed).
        void MarkStyleDirty(ElementHandle e);
        // The element's layout and its ancestors'.
        void MarkLayoutDirty(ElementHandle e);
        void ClearDirty(ElementHandle e, Dirty flags);

        // The number of slots (alive or free) the per element arrays of other systems must have.
        u32 Capacity() const { return u32(generation_.Size()); }
        // The handle of a slot (invalid when the slot is free).
        ElementHandle HandleAt(u32 index) const;
        // Changes with every change of the structure.
        u32 Version() const { return version_; }
        u32 Count() const { return alive_; }

        // The elements of a subtree in tree order (the element first), with their depth below it.
        template <typename F>
        void ForEachInSubtree(ElementHandle root, F&& f) const
        {
            if (!IsAlive(root))
                return;
            u32 depth = 0;
            u32 at = root.index;
            for (;;)
            {
                f(ElementHandle{at, generation_[at]}, depth);
                if (firstChild_[at] != kNone)
                {
                    at = firstChild_[at];
                    depth++;
                    continue;
                }
                while (at != root.index && next_[at] == kNone)
                {
                    at = parent_[at];
                    depth--;
                }
                if (at == root.index)
                    return;
                at = next_[at];
            }
        }

    private:
        static constexpr u32 kNone = ~0u;

        ElementHandle Link(const Array<u32>& links, ElementHandle e) const
        {
            if (!IsAlive(e))
                return {};
            u32 i = links[e.index];
            return i == kNone ? ElementHandle{} : ElementHandle{i, generation_[i]};
        }
        void Unlink(u32 child);
        void LinkBefore(u32 parent, u32 child, u32 before);

        Array<u32> generation_;
        Array<u8> live_;
        Array<u32> parent_, firstChild_, lastChild_, next_, previous_, childCount_;
        Array<Atom> type_, name_;
        Array<Array<Atom>> classes_;
        Array<PseudoStates> states_;
        Array<Dirty> dirty_;
        Array<u32> free_;
        u32 version_ = 0;
        u32 alive_ = 0;
    };
}
