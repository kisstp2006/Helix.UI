#include <Helix/Jobs/Jobs.h>
#include <Helix/UI/Layout/Layout.h>
#include <Helix/Testing/Test.h>

#include <cmath>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

namespace
{
    struct Fixture
    {
        ElementTree tree;
        LayoutEngine layout{tree};
        ElementHandle root;

        explicit Fixture(LayoutStyle rootStyle = {})
        {
            root = tree.Create(Intern("Element"));
            layout.SetStyle(root, rootStyle);
        }

        ElementHandle Add(ElementHandle parent, const LayoutStyle& style)
        {
            ElementHandle e = tree.Create(Intern("Element"));
            tree.Append(parent, e);
            layout.SetStyle(e, style);
            return e;
        }
    };

    LayoutStyle Sized(f32 w, f32 h)
    {
        LayoutStyle s;
        if (w >= 0)
            s.width = Length::Px(w);
        if (h >= 0)
            s.height = Length::Px(h);
        return s;
    }

    bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.01f; }

    bool BoxIs(const LayoutEngine& l, ElementHandle e, f32 x, f32 y, f32 w, f32 h)
    {
        const LayoutBox& b = l.Box(e);
        bool ok = Near(b.x, x) && Near(b.y, y) && Near(b.width, w) && Near(b.height, h);
        if (!ok)
            std::printf("    box %g %g %g %g, expected %g %g %g %g\n", b.x, b.y, b.width, b.height, x, y, w, h);
        return ok;
    }
}

HX_TEST(Layout, AColumnStretchesItsChildrenAndGrowsOne)
{
    LayoutStyle rs;
    for (auto& p : rs.padding)
        p = Length::Px(10);
    Fixture f(rs);
    ElementHandle a = f.Add(f.root, Sized(-1, 50));
    LayoutStyle grow;
    grow.grow = 1;
    ElementHandle b = f.Add(f.root, grow);
    ElementHandle c = f.Add(f.root, Sized(-1, 30));
    f.layout.Update(f.root, 400, 300);
    HX_CHECK(BoxIs(f.layout, f.root, 0, 0, 400, 300));
    HX_CHECK(BoxIs(f.layout, a, 10, 10, 380, 50));
    HX_CHECK(BoxIs(f.layout, b, 10, 60, 380, 200));
    HX_CHECK(BoxIs(f.layout, c, 10, 260, 380, 30));
    Rect world = f.layout.WorldRect(c);
    HX_CHECK(Near(world.x, 10) && Near(world.y, 260));
}

HX_TEST(Layout, RowsJustifyTheirItems)
{
    const Justify kinds[] = {Justify::FlexStart, Justify::Center, Justify::FlexEnd, Justify::SpaceBetween, Justify::SpaceAround, Justify::SpaceEvenly};
    // Three items of 50 in 300: free 150.
    const f32 want[6][3] = {{0, 50, 100}, {75, 125, 175}, {150, 200, 250}, {0, 125, 250}, {25, 125, 225}, {37.5f, 125, 212.5f}};
    for (u32 k = 0; k < 6; k++)
    {
        LayoutStyle rs;
        rs.direction = FlexDirection::Row;
        rs.justify = kinds[k];
        Fixture f(rs);
        ElementHandle items[3];
        for (auto& it : items)
            it = f.Add(f.root, Sized(50, 20));
        f.layout.Update(f.root, 300, 100);
        for (u32 i = 0; i < 3; i++)
            HX_CHECK(BoxIs(f.layout, items[i], want[k][i], 0, 50, 20));
    }
}

