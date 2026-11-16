#include <Helix/UI/Styles/Properties.h>

#include <Helix/Core/Containers/HashMap.h>

#include <cmath>

namespace helix::ui
{
    bool StyleValue::operator==(const StyleValue& o) const
    {
        if (kind != o.kind)
            return false;
        switch (kind)
        {
        case Kind::Length: return length == o.length;
        case Kind::Number:
        case Kind::Time: return number == o.number;
        case Kind::Color: return color.r == o.color.r && color.g == o.color.g && color.b == o.color.b && color.a == o.color.a;
        case Kind::Keyword: return keyword == o.keyword;
        case Kind::Resource: return resource == o.resource;
        default: return true;
        }
    }

    namespace
    {
        const StringView kDirections[] = {"column", "column-reverse", "row", "row-reverse"};
        const StringView kWraps[] = {"nowrap", "wrap", "wrap-reverse"};
        const StringView kJustify[] = {"flex-start", "center", "flex-end", "space-between", "space-around", "space-evenly"};
        const StringView kAlign[] = {"auto", "flex-start", "center", "flex-end", "stretch", "space-between", "space-around", "space-evenly"};
        const StringView kPositions[] = {"relative", "absolute"};
        const StringView kDisplays[] = {"flex", "none"};
        const StringView kVisibility[] = {"visible", "hidden"};
        const StringView kOverflow[] = {"visible", "hidden"};
        const StringView kEasings[] = {"ease", "linear", "ease-in", "ease-out", "ease-in-out"};

        template <usize N>
        Span<const StringView> Words(const StringView (&words)[N])
        {
            return Span<const StringView>(words, N);
        }

        Length L(Span<const StyleValue> v) { return v.Empty() ? Length() : v[0].length; }
        f32 N(Span<const StyleValue> v) { return v.Empty() ? 0.0f : v[0].number; }
        Color C(Span<const StyleValue> v) { return v.Empty() ? Color() : v[0].color; }
        u32 K(Span<const StyleValue> v) { return v.Empty() ? 0u : v[0].keyword; }
        // Border widths are pixels.
        f32 Px(Span<const StyleValue> v) { return v.Empty() ? 0.0f : v[0].length.value; }

        StyleValue Len(Length l) { return StyleValue::OfLength(l); }
        StyleValue Num(f32 n) { return StyleValue::OfNumber(n); }
        StyleValue Col(Color c) { return StyleValue::OfColor(c); }
        StyleValue Key(u32 k) { return StyleValue::OfKeyword(k); }

        using V = Span<const StyleValue>;
        using S = ComputedStyle;

