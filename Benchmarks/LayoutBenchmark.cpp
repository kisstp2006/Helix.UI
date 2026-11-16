#include <Helix/Jobs/Jobs.h>
#include <Helix/UI/Layout/Layout.h>
#include <Helix/Testing/Bench.h>

#include <chrono>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

namespace
{
    // 10k elements: a column of 100 rows, each a wrapping row of 99 items with margins, paddings, flexible widths.
    struct Page
    {
        ElementTree tree;
        LayoutEngine layout{tree};
        ElementHandle root;
        Array<ElementHandle> items;

        Page()
        {
            Atom type = Intern("Element");
            root = tree.Create(type);
            LayoutStyle rowStyle;
            rowStyle.direction = FlexDirection::Row;
            rowStyle.wrap = FlexWrap::Wrap;
            for (auto& p : rowStyle.padding)
                p = Length::Px(4);
            LayoutStyle item;
            item.basis = Length::Px(40);
            item.grow = 1;
            item.height = Length::Px(18);
            for (auto& m : item.margin)
                m = Length::Px(1);
            for (u32 r = 0; r < 100; r++)
            {
                ElementHandle row = tree.Create(type);
                tree.Append(root, row);
                layout.SetStyle(row, rowStyle);
                for (u32 c = 0; c < 99; c++)
                {
                    ElementHandle e = tree.Create(type);
                    tree.Append(row, e);
                    layout.SetStyle(e, item);
                    items.Push(e);
                }
            }
        }
    };

    f64 Ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b)
    {
        return std::chrono::duration<f64, std::milli>(b - a).count();
    }
}

// Budget (M8): 10k elements laid out in full under 1 ms. A full layout (every element dirty, the window resized) and an
// incremental one (one item changed).
HX_BENCH(Layout10kElements)
{
    if (!jobs::IsInitialized())
        jobs::Initialize();
    Page page;
    page.layout.Update(page.root, 1600, 4000);
    state.SetItemsPerIteration(10000);
    f64 full = 0, incremental = 0;
    u32 rounds = 0;
    f32 width = 1600;
    while (state.KeepRunning())
    {
        // A new width: every element is laid out again.
        width = width == 1600.0f ? 1601.0f : 1600.0f;
        auto t0 = std::chrono::steady_clock::now();
        page.layout.Update(page.root, width, 4000);
        auto t1 = std::chrono::steady_clock::now();
        LayoutStyle changed = page.layout.Style(page.items[rounds % page.items.Size()]);
        changed.basis = Length::Px(changed.basis.value == 40 ? 41.0f : 40.0f);
        page.layout.SetStyle(page.items[rounds % page.items.Size()], changed);
        auto t2 = std::chrono::steady_clock::now();
        page.layout.Update(page.root, width, 4000);
        auto t3 = std::chrono::steady_clock::now();
        full += Ms(t0, t1);
        incremental += Ms(t2, t3);
        rounds++;
    }
    page.layout.Update(page.root, 1700, 4000);
    u32 computedFull = page.layout.LastComputed();
    std::printf("full layout %.3f ms (%u computed), one item changed %.3f ms (budget 1 ms for the full layout)\n", full / rounds, computedFull,
                incremental / rounds);
}