HX_TEST(Layout, ShrinkingRespectsMinimumsAndGrowingMaximums)
{
    LayoutStyle rs;
    rs.direction = FlexDirection::Row;
    Fixture f(rs);
    LayoutStyle s;
    s.basis = Length::Px(150);
    ElementHandle a = f.Add(f.root, s);
    ElementHandle b = f.Add(f.root, s);
    f.layout.Update(f.root, 200, 50);
    HX_CHECK(BoxIs(f.layout, a, 0, 0, 100, 50) && BoxIs(f.layout, b, 100, 0, 100, 50));
    s.minWidth = Length::Px(120);
    f.layout.SetStyle(a, s);
    f.layout.Update(f.root, 200, 50);
    HX_CHECK(BoxIs(f.layout, a, 0, 0, 120, 50) && BoxIs(f.layout, b, 120, 0, 80, 50));

    // Growing: 2:1, the first capped at 60, the rest goes to the second.
    Fixture g(rs);
    LayoutStyle grow;
    grow.grow = 2;
    grow.maxWidth = Length::Px(60);
    ElementHandle c = g.Add(g.root, grow);
    LayoutStyle grow1;
    grow1.grow = 1;
    ElementHandle d = g.Add(g.root, grow1);
    g.layout.Update(g.root, 300, 10);
    HX_CHECK(BoxIs(g.layout, c, 0, 0, 60, 10) && BoxIs(g.layout, d, 60, 0, 240, 10));
}

HX_TEST(Layout, WrappingMakesLines)
{
    LayoutStyle rs;
    rs.direction = FlexDirection::Row;
    rs.wrap = FlexWrap::Wrap;
    Fixture f(rs);
    ElementHandle items[5];
    for (auto& it : items)
        it = f.Add(f.root, Sized(100, 40));
    f.layout.Update(f.root, 250, 200);
    HX_CHECK(BoxIs(f.layout, items[0], 0, 0, 100, 40) && BoxIs(f.layout, items[1], 100, 0, 100, 40));
    HX_CHECK(BoxIs(f.layout, items[2], 0, 40, 100, 40) && BoxIs(f.layout, items[3], 100, 40, 100, 40));
    HX_CHECK(BoxIs(f.layout, items[4], 0, 80, 100, 40));
    // Lines centered across.
    rs.alignContent = Align::Center;
    f.layout.SetStyle(f.root, rs);
    f.layout.Update(f.root, 250, 200);
    HX_CHECK(BoxIs(f.layout, items[0], 0, 40, 100, 40) && BoxIs(f.layout, items[4], 0, 120, 100, 40));
}

HX_TEST(Layout, ItemsAlignAcrossTheLine)
{
    LayoutStyle rs;
    rs.direction = FlexDirection::Row;
    rs.alignItems = Align::Center;
    Fixture f(rs);
    ElementHandle a = f.Add(f.root, Sized(50, 20));
    LayoutStyle end = Sized(50, 20);
    end.alignSelf = Align::FlexEnd;
    ElementHandle b = f.Add(f.root, end);
    LayoutStyle stretch = Sized(50, -1);
    stretch.alignSelf = Align::Stretch;
    ElementHandle c = f.Add(f.root, stretch);
    f.layout.Update(f.root, 300, 100);
    HX_CHECK(BoxIs(f.layout, a, 0, 40, 50, 20) && BoxIs(f.layout, b, 50, 80, 50, 20) && BoxIs(f.layout, c, 100, 0, 50, 100));
}

HX_TEST(Layout, PercentagesMarginsAndAutoMargins)
{
    LayoutStyle rs;
    rs.direction = FlexDirection::Row;
    for (auto& p : rs.padding)
        p = Length::Px(20);
    Fixture f(rs);
    LayoutStyle half;
    half.width = Length::Percent(50);
    half.height = Length::Percent(25);
    half.margin[kLeft] = Length::Px(5);
    ElementHandle a = f.Add(f.root, half);
    f.layout.Update(f.root, 440, 240);
    // Inner 400 x 200.
    HX_CHECK(BoxIs(f.layout, a, 25, 20, 200, 50));
    HX_CHECK(Near(f.layout.Box(a).margin[kLeft], 5));

    // Auto margins on both sides center an item.
    Fixture g(rs);
    LayoutStyle centered = Sized(100, 50);
    centered.margin[kLeft] = Length::Auto();
    centered.margin[kRight] = Length::Auto();
    ElementHandle b = g.Add(g.root, centered);
    g.layout.Update(g.root, 440, 240);
    HX_CHECK(BoxIs(g.layout, b, 170, 20, 100, 50));
}

