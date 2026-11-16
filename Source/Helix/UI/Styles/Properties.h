#pragma once

#include <Helix/UI/Styles/StyleValues.h>

// The HSS properties as a table: each one's name, value type, whether it is inherited, laid out or animated, its
// keywords, and how it is written into and read from a computed style. Shorthands (margin, padding, border-width,
// border-color, border-radius, flex, transition) are expanded by the parser into these.
namespace helix::ui
{
    enum class ValueType : u8
    {
        Length,
        Number,
        Color,
        Enum,
        Time,
        Resource,
        // The names of properties (transition-property).
        PropertyName,
    };

    struct PropertyInfo
    {
        StringView name;
        ValueType type;
        bool inherited;
        // Changes the layout.
        bool layout;
        // May take a list of values separated by commas.
        bool list;
        // The keywords of an Enum, in the order of their values.
        Span<const StringView> keywords;
        void (*set)(ComputedStyle& style, Span<const StyleValue> values);
        // The value of a single valued property (what transitions animate).
        StyleValue (*get)(const ComputedStyle& style);
    };

    Span<const PropertyInfo> Properties();
    const PropertyInfo& Property(PropertyId id);
    // kNoProperty for an unknown name.
    PropertyId FindProperty(StringView name);
    // A value between two of a property (lengths of the same unit, numbers and colors; others switch at the end).
    StyleValue Interpolate(const StyleValue& a, const StyleValue& b, f32 t);
    // The position of an easing curve at linear time t (0 to 1).
    f32 Ease(Easing easing, f32 t);
}
