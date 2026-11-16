#include <Helix/UI/Elements/ElementTree.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::ui;

HX_TEST(Atoms, InternedNamesCompareAsNumbers)
{
    Atom a = Intern("button");
    HX_CHECK(a != kNoAtom && Intern("button") == a && FindAtom("button") == a && AtomName(a) == StringView("button"));
    HX_CHECK(Intern("") == kNoAtom && FindAtom("never-interned-name") == kNoAtom && AtomName(kNoAtom).Empty());
    HX_CHECK(Intern("label") != a);
}

HX_TEST(ElementTree, ChildrenInOrderAndHandlesOfDestroyedElements)
{
    ElementTree tree;
    Atom type = Intern("Element");
    ElementHandle root = tree.Create(type);
    ElementHandle a = tree.Create(type), b = tree.Create(type), c = tree.Create(type);
    HX_CHECK(tree.Append(root, a) && tree.Append(root, c) && tree.Insert(root, 1, b));
    HX_CHECK(tree.ChildCount(root) == 3 && tree.ChildAt(root, 0) == a && tree.ChildAt(root, 1) == b && tree.ChildAt(root, 2) == c);
    HX_CHECK(tree.FirstChild(root) == a && tree.LastChild(root) == c && tree.NextSibling(a) == b && tree.PreviousSibling(c) == b);
    HX_CHECK(tree.Parent(b) == root && tree.IndexInParent(c) == 2 && tree.IndexInParent(root) == ~0u);

    // Moving: taken from the old place first; an element never under itself.
    ElementHandle d = tree.Create(type);
    HX_CHECK(tree.Append(b, d));
    HX_CHECK(!tree.Append(d, b) && !tree.Append(b, b) && !tree.Append(d, root));
    HX_CHECK(tree.Insert(root, 0, d) && tree.ChildCount(b) == 0 && tree.ChildAt(root, 0) == d && tree.ChildCount(root) == 4);
    tree.Detach(a);
    HX_CHECK(!tree.Parent(a).IsValid() && tree.ChildCount(root) == 3 && tree.FirstChild(root) == d);

    // Destroying a subtree frees its slots; old handles stay dead when a slot is used again.
    HX_CHECK(tree.Append(c, a));
    tree.Destroy(c);
    HX_CHECK(!tree.IsAlive(c) && !tree.IsAlive(a) && tree.ChildCount(root) == 2 && tree.Count() == 3);
    ElementHandle e = tree.Create(type);
    HX_CHECK(tree.IsAlive(e) && !tree.IsAlive(a) && !tree.IsAlive(c) && (e.index == a.index || e.index == c.index));
    HX_CHECK(!tree.Append(root, c) && tree.Type(c) == kNoAtom && tree.ChildCount(c) == 0);
}

HX_TEST(ElementTree, SubtreesInTreeOrderWithTheirDepth)
{
    ElementTree tree;
    Atom type = Intern("Element");
    ElementHandle root = tree.Create(type), a = tree.Create(type), a1 = tree.Create(type), a2 = tree.Create(type), b = tree.Create(type),
                  b1 = tree.Create(type);
    tree.Append(root, a), tree.Append(a, a1), tree.Append(a, a2), tree.Append(root, b), tree.Append(b, b1);
    Array<ElementHandle> order;
    Array<u32> depths;
    tree.ForEachInSubtree(root, [&](ElementHandle h, u32 depth) { order.Push(h), depths.Push(depth); });
    const ElementHandle want[] = {root, a, a1, a2, b, b1};
    const u32 wantDepth[] = {0, 1, 2, 2, 1, 2};
    HX_REQUIRE(order.Size() == 6);
    for (u32 i = 0; i < 6; i++)
        HX_CHECK(order[i] == want[i] && depths[i] == wantDepth[i]);
    // A subtree whose root has siblings stops at its root.
    order.Clear();
    tree.ForEachInSubtree(a, [&](ElementHandle h, u32) { order.Push(h); });
    HX_CHECK(order.Size() == 3 && order[2] == a2);
}

HX_TEST(ElementTree, NamesClassesStatesAndWhatMustBeComputedAgain)
{
    ElementTree tree;
    Atom type = Intern("Element");
    ElementHandle root = tree.Create(type), a = tree.Create(type), b = tree.Create(type);
    tree.Append(root, a), tree.Append(a, b);
    for (ElementHandle h : {root, a, b})
        tree.ClearDirty(h, Dirty::Style | Dirty::DescendantStyle | Dirty::Layout);

    Atom big = Intern("big"), red = Intern("red");
    tree.AddClass(b, big), tree.AddClass(b, red), tree.AddClass(b, big);
    HX_CHECK(tree.Classes(b).Size() == 2 && tree.HasClass(b, red));
    HX_CHECK(Any(tree.DirtyFlags(b) & Dirty::Style) && Any(tree.DirtyFlags(a) & Dirty::DescendantStyle) &&
             Any(tree.DirtyFlags(root) & Dirty::DescendantStyle) && !Any(tree.DirtyFlags(a) & Dirty::Style));
    tree.RemoveClass(b, big);
    HX_CHECK(tree.Classes(b).Size() == 1 && !tree.HasClass(b, big));

    tree.SetName(a, Intern("panel"));
    tree.SetStates(a, PseudoStates::Hover | PseudoStates::Focus);
    HX_CHECK(tree.Name(a) == FindAtom("panel") && Any(tree.States(a) & PseudoStates::Focus) && !Any(tree.States(a) & PseudoStates::Active));

    // Layout: the element and its ancestors.
    tree.MarkLayoutDirty(b);
    HX_CHECK(Any(tree.DirtyFlags(b) & Dirty::Layout) && Any(tree.DirtyFlags(a) & Dirty::Layout) && Any(tree.DirtyFlags(root) & Dirty::Layout));
}