HX_TEST(Layout, AbsoluteElementsByTheirInsets)
{
    LayoutStyle rs;
    for (u32 k = 0; k < 4; k++)
        rs.border[k] = 2, rs.padding[k] = Length::Px(8);
    Fixture f(rs);
    LayoutStyle abs = Sized(30, 40);
    abs.position = Position::Absolute;
    abs.inset[kLeft] = Length::Px(10);
    abs.inset[kTop] = Length::Px(20);
    ElementHandle a = f.Add(f.root, abs);
    LayoutStyle corner = Sized(30, 40);
    corner.position = Position::Absolute;
    corner.inset[kRight] = Length::Px(0);
    corner.inset[kBottom] = Length::Px(0);
    ElementHandle b = f.Add(f.root, corner);
    LayoutStyle spread;
    spread.position = Position::Absolute;
    for (u32 k = 0; k < 4; k++)
        spread.inset[k] = Length::Px(5);
    ElementHandle c = f.Add(f.root, spread);
    // An absolute element takes no room from the flow.
    ElementHandle flow = f.Add(f.root, Sized(-1, 10));
    f.layout.Update(f.root, 200, 100);
    HX_CHECK(BoxIs(f.layout, a, 12, 22, 30, 40));
    HX_CHECK(BoxIs(f.layout, b, 168, 58, 30, 40));
    HX_CHECK(BoxIs(f.layout, c, 7, 7, 186, 86));
    HX_CHECK(BoxIs(f.layout, flow, 10, 10, 180, 10));
}

HX_TEST(Layout, AspectRatioDisplayNoneAndReverse)
{
    LayoutStyle rs;
    rs.direction = FlexDirection::RowReverse;
    rs.alignItems = Align::FlexStart;
    Fixture f(rs);
    LayoutStyle wide = Sized(100, -1);
    wide.aspectRatio = 2;
    ElementHandle a = f.Add(f.root, wide);
    LayoutStyle gone = Sized(70, 70);
    gone.display = Display::None;
    ElementHandle hidden = f.Add(f.root, gone);
    ElementHandle b = f.Add(f.root, Sized(30, 30));
    f.layout.Update(f.root, 300, 100);
    HX_CHECK(BoxIs(f.layout, a, 200, 0, 100, 50) && BoxIs(f.layout, b, 170, 0, 30, 30) && BoxIs(f.layout, hidden, 0, 0, 0, 0));
}

HX_TEST(Layout, AspectRatioFromTheStretchedCrossSize)
{
    // A column stretches its items across: a ratio gives their height from that width (margins left out).
    Fixture column;
    LayoutStyle picture;
    picture.aspectRatio = 1.6f;
    picture.margin[kLeft] = Length::Px(10);
    picture.margin[kRight] = Length::Px(10);
    ElementHandle p = column.Add(column.root, picture);
    ElementHandle after = column.Add(column.root, Sized(-1, 20));
    column.layout.Update(column.root, 180, 300);
    HX_CHECK(BoxIs(column.layout, p, 10, 0, 160, 100) && BoxIs(column.layout, after, 0, 100, 180, 20));
    // A row: the width from the stretched height.
    LayoutStyle rs;
    rs.direction = FlexDirection::Row;
    Fixture row(rs);
    LayoutStyle square;
    square.aspectRatio = 1;
    ElementHandle s = row.Add(row.root, square);
    row.layout.Update(row.root, 300, 40);
    HX_CHECK(BoxIs(row.layout, s, 0, 0, 40, 40));
}

namespace
{
    // Text like content: 10 per character, wrapped at the available width, 20 per line.
    Vector2 MeasureText(void* user, ElementHandle, f32 width, MeasureMode mode, f32, MeasureMode)
    {
        f32 full = f32(*static_cast<u32*>(user)) * 10.0f;
        if (mode == MeasureMode::Undefined || full <= width)
            return Vector2(full, 20);
        f32 perLine = std::floor(width / 10.0f) * 10.0f;
        f32 lines = std::ceil(full / perLine);
        return Vector2(perLine, lines * 20);
    }
}

