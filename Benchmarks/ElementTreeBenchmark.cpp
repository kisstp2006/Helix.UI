#include <Helix/UI/Elements/ElementTree.h>
#include <Helix/Testing/Bench.h>

using namespace helix;
using namespace helix::ui;

// A tree of 10k elements (100 rows of 99 children) made, walked in tree order and destroyed.
HX_BENCH(ElementTreeBuildWalkDestroy10k)
{
    Atom type = Intern("Element");
    state.SetItemsPerIteration(10000);
    u32 visited = 0;
    while (state.KeepRunning())
    {
        ElementTree tree;
        ElementHandle root = tree.Create(type);
        for (u32 r = 0; r < 100; r++)
        {
            ElementHandle row = tree.Create(type);
            tree.Append(root, row);
            for (u32 c = 0; c < 99; c++)
                tree.Append(row, tree.Create(type));
        }
        visited = 0;
        tree.ForEachInSubtree(root, [&](ElementHandle, u32) { visited++; });
        tree.Destroy(root);
    }
    HX_ASSERT(visited == 10001);
}
