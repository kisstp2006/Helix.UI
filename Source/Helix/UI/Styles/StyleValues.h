#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Math/Color.h>
#include <Helix/UI/Core/Atoms.h>
#include <Helix/UI/Layout/LayoutStyle.h>

// The values of HSS properties and an element's computed style.
namespace helix::ui
{
    // One value of a declaration.
    struct StyleValue
    {
        enum class Kind : u8
        {
            None,
            Length,
            Number,
            Color,
            // A keyword: an enum value's index for enum properties, the atom of a property name in transition-property.
            Keyword,
            // Seconds.
            Time,
            // The text of url() or resource().
            Resource,
        };
        Kind kind = Kind::None;
        Length length;
        f32 number = 0;
        Color color;
        u32 keyword = 0;
        Atom resource = kNoAtom;

        static StyleValue OfLength(Length l)
        {
            StyleValue v;
            v.kind = Kind::Length;
            v.length = l;
            return v;
        }
        static StyleValue OfNumber(f32 n)
        {
            StyleValue v;
            v.kind = Kind::Number;
            v.number = n;
            return v;
        }
        static StyleValue OfColor(Color c)
        {
            StyleValue v;
            v.kind = Kind::Color;
            v.color = c;
            return v;
        }
        static StyleValue OfKeyword(u32 k)
        {
            StyleValue v;
            v.kind = Kind::Keyword;
            v.keyword = k;
            return v;
        }
        static StyleValue OfTime(f32 seconds)
        {
            StyleValue v;
            v.kind = Kind::Time;
            v.number = seconds;
            return v;
        }
        bool operator==(const StyleValue& o) const;
    };

    enum class Visibility : u8
    {
        Visible,
        Hidden,
    };

    enum class Overflow : u8
    {
        Visible,
        Hidden,
    };

    enum class Easing : u8
    {
        Ease,
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
    };

    // A transition of one property (kNoAtom: every property).
    struct TransitionSpec
    {
        Atom property = kNoAtom;
        f32 duration = 0;
        f32 delay = 0;
        Easing easing = Easing::Ease;
    };

    // Corners in the order top left, top right, bottom right, bottom left.
    enum Corner : u8
    {
        kTopLeft,
        kTopRight,
        kBottomRight,
        kBottomLeft,
    };

    struct ComputedStyle
    {
        static constexpr u32 kMaxTransitions = 8;

        LayoutStyle layout;
        Color backgroundColor = Color(0, 0, 0, 0);
        Color borderColor[4] = {Color(0, 0, 0, 1), Color(0, 0, 0, 1), Color(0, 0, 0, 1), Color(0, 0, 0, 1)};
        Length borderRadius[4] = {Length::Px(0), Length::Px(0), Length::Px(0), Length::Px(0)};
        Color color = Color(0, 0, 0, 1);
        f32 opacity = 1;
        Visibility visibility = Visibility::Visible;
        Overflow overflow = Overflow::Visible;
        Length fontSize = Length::Px(12);
        Atom backgroundImage = kNoAtom;
        TransitionSpec transitions[kMaxTransitions];
        u8 transitionCount = 0;
    };

    // A property's place in the tables of Properties.h.
    using PropertyId = u16;
    inline constexpr PropertyId kNoProperty = 0xFFFF;
}