HX_TEST(Layout, ContentSizesFromMeasureFunctions)
{
    LayoutStyle rs;
    rs.alignItems = Align::FlexStart;
    Fixture f(rs);
    ElementHandle shortText = f.Add(f.root, {});
    ElementHandle longText = f.Add(f.root, {});
    u32 shortChars = 5, longChars = 25;
    f.layout.SetMeasure(shortText, &MeasureText, &shortChars);
    f.layout.SetMeasure(longText, &MeasureText, &longChars);
    f.layout.Update(f.root, 120, 300);
    HX_CHECK(BoxIs(f.layout, shortText, 0, 0, 50, 20));
    // 250 wide text in 120: 12 characters a line, 3 lines.
    HX_CHECK(BoxIs(f.layout, longText, 0, 20, 120, 60));

    // A row sized by its content inside a column: as wide as its children.
    LayoutStyle rowStyle;
    rowStyle.direction = FlexDirection::Row;
    ElementHandle row = f.Add(f.root, rowStyle);
    ElementHandle x = f.Add(row, Sized(30, 10));
    ElementHandle y = f.Add(row, Sized(40, 15));
    f.layout.Update(f.root, 120, 300);
    HX_CHECK(BoxIs(f.layout, row, 0, 80, 70, 15) && BoxIs(f.layout, x, 0, 0, 30, 10) && BoxIs(f.layout, y, 30, 0, 40, 15));
}

HX_TEST(Layout, OnlyDirtyElementsAreComputedAgain)
{
    Fixture f;
    LayoutStyle rowStyle;
    rowStyle.direction = FlexDirection::Row;
    ElementHandle rows[10];
    ElementHandle last;
    for (auto& r : rows)
    {
        r = f.Add(f.root, rowStyle);
        for (u32 c = 0; c < 10; c++)
            last = f.Add(r, Sized(10, 10));
    }
    f.layout.Update(f.root, 500, 500);
    HX_CHECK(f.layout.LastComputed() > 0);
    f.layout.Update(f.root, 500, 500);
    HX_CHECK(f.layout.LastComputed() == 0);
    f.layout.SetStyle(last, Sized(20, 10));
    f.layout.Update(f.root, 500, 500);
    u32 again = f.layout.LastComputed();
    if (again > 6)
        std::printf("    computed %u elements again\n", again);
    HX_CHECK(again > 0 && again <= 6);
    HX_CHECK(BoxIs(f.layout, last, 90, 0, 20, 10));
}

HX_TEST(Layout, ManySubtreesOnTheJobsGiveTheSameBoxes)
{
    // 60 rows of 60 items: enough work for the jobs; every box where the arithmetic puts it.
    if (!jobs::IsInitialized())
        jobs::Initialize();
    Fixture f;
    LayoutStyle rowStyle;
    rowStyle.direction = FlexDirection::Row;
    rowStyle.justify = Justify::Center;
    Array<ElementHandle> items;
    for (u32 r = 0; r < 60; r++)
    {
        ElementHandle row = f.Add(f.root, rowStyle);
        for (u32 c = 0; c < 60; c++)
            items.Push(f.Add(row, Sized(10, 5 + f32(r % 3))));
    }
    f.layout.Update(f.root, 800, 600);
    bool all = true;
    f32 y = 0;
    for (u32 r = 0; r < 60; r++)
    {
        f32 h = 5 + f32(r % 3);
        for (u32 c = 0; c < 60; c++)
        {
            Rect world = f.layout.WorldRect(items[r * 60 + c]);
            all = all && Near(world.x, 100 + f32(c) * 10) && Near(world.y, y) && Near(world.width, 10) && Near(world.height, h);
        }
        y += h;
    }
    HX_CHECK(all);
}

HX_TEST(Layout, WorldRectsSitOnThePixelGridAndStillTouch)
{
    LayoutStyle rs;
    rs.direction = FlexDirection::Row;
    Fixture f(rs);
    LayoutStyle grow;
    grow.grow = 1;
    ElementHandle items[3] = {f.Add(f.root, grow), f.Add(f.root, grow), f.Add(f.root, grow)};
    f.layout.Update(f.root, 100, 20);
    // The boxes keep their thirds; their world rects round the edges.
    HX_CHECK(Near(f.layout.Box(items[1]).x, 33.333f) && Near(f.layout.Box(items[1]).width, 33.333f));
    Rect a = f.layout.WorldRect(items[0]), b = f.layout.WorldRect(items[1]), c = f.layout.WorldRect(items[2]);
    HX_CHECK(Near(a.x, 0) && Near(a.width, 33));
    HX_CHECK(Near(b.x, 33) && Near(b.width, 34));
    HX_CHECK(Near(c.x, 67) && Near(c.width, 33));
}
