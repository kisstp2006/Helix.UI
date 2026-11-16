#pragma once

#include <Helix/Core/Basics/Types.h>

// What the layout reads of an element's style: the flexbox properties, sizes, insets and box edges. Sizes are border
// box sizes (padding and border inside the width and height).
namespace helix::ui
{
    struct Length
    {
        enum class Unit : u8
        {
            Auto,
            Pixel,
            Percent,
        };
        f32 value = 0;
        Unit unit = Unit::Auto;

        static constexpr Length Auto() { return {}; }
        static constexpr Length Px(f32 v) { return {v, Unit::Pixel}; }
        static constexpr Length Percent(f32 v) { return {v, Unit::Percent}; }
        bool IsAuto() const { return unit == Unit::Auto; }
        bool operator==(const Length& o) const { return unit == o.unit && value == o.value; }
    };

    enum class FlexDirection : u8
    {
        Column,
        ColumnReverse,
        Row,
        RowReverse,
    };

    enum class FlexWrap : u8
    {
        NoWrap,
        Wrap,
        WrapReverse,
    };

    enum class Justify : u8
    {
        FlexStart,
        Center,
        FlexEnd,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly,
    };

    // align-items, align-self (Auto takes the parent's align-items) and align-content (the Space values).
    enum class Align : u8
    {
        Auto,
        FlexStart,
        Center,
        FlexEnd,
        Stretch,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly,
    };

    enum class Position : u8
    {
        Relative,
        Absolute,
    };

    enum class Display : u8
    {
        Flex,
        None,
    };

    // Box edges in the order left, top, right, bottom.
    enum Edge : u8
    {
        kLeft,
        kTop,
        kRight,
        kBottom,
    };

    struct LayoutStyle
    {
        FlexDirection direction = FlexDirection::Column;
        FlexWrap wrap = FlexWrap::NoWrap;
        Justify justify = Justify::FlexStart;
        Align alignItems = Align::Stretch;
        Align alignSelf = Align::Auto;
        Align alignContent = Align::FlexStart;
        Position position = Position::Relative;
        Display display = Display::Flex;
        f32 grow = 0;
        f32 shrink = 1;
        Length basis;
        Length width, height;
        // Auto minimums are 0, auto maximums are none.
        Length minWidth, minHeight, maxWidth, maxHeight;
        // left, top, right, bottom of a positioned element.
        Length inset[4];
        // Margins may be auto (they take the free space); paddings and percentages of both are of the parent's width.
        Length margin[4] = {Length::Px(0), Length::Px(0), Length::Px(0), Length::Px(0)};
        Length padding[4] = {Length::Px(0), Length::Px(0), Length::Px(0), Length::Px(0)};
        f32 border[4] = {0, 0, 0, 0};
        // width / height; 0 for none.
        f32 aspectRatio = 0;
    };

    // How a measured size is bound: free, exactly the given size, or at most the given size.
    enum class MeasureMode : u8
    {
        Undefined,
        Exactly,
        AtMost,
    };

    // An element's place after layout: its border box relative to its parent's border box, and its resolved edges.
    struct LayoutBox
    {
        f32 x = 0, y = 0, width = 0, height = 0;
        f32 margin[4] = {0, 0, 0, 0};
        f32 padding[4] = {0, 0, 0, 0};
        f32 border[4] = {0, 0, 0, 0};
    };
}