        const PropertyInfo kProperties[] = {
            {"width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.width = L(v); }, [](const S& s) { return Len(s.layout.width); }},
            {"height", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.height = L(v); }, [](const S& s) { return Len(s.layout.height); }},
            {"min-width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.minWidth = L(v); }, [](const S& s) { return Len(s.layout.minWidth); }},
            {"min-height", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.minHeight = L(v); }, [](const S& s) { return Len(s.layout.minHeight); }},
            {"max-width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.maxWidth = L(v); }, [](const S& s) { return Len(s.layout.maxWidth); }},
            {"max-height", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.maxHeight = L(v); }, [](const S& s) { return Len(s.layout.maxHeight); }},
            {"flex-basis", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.basis = L(v); }, [](const S& s) { return Len(s.layout.basis); }},
            {"flex-grow", ValueType::Number, false, true, false, {}, [](S& s, V v) { s.layout.grow = N(v); }, [](const S& s) { return Num(s.layout.grow); }},
            {"flex-shrink", ValueType::Number, false, true, false, {}, [](S& s, V v) { s.layout.shrink = N(v); }, [](const S& s) { return Num(s.layout.shrink); }},
            {"flex-direction", ValueType::Enum, false, true, false, Words(kDirections), [](S& s, V v) { s.layout.direction = FlexDirection(K(v)); },
             [](const S& s) { return Key(u32(s.layout.direction)); }},
            {"flex-wrap", ValueType::Enum, false, true, false, Words(kWraps), [](S& s, V v) { s.layout.wrap = FlexWrap(K(v)); },
             [](const S& s) { return Key(u32(s.layout.wrap)); }},
            {"justify-content", ValueType::Enum, false, true, false, Words(kJustify), [](S& s, V v) { s.layout.justify = Justify(K(v)); },
             [](const S& s) { return Key(u32(s.layout.justify)); }},
            {"align-items", ValueType::Enum, false, true, false, Words(kAlign), [](S& s, V v) { s.layout.alignItems = Align(K(v)); },
             [](const S& s) { return Key(u32(s.layout.alignItems)); }},
            {"align-self", ValueType::Enum, false, true, false, Words(kAlign), [](S& s, V v) { s.layout.alignSelf = Align(K(v)); },
             [](const S& s) { return Key(u32(s.layout.alignSelf)); }},
            {"align-content", ValueType::Enum, false, true, false, Words(kAlign), [](S& s, V v) { s.layout.alignContent = Align(K(v)); },
             [](const S& s) { return Key(u32(s.layout.alignContent)); }},
            {"position", ValueType::Enum, false, true, false, Words(kPositions), [](S& s, V v) { s.layout.position = Position(K(v)); },
             [](const S& s) { return Key(u32(s.layout.position)); }},
            {"display", ValueType::Enum, false, true, false, Words(kDisplays), [](S& s, V v) { s.layout.display = Display(K(v)); },
             [](const S& s) { return Key(u32(s.layout.display)); }},
            {"left", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.inset[kLeft] = L(v); }, [](const S& s) { return Len(s.layout.inset[kLeft]); }},
            {"top", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.inset[kTop] = L(v); }, [](const S& s) { return Len(s.layout.inset[kTop]); }},
            {"right", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.inset[kRight] = L(v); }, [](const S& s) { return Len(s.layout.inset[kRight]); }},
            {"bottom", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.inset[kBottom] = L(v); },
             [](const S& s) { return Len(s.layout.inset[kBottom]); }},
            {"margin-left", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.margin[kLeft] = L(v); },
             [](const S& s) { return Len(s.layout.margin[kLeft]); }},
            {"margin-top", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.margin[kTop] = L(v); },
             [](const S& s) { return Len(s.layout.margin[kTop]); }},
            {"margin-right", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.margin[kRight] = L(v); },
             [](const S& s) { return Len(s.layout.margin[kRight]); }},
            {"margin-bottom", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.margin[kBottom] = L(v); },
             [](const S& s) { return Len(s.layout.margin[kBottom]); }},
            {"padding-left", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.padding[kLeft] = L(v); },
             [](const S& s) { return Len(s.layout.padding[kLeft]); }},
            {"padding-top", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.padding[kTop] = L(v); },
             [](const S& s) { return Len(s.layout.padding[kTop]); }},
            {"padding-right", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.padding[kRight] = L(v); },
             [](const S& s) { return Len(s.layout.padding[kRight]); }},
            {"padding-bottom", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.padding[kBottom] = L(v); },
             [](const S& s) { return Len(s.layout.padding[kBottom]); }},
            {"border-left-width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.border[kLeft] = Px(v); },
             [](const S& s) { return Len(Length::Px(s.layout.border[kLeft])); }},
            {"border-top-width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.border[kTop] = Px(v); },
             [](const S& s) { return Len(Length::Px(s.layout.border[kTop])); }},
            {"border-right-width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.border[kRight] = Px(v); },
             [](const S& s) { return Len(Length::Px(s.layout.border[kRight])); }},
            {"border-bottom-width", ValueType::Length, false, true, false, {}, [](S& s, V v) { s.layout.border[kBottom] = Px(v); },
             [](const S& s) { return Len(Length::Px(s.layout.border[kBottom])); }},
            {"aspect-ratio", ValueType::Number, false, true, false, {}, [](S& s, V v) { s.layout.aspectRatio = N(v); },
             [](const S& s) { return Num(s.layout.aspectRatio); }},
            {"background-color", ValueType::Color, false, false, false, {}, [](S& s, V v) { s.backgroundColor = C(v); },
             [](const S& s) { return Col(s.backgroundColor); }},
            {"border-left-color", ValueType::Color, false, false, false, {}, [](S& s, V v) { s.borderColor[kLeft] = C(v); },
             [](const S& s) { return Col(s.borderColor[kLeft]); }},
            {"border-top-color", ValueType::Color, false, false, false, {}, [](S& s, V v) { s.borderColor[kTop] = C(v); },
             [](const S& s) { return Col(s.borderColor[kTop]); }},
            {"border-right-color", ValueType::Color, false, false, false, {}, [](S& s, V v) { s.borderColor[kRight] = C(v); },
             [](const S& s) { return Col(s.borderColor[kRight]); }},
            {"border-bottom-color", ValueType::Color, false, false, false, {}, [](S& s, V v) { s.borderColor[kBottom] = C(v); },
             [](const S& s) { return Col(s.borderColor[kBottom]); }},
            {"border-top-left-radius", ValueType::Length, false, false, false, {}, [](S& s, V v) { s.borderRadius[kTopLeft] = L(v); },
             [](const S& s) { return Len(s.borderRadius[kTopLeft]); }},
            {"border-top-right-radius", ValueType::Length, false, false, false, {}, [](S& s, V v) { s.borderRadius[kTopRight] = L(v); },
             [](const S& s) { return Len(s.borderRadius[kTopRight]); }},
            {"border-bottom-right-radius", ValueType::Length, false, false, false, {}, [](S& s, V v) { s.borderRadius[kBottomRight] = L(v); },
             [](const S& s) { return Len(s.borderRadius[kBottomRight]); }},
            {"border-bottom-left-radius", ValueType::Length, false, false, false, {}, [](S& s, V v) { s.borderRadius[kBottomLeft] = L(v); },
             [](const S& s) { return Len(s.borderRadius[kBottomLeft]); }},
            {"color", ValueType::Color, true, false, false, {}, [](S& s, V v) { s.color = C(v); }, [](const S& s) { return Col(s.color); }},
            {"opacity", ValueType::Number, false, false, false, {}, [](S& s, V v) { s.opacity = N(v); }, [](const S& s) { return Num(s.opacity); }},
            {"visibility", ValueType::Enum, true, false, false, Words(kVisibility), [](S& s, V v) { s.visibility = Visibility(K(v)); },
             [](const S& s) { return Key(u32(s.visibility)); }},
            {"overflow", ValueType::Enum, false, false, false, Words(kOverflow), [](S& s, V v) { s.overflow = Overflow(K(v)); },
             [](const S& s) { return Key(u32(s.overflow)); }},
            {"font-size", ValueType::Length, true, true, false, {}, [](S& s, V v) { s.fontSize = L(v); }, [](const S& s) { return Len(s.fontSize); }},
            {"background-image", ValueType::Resource, false, false, false, {}, [](S& s, V v) { s.backgroundImage = v.Empty() ? kNoAtom : v[0].resource; },
             [](const S& s) {
                 StyleValue r;
                 r.kind = StyleValue::Kind::Resource;
                 r.resource = s.backgroundImage;
                 return r;
             }},
            {"transition-property", ValueType::PropertyName, false, false, true, {},
             [](S& s, V v) {
                 s.transitionCount = u8(v.Size() < ComputedStyle::kMaxTransitions ? v.Size() : ComputedStyle::kMaxTransitions);
                 for (u32 i = 0; i < s.transitionCount; i++)
                     s.transitions[i].property = v[i].keyword;
             },
             [](const S&) { return StyleValue(); }},
            {"transition-duration", ValueType::Time, false, false, true, {},
             [](S& s, V v) {
                 for (u32 i = 0; i < ComputedStyle::kMaxTransitions && !v.Empty(); i++)
                     s.transitions[i].duration = v[i % v.Size()].number;
             },
             [](const S&) { return StyleValue(); }},
            {"transition-delay", ValueType::Time, false, false, true, {},
             [](S& s, V v) {
                 for (u32 i = 0; i < ComputedStyle::kMaxTransitions && !v.Empty(); i++)
                     s.transitions[i].delay = v[i % v.Size()].number;
             },
             [](const S&) { return StyleValue(); }},
            {"transition-timing-function", ValueType::Enum, false, false, true, Words(kEasings),
             [](S& s, V v) {
                 for (u32 i = 0; i < ComputedStyle::kMaxTransitions && !v.Empty(); i++)
                     s.transitions[i].easing = Easing(v[i % v.Size()].keyword);
             },
             [](const S&) { return StyleValue(); }},
        };

        HashMap<StringView, PropertyId>& ByName()
        {
            static HashMap<StringView, PropertyId>* map = [] {
                auto* m = new HashMap<StringView, PropertyId>();
                for (PropertyId i = 0; i < PropertyId(sizeof(kProperties) / sizeof(kProperties[0])); i++)
                    m->Set(kProperties[i].name, i);
                return m;
            }();
            return *map;
        }
    }

    Span<const PropertyInfo> Properties() { return Span<const PropertyInfo>(kProperties, sizeof(kProperties) / sizeof(kProperties[0])); }

    const PropertyInfo& Property(PropertyId id) { return kProperties[id]; }

    PropertyId FindProperty(StringView name)
    {
        const PropertyId* id = ByName().Find(name);
        return id ? *id : kNoProperty;
    }

    StyleValue Interpolate(const StyleValue& a, const StyleValue& b, f32 t)
    {
        if (a.kind != b.kind)
            return t < 1 ? a : b;
        StyleValue v = b;
        switch (a.kind)
        {
        case StyleValue::Kind::Number:
        case StyleValue::Kind::Time: v.number = a.number + (b.number - a.number) * t; break;
        case StyleValue::Kind::Color:
            v.color = Color(a.color.r + (b.color.r - a.color.r) * t, a.color.g + (b.color.g - a.color.g) * t, a.color.b + (b.color.b - a.color.b) * t,
                            a.color.a + (b.color.a - a.color.a) * t);
            break;
        case StyleValue::Kind::Length:
            if (a.length.unit == b.length.unit && !a.length.IsAuto())
                v.length.value = a.length.value + (b.length.value - a.length.value) * t;
            else
                v = t < 1 ? a : b;
            break;
        default: v = t < 1 ? a : b; break;
        }
        return v;
    }

    f32 Ease(Easing easing, f32 t)
    {
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        // The cubic Bezier curves of the CSS easings, solved for x by Newton steps.
        f32 x1, y1, x2, y2;
        switch (easing)
        {
        case Easing::Linear: return t;
        case Easing::EaseIn: x1 = 0.42f, y1 = 0, x2 = 1, y2 = 1; break;
        case Easing::EaseOut: x1 = 0, y1 = 0, x2 = 0.58f, y2 = 1; break;
        case Easing::EaseInOut: x1 = 0.42f, y1 = 0, x2 = 0.58f, y2 = 1; break;
        default: x1 = 0.25f, y1 = 0.1f, x2 = 0.25f, y2 = 1; break;
        }
        auto curve = [](f32 p1, f32 p2, f32 s) { return 3 * (1 - s) * (1 - s) * s * p1 + 3 * (1 - s) * s * s * p2 + s * s * s; };
        auto slope = [](f32 p1, f32 p2, f32 s) { return 3 * (1 - s) * (1 - s) * p1 + 6 * (1 - s) * s * (p2 - p1) + 3 * s * s * (1 - p2); };
        f32 s = t;
        for (u32 i = 0; i < 8; i++)
        {
            f32 d = slope(x1, x2, s);
            if (std::fabs(d) < 1e-6f)
                break;
            s -= (curve(x1, x2, s) - t) / d;
            s = s < 0 ? 0 : s > 1 ? 1 : s;
        }
        return curve(y1, y2, s);
    }
}
