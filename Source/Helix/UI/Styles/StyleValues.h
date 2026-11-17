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
            // An angle: number in its unit, keyword the unit (AngleUnit).
            Angle,
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
        static StyleValue OfAngle(f32 value, u32 unit)
        {
            StyleValue v;
            v.kind = Kind::Angle;
            v.number = value;
            v.keyword = unit;
            return v;
        }
        static StyleValue OfResource(Atom resource)
        {
            StyleValue v;
            v.kind = Kind::Resource;
            v.resource = resource;
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

    // The units of angles, in the order of the reference's AngleUnit.
    enum class AngleUnit : u8
    {
        Degree,
        Gradian,
        Radian,
        Turn,
    };

    // An angle value in degrees.
    f32 AngleDegrees(const StyleValue& v);

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

    // background-position-x and -y: the edge an offset is from (or the middle).
    enum class BackgroundPositionKeyword : u8
    {
        Center,
        Top,
        Bottom,
        Left,
        Right,
    };

    enum class BackgroundRepeat : u8
    {
        NoRepeat,
        Space,
        Round,
        Repeat,
    };

    enum class BackgroundSizeType : u8
    {
        Length,
        Cover,
        Contain,
    };

    // Where an ellipsis cuts text.
    enum class TextOverflowPosition : u8
    {
        End,
        Start,
        Middle,
    };

    // The box overflow: hidden clips the descendants to.
    enum class OverflowClipBox : u8
    {
        PaddingBox,
        ContentBox,
    };

    // The cursor's system shapes (cursor keywords).
    enum class CursorShape : u8
    {
        Arrow,
        Text,
        ResizeVertical,
        ResizeHorizontal,
        Link,
        SlideArrow,
        ResizeUpRight,
        ResizeUpLeft,
        MoveArrow,
        RotateArrow,
        ScaleArrow,
        ArrowPlus,
        ArrowMinus,
        Pan,
        Orbit,
        Zoom,
        Fps,
        SplitResizeUpDown,
        SplitResizeLeftRight,
        NotAllowed,
        Wait,
        Crosshair,
    };

    enum class FontStyle : u8
    {
        Normal,
        Bold,
        Italic,
        BoldAndItalic,
    };

    // Where text sits in its element's content box.
    enum class TextAnchor : u8
    {
        UpperLeft,
        UpperCenter,
        UpperRight,
        MiddleLeft,
        MiddleCenter,
        MiddleRight,
        LowerLeft,
        LowerCenter,
        LowerRight,
    };

    enum class WhiteSpace : u8
    {
        Normal,
        NoWrap,
        Pre,
        PreWrap,
    };

    enum class TextOverflow : u8
    {
        Clip,
        Ellipsis,
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
        // The background image's tint and 9-slice (texture pixels from the edges; screen pixels per them).
        Color backgroundTint = Color(1, 1, 1, 1);
        f32 slices[4] = {0, 0, 0, 0};
        f32 sliceScale = 1;
        // Text: a font asset (url or resource), the machine's font families after it, and how the text is set.
        static constexpr u32 kMaxFontFamilies = 6;
        Atom fontDefinition = kNoAtom;
        Atom fontFamilies[kMaxFontFamilies] = {};
        u8 fontFamilyCount = 0;
        FontStyle fontStyle = FontStyle::Normal;
        TextAnchor textAlign = TextAnchor::UpperLeft;
        WhiteSpace whiteSpace = WhiteSpace::Normal;
        TextOverflow textOverflow = TextOverflow::Clip;
        Length letterSpacing = Length::Px(0), wordSpacing = Length::Px(0), paragraphSpacing = Length::Px(0);
        TransitionSpec transitions[kMaxTransitions];
        u8 transitionCount = 0;
        // Transforms: a translation (% of the border box), a rotation in degrees, a scale, about an origin (% or px
        // of the border box).
        Length translate[2] = {Length::Px(0), Length::Px(0)};
        f32 rotate = 0;
        // The rotation's axis (z: in the screen's plane).
        f32 rotateAxis[3] = {0, 0, 1};
        f32 scale[2] = {1, 1};
        Length transformOrigin[2] = {Length::Percent(50), Length::Percent(50)};
        // The background image's place, size and repetition per axis (x, y); the initial size fills the box (as the
        // scale mode stretch-to-fill does).
        BackgroundPositionKeyword backgroundPositionKeyword[2] = {BackgroundPositionKeyword::Center, BackgroundPositionKeyword::Center};
        Length backgroundPositionOffset[2] = {Length::Px(0), Length::Px(0)};
        BackgroundSizeType backgroundSizeType = BackgroundSizeType::Length;
        Length backgroundSize[2] = {Length::Percent(100), Length::Percent(100)};
        BackgroundRepeat backgroundRepeat[2] = {BackgroundRepeat::NoRepeat, BackgroundRepeat::NoRepeat};
        // Text: its shadow (offset and blur in pixels; transparent: none), its outline, where the ellipsis cuts.
        f32 textShadowOffset[2] = {0, 0};
        f32 textShadowBlur = 0;
        Color textShadowColor = Color(0, 0, 0, 0);
        f32 textOutlineWidth = 0;
        Color textOutlineColor = Color(0, 0, 0, 1);
        TextOverflowPosition textOverflowPosition = TextOverflowPosition::End;
        OverflowClipBox overflowClipBox = OverflowClipBox::PaddingBox;
        // The cursor over the element (inherited, as in CSS): an image with its hotspot, or a system shape; unset:
        // the default.
        Atom cursorImage = kNoAtom;
        f32 cursorHotspot[2] = {0, 0};
        bool cursorSet = false;
        CursorShape cursorShape = CursorShape::Arrow;
    };

    // A property's place in the tables of Properties.h.
    using PropertyId = u16;
    inline constexpr PropertyId kNoProperty = 0xFFFF;
}
