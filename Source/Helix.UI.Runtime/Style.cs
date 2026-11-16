using System;
using System.Collections.Generic;
using System.Globalization;

namespace HelixEngine
{
    public enum FontStyle
    {
        Normal = 0,
        Bold = 1,
        Italic = 2,
        BoldAndItalic = 3,
    }

    public enum TextAnchor
    {
        UpperLeft = 0,
        UpperCenter = 1,
        UpperRight = 2,
        MiddleLeft = 3,
        MiddleCenter = 4,
        MiddleRight = 5,
        LowerLeft = 6,
        LowerCenter = 7,
        LowerRight = 8,
    }
}

namespace HelixEngine.UI
{
    public enum StyleKeyword
    {
        Undefined = 0,
        Null = 1,
        Auto = 2,
        None = 3,
        Initial = 4,
    }

    public enum LengthUnit
    {
        Pixel = 0,
        Percent = 1,
    }

    public enum Align { Auto, FlexStart, Center, FlexEnd, Stretch, SpaceBetween, SpaceAround, SpaceEvenly }
    public enum Justify { FlexStart, Center, FlexEnd, SpaceBetween, SpaceAround, SpaceEvenly }
    public enum FlexDirection { Column, ColumnReverse, Row, RowReverse }
    public enum Wrap { NoWrap, Wrap, WrapReverse }
    public enum Position { Relative, Absolute }
    public enum DisplayStyle { Flex, None }
    public enum Visibility { Visible, Hidden }
    public enum Overflow { Visible, Hidden }
    public enum WhiteSpace { Normal, NoWrap, Pre, PreWrap }
    public enum TextOverflow { Clip, Ellipsis }

    public struct Length : IEquatable<Length>
    {
        public float value { get; set; }
        public LengthUnit unit { get; set; }
        internal bool isAuto, isNone;

        public Length(float value) : this(value, LengthUnit.Pixel) { }

        public Length(float value, LengthUnit unit)
        {
            this.value = value;
            this.unit = unit;
            isAuto = isNone = false;
        }

        public static Length Percent(float value) => new Length(value, LengthUnit.Percent);
        public static Length Auto() => new Length { isAuto = true };
        public static Length None() => new Length { isNone = true };
        public bool IsAuto() => isAuto;
        public bool IsNone() => isNone;

        public static implicit operator Length(float value) => new Length(value);
        public static bool operator ==(Length a, Length b) => a.Equals(b);
        public static bool operator !=(Length a, Length b) => !a.Equals(b);
        public bool Equals(Length other) => value == other.value && unit == other.unit && isAuto == other.isAuto && isNone == other.isNone;
        public override bool Equals(object obj) => obj is Length l && Equals(l);
        public override int GetHashCode() => HashCode.Combine(value, unit, isAuto, isNone);

        public override string ToString()
        {
            if (isAuto)
                return "auto";
            if (isNone)
                return "none";
            return value.ToString(CultureInfo.InvariantCulture) + (unit == LengthUnit.Percent ? "%" : "px");
        }
    }

    public interface IStyleValue<T>
    {
        T value { get; set; }
        StyleKeyword keyword { get; set; }
    }

    public struct StyleLength : IStyleValue<Length>, IEquatable<StyleLength>
    {
        public Length value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleLength(float v) : this(new Length(v)) { }
        public StyleLength(Length v) : this(v, StyleKeyword.Undefined) { }
        public StyleLength(StyleKeyword keyword) : this(default, keyword) { }

        StyleLength(Length v, StyleKeyword k)
        {
            value = v;
            keyword = k;
        }

        public static implicit operator StyleLength(StyleKeyword keyword) => new StyleLength(keyword);
        public static implicit operator StyleLength(float v) => new StyleLength(v);
        public static implicit operator StyleLength(Length v) => new StyleLength(v);
        public static bool operator ==(StyleLength a, StyleLength b) => a.Equals(b);
        public static bool operator !=(StyleLength a, StyleLength b) => !a.Equals(b);
        public bool Equals(StyleLength other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleLength s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleFloat : IStyleValue<float>, IEquatable<StyleFloat>
    {
        public float value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleFloat(float v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleFloat(StyleKeyword keyword)
        {
            value = 0;
            this.keyword = keyword;
        }

        public static implicit operator StyleFloat(StyleKeyword keyword) => new StyleFloat(keyword);
        public static implicit operator StyleFloat(float v) => new StyleFloat(v);
        public static bool operator ==(StyleFloat a, StyleFloat b) => a.Equals(b);
        public static bool operator !=(StyleFloat a, StyleFloat b) => !a.Equals(b);
        public bool Equals(StyleFloat other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleFloat s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString(CultureInfo.InvariantCulture) : keyword.ToString();
    }

    public struct StyleInt : IStyleValue<int>, IEquatable<StyleInt>
    {
        public int value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleInt(int v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleInt(StyleKeyword keyword)
        {
            value = 0;
            this.keyword = keyword;
        }

        public static implicit operator StyleInt(StyleKeyword keyword) => new StyleInt(keyword);
        public static implicit operator StyleInt(int v) => new StyleInt(v);
        public static bool operator ==(StyleInt a, StyleInt b) => a.Equals(b);
        public static bool operator !=(StyleInt a, StyleInt b) => !a.Equals(b);
        public bool Equals(StyleInt other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleInt s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString(CultureInfo.InvariantCulture) : keyword.ToString();
    }

    public struct StyleColor : IStyleValue<Color>, IEquatable<StyleColor>
    {
        public Color value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleColor(Color v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleColor(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleColor(StyleKeyword keyword) => new StyleColor(keyword);
        public static implicit operator StyleColor(Color v) => new StyleColor(v);
        public static bool operator ==(StyleColor a, StyleColor b) => a.Equals(b);
        public static bool operator !=(StyleColor a, StyleColor b) => !a.Equals(b);
        public static bool operator ==(StyleColor a, Color b) => a.keyword == StyleKeyword.Undefined && a.value == b;
        public static bool operator !=(StyleColor a, Color b) => !(a == b);
        public bool Equals(StyleColor other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleColor s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleEnum<T> : IStyleValue<T>, IEquatable<StyleEnum<T>> where T : struct, IConvertible
    {
        public T value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleEnum(T v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleEnum(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleEnum<T>(StyleKeyword keyword) => new StyleEnum<T>(keyword);
        public static implicit operator StyleEnum<T>(T v) => new StyleEnum<T>(v);
        public static bool operator ==(StyleEnum<T> a, StyleEnum<T> b) => a.Equals(b);
        public static bool operator !=(StyleEnum<T> a, StyleEnum<T> b) => !a.Equals(b);
        public bool Equals(StyleEnum<T> other) => keyword == other.keyword && EqualityComparer<T>.Default.Equals(value, other.value);
        public override bool Equals(object obj) => obj is StyleEnum<T> s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    // The words of the style enums in HSS, by the enum's value.
    static class StyleWords
    {
        static readonly Dictionary<Type, string[]> s_Words = new Dictionary<Type, string[]>
        {
            [typeof(Align)] = new[] { "auto", "flex-start", "center", "flex-end", "stretch", "space-between", "space-around", "space-evenly" },
            [typeof(Justify)] = new[] { "flex-start", "center", "flex-end", "space-between", "space-around", "space-evenly" },
            [typeof(FlexDirection)] = new[] { "column", "column-reverse", "row", "row-reverse" },
            [typeof(Wrap)] = new[] { "nowrap", "wrap", "wrap-reverse" },
            [typeof(Position)] = new[] { "relative", "absolute" },
            [typeof(DisplayStyle)] = new[] { "flex", "none" },
            [typeof(Visibility)] = new[] { "visible", "hidden" },
            [typeof(Overflow)] = new[] { "visible", "hidden" },
            [typeof(WhiteSpace)] = new[] { "normal", "nowrap", "pre", "pre-wrap" },
            [typeof(TextOverflow)] = new[] { "clip", "ellipsis" },
            [typeof(FontStyle)] = new[] { "normal", "bold", "italic", "bold-and-italic" },
            [typeof(TextAnchor)] = new[] { "upper-left", "upper-center", "upper-right", "middle-left", "middle-center", "middle-right", "lower-left",
                                           "lower-center", "lower-right" },
        };

        public static string Of<T>(T value) where T : struct, IConvertible => s_Words[typeof(T)][Convert.ToInt32(value)];

        public static T Parse<T>(string word) where T : struct, IConvertible
        {
            int i = Array.IndexOf(s_Words[typeof(T)], word);
            return (T)Enum.ToObject(typeof(T), i < 0 ? 0 : i);
        }
    }

    // The inline style of an element (its style attribute and what scripts set), property by property.
    public interface IStyle
    {
        StyleEnum<Align> alignContent { get; set; }
        StyleEnum<Align> alignItems { get; set; }
        StyleEnum<Align> alignSelf { get; set; }
        StyleColor backgroundColor { get; set; }
        StyleColor borderBottomColor { get; set; }
        StyleLength borderBottomLeftRadius { get; set; }
        StyleLength borderBottomRightRadius { get; set; }
        StyleFloat borderBottomWidth { get; set; }
        StyleColor borderLeftColor { get; set; }
        StyleFloat borderLeftWidth { get; set; }
        StyleColor borderRightColor { get; set; }
        StyleFloat borderRightWidth { get; set; }
        StyleColor borderTopColor { get; set; }
        StyleLength borderTopLeftRadius { get; set; }
        StyleLength borderTopRightRadius { get; set; }
        StyleFloat borderTopWidth { get; set; }
        StyleLength bottom { get; set; }
        StyleColor color { get; set; }
        StyleEnum<DisplayStyle> display { get; set; }
        StyleLength flexBasis { get; set; }
        StyleEnum<FlexDirection> flexDirection { get; set; }
        StyleFloat flexGrow { get; set; }
        StyleFloat flexShrink { get; set; }
        StyleEnum<Wrap> flexWrap { get; set; }
        StyleLength fontSize { get; set; }
        StyleLength height { get; set; }
        StyleEnum<Justify> justifyContent { get; set; }
        StyleLength left { get; set; }
        StyleLength letterSpacing { get; set; }
        StyleLength marginBottom { get; set; }
        StyleLength marginLeft { get; set; }
        StyleLength marginRight { get; set; }
        StyleLength marginTop { get; set; }
        StyleLength maxHeight { get; set; }
        StyleLength maxWidth { get; set; }
        StyleLength minHeight { get; set; }
        StyleLength minWidth { get; set; }
        StyleFloat opacity { get; set; }
        StyleEnum<Overflow> overflow { get; set; }
        StyleLength paddingBottom { get; set; }
        StyleLength paddingLeft { get; set; }
        StyleLength paddingRight { get; set; }
        StyleLength paddingTop { get; set; }
        StyleEnum<Position> position { get; set; }
        StyleLength right { get; set; }
        StyleEnum<TextOverflow> textOverflow { get; set; }
        StyleLength top { get; set; }
        StyleColor helixBackgroundImageTintColor { get; set; }
        StyleEnum<FontStyle> helixFontStyleAndWeight { get; set; }
        StyleLength helixParagraphSpacing { get; set; }
        StyleInt helixSliceBottom { get; set; }
        StyleInt helixSliceLeft { get; set; }
        StyleInt helixSliceRight { get; set; }
        StyleFloat helixSliceScale { get; set; }
        StyleInt helixSliceTop { get; set; }
        StyleEnum<TextAnchor> helixTextAlign { get; set; }
        StyleEnum<Visibility> visibility { get; set; }
        StyleEnum<WhiteSpace> whiteSpace { get; set; }
        StyleLength width { get; set; }
        StyleLength wordSpacing { get; set; }
    }

    // An element's inline style over the native one.
    internal sealed class InlineStyle : IStyle
    {
        readonly Element m_Element;

        internal InlineStyle(Element element) { m_Element = element; }

        static string Number(float v) => v.ToString(CultureInfo.InvariantCulture);

        // Null takes the property out (the sheets decide), Initial sets its initial value; true when a value is left.
        bool Keyword(string property, StyleKeyword keyword)
        {
            if (keyword == StyleKeyword.Null || keyword == StyleKeyword.Initial)
            {
                m_Element.SetInlineStyleKeyword(property, keyword == StyleKeyword.Initial ? 1 : 0);
                return false;
            }
            return true;
        }

        StyleKeyword Unset(string property)
        {
            int kind = m_Element.InlineStyleKind(property);
            return kind == -2 ? StyleKeyword.Null : kind == -1 ? StyleKeyword.Initial : StyleKeyword.Undefined;
        }

        StyleLength GetLength(string property)
        {
            StyleKeyword k = Unset(property);
            if (k != StyleKeyword.Undefined)
                return k;
            int unit = m_Element.InlineStyleUnit(property);
            if (unit == 0)
                return m_Element.InlineStyleWord(property) == "none" ? new StyleLength(Length.None()) : StyleKeyword.Auto;
            return new Length(m_Element.InlineStyleNumber(property), unit == 2 ? LengthUnit.Percent : LengthUnit.Pixel);
        }

        void SetLength(string property, StyleLength v)
        {
            if (!Keyword(property, v.keyword))
                return;
            string text = v.keyword == StyleKeyword.Auto ? "auto" : v.keyword == StyleKeyword.None ? "none" : v.value.ToString();
            m_Element.SetInlineStyleText(property, text);
        }

        StyleFloat GetFloat(string property)
        {
            StyleKeyword k = Unset(property);
            return k != StyleKeyword.Undefined ? k : m_Element.InlineStyleNumber(property);
        }

        void SetFloat(string property, StyleFloat v)
        {
            if (Keyword(property, v.keyword))
                m_Element.SetInlineStyleText(property, Number(v.value));
        }

        StyleInt GetInt(string property)
        {
            StyleKeyword k = Unset(property);
            return k != StyleKeyword.Undefined ? k : (int)m_Element.InlineStyleNumber(property);
        }

        void SetInt(string property, StyleInt v)
        {
            if (Keyword(property, v.keyword))
                m_Element.SetInlineStyleText(property, v.value.ToString(CultureInfo.InvariantCulture));
        }

        StyleColor GetColor(string property)
        {
            StyleKeyword k = Unset(property);
            return k != StyleKeyword.Undefined ? k : m_Element.InlineStyleColor(property);
        }

        void SetColor(string property, StyleColor v)
        {
            if (!Keyword(property, v.keyword))
                return;
            Color c = v.value;
            m_Element.SetInlineStyleText(property, $"rgba({Number(c.r * 255)}, {Number(c.g * 255)}, {Number(c.b * 255)}, {Number(c.a)})");
        }

        StyleEnum<T> GetEnum<T>(string property) where T : struct, IConvertible
        {
            StyleKeyword k = Unset(property);
            return k != StyleKeyword.Undefined ? k : StyleWords.Parse<T>(m_Element.InlineStyleWord(property));
        }

        void SetEnum<T>(string property, StyleEnum<T> v) where T : struct, IConvertible
        {
            if (Keyword(property, v.keyword))
                m_Element.SetInlineStyleText(property, StyleWords.Of(v.value));
        }

        public StyleEnum<Align> alignContent { get => GetEnum<Align>("align-content"); set => SetEnum("align-content", value); }
        public StyleEnum<Align> alignItems { get => GetEnum<Align>("align-items"); set => SetEnum("align-items", value); }
        public StyleEnum<Align> alignSelf { get => GetEnum<Align>("align-self"); set => SetEnum("align-self", value); }
        public StyleColor backgroundColor { get => GetColor("background-color"); set => SetColor("background-color", value); }
        public StyleColor borderBottomColor { get => GetColor("border-bottom-color"); set => SetColor("border-bottom-color", value); }
        public StyleLength borderBottomLeftRadius { get => GetLength("border-bottom-left-radius"); set => SetLength("border-bottom-left-radius", value); }
        public StyleLength borderBottomRightRadius { get => GetLength("border-bottom-right-radius"); set => SetLength("border-bottom-right-radius", value); }
        public StyleFloat borderBottomWidth { get => GetFloat("border-bottom-width"); set => SetLengthFloat("border-bottom-width", value); }
        public StyleColor borderLeftColor { get => GetColor("border-left-color"); set => SetColor("border-left-color", value); }
        public StyleFloat borderLeftWidth { get => GetFloat("border-left-width"); set => SetLengthFloat("border-left-width", value); }
        public StyleColor borderRightColor { get => GetColor("border-right-color"); set => SetColor("border-right-color", value); }
        public StyleFloat borderRightWidth { get => GetFloat("border-right-width"); set => SetLengthFloat("border-right-width", value); }
        public StyleColor borderTopColor { get => GetColor("border-top-color"); set => SetColor("border-top-color", value); }
        public StyleLength borderTopLeftRadius { get => GetLength("border-top-left-radius"); set => SetLength("border-top-left-radius", value); }
        public StyleLength borderTopRightRadius { get => GetLength("border-top-right-radius"); set => SetLength("border-top-right-radius", value); }
        public StyleFloat borderTopWidth { get => GetFloat("border-top-width"); set => SetLengthFloat("border-top-width", value); }
        public StyleLength bottom { get => GetLength("bottom"); set => SetLength("bottom", value); }
        public StyleColor color { get => GetColor("color"); set => SetColor("color", value); }
        public StyleEnum<DisplayStyle> display { get => GetEnum<DisplayStyle>("display"); set => SetEnum("display", value); }
        public StyleLength flexBasis { get => GetLength("flex-basis"); set => SetLength("flex-basis", value); }
        public StyleEnum<FlexDirection> flexDirection { get => GetEnum<FlexDirection>("flex-direction"); set => SetEnum("flex-direction", value); }
        public StyleFloat flexGrow { get => GetFloat("flex-grow"); set => SetFloat("flex-grow", value); }
        public StyleFloat flexShrink { get => GetFloat("flex-shrink"); set => SetFloat("flex-shrink", value); }
        public StyleEnum<Wrap> flexWrap { get => GetEnum<Wrap>("flex-wrap"); set => SetEnum("flex-wrap", value); }
        public StyleLength fontSize { get => GetLength("font-size"); set => SetLength("font-size", value); }
        public StyleLength height { get => GetLength("height"); set => SetLength("height", value); }
        public StyleEnum<Justify> justifyContent { get => GetEnum<Justify>("justify-content"); set => SetEnum("justify-content", value); }
        public StyleLength left { get => GetLength("left"); set => SetLength("left", value); }
        public StyleLength letterSpacing { get => GetLength("letter-spacing"); set => SetLength("letter-spacing", value); }
        public StyleLength marginBottom { get => GetLength("margin-bottom"); set => SetLength("margin-bottom", value); }
        public StyleLength marginLeft { get => GetLength("margin-left"); set => SetLength("margin-left", value); }
        public StyleLength marginRight { get => GetLength("margin-right"); set => SetLength("margin-right", value); }
        public StyleLength marginTop { get => GetLength("margin-top"); set => SetLength("margin-top", value); }
        public StyleLength maxHeight { get => GetLength("max-height"); set => SetLength("max-height", value); }
        public StyleLength maxWidth { get => GetLength("max-width"); set => SetLength("max-width", value); }
        public StyleLength minHeight { get => GetLength("min-height"); set => SetLength("min-height", value); }
        public StyleLength minWidth { get => GetLength("min-width"); set => SetLength("min-width", value); }
        public StyleFloat opacity { get => GetFloat("opacity"); set => SetFloat("opacity", value); }
        public StyleEnum<Overflow> overflow { get => GetEnum<Overflow>("overflow"); set => SetEnum("overflow", value); }
        public StyleLength paddingBottom { get => GetLength("padding-bottom"); set => SetLength("padding-bottom", value); }
        public StyleLength paddingLeft { get => GetLength("padding-left"); set => SetLength("padding-left", value); }
        public StyleLength paddingRight { get => GetLength("padding-right"); set => SetLength("padding-right", value); }
        public StyleLength paddingTop { get => GetLength("padding-top"); set => SetLength("padding-top", value); }
        public StyleEnum<Position> position { get => GetEnum<Position>("position"); set => SetEnum("position", value); }
        public StyleLength right { get => GetLength("right"); set => SetLength("right", value); }
        public StyleEnum<TextOverflow> textOverflow { get => GetEnum<TextOverflow>("text-overflow"); set => SetEnum("text-overflow", value); }
        public StyleLength top { get => GetLength("top"); set => SetLength("top", value); }
        public StyleColor helixBackgroundImageTintColor
        {
            get => GetColor("-helix-background-image-tint-color");
            set => SetColor("-helix-background-image-tint-color", value);
        }
        public StyleEnum<FontStyle> helixFontStyleAndWeight { get => GetEnum<FontStyle>("-helix-font-style"); set => SetEnum("-helix-font-style", value); }
        public StyleLength helixParagraphSpacing { get => GetLength("-helix-paragraph-spacing"); set => SetLength("-helix-paragraph-spacing", value); }
        public StyleInt helixSliceBottom { get => GetInt("-helix-slice-bottom"); set => SetInt("-helix-slice-bottom", value); }
        public StyleInt helixSliceLeft { get => GetInt("-helix-slice-left"); set => SetInt("-helix-slice-left", value); }
        public StyleInt helixSliceRight { get => GetInt("-helix-slice-right"); set => SetInt("-helix-slice-right", value); }
        public StyleFloat helixSliceScale { get => GetFloat("-helix-slice-scale"); set => SetFloat("-helix-slice-scale", value); }
        public StyleInt helixSliceTop { get => GetInt("-helix-slice-top"); set => SetInt("-helix-slice-top", value); }
        public StyleEnum<TextAnchor> helixTextAlign { get => GetEnum<TextAnchor>("-helix-text-align"); set => SetEnum("-helix-text-align", value); }
        public StyleEnum<Visibility> visibility { get => GetEnum<Visibility>("visibility"); set => SetEnum("visibility", value); }
        public StyleEnum<WhiteSpace> whiteSpace { get => GetEnum<WhiteSpace>("white-space"); set => SetEnum("white-space", value); }
        public StyleLength width { get => GetLength("width"); set => SetLength("width", value); }
        public StyleLength wordSpacing { get => GetLength("word-spacing"); set => SetLength("word-spacing", value); }

        // Border widths are lengths in HSS (the reference's are floats in pixels).
        void SetLengthFloat(string property, StyleFloat v)
        {
            if (Keyword(property, v.keyword))
                m_Element.SetInlineStyleText(property, Number(v.value) + "px");
        }
    }

    // The values of an element's style as computed and laid out.
    public interface IResolvedStyle
    {
        float width { get; }
        float height { get; }
        float left { get; }
        float top { get; }
        Color backgroundColor { get; }
        Color color { get; }
        float opacity { get; }
        float flexGrow { get; }
        float flexShrink { get; }
        float fontSize { get; }
        DisplayStyle display { get; }
        Visibility visibility { get; }
        float borderLeftWidth { get; }
        float borderTopWidth { get; }
        float borderRightWidth { get; }
        float borderBottomWidth { get; }
        float paddingLeft { get; }
        float paddingTop { get; }
        float paddingRight { get; }
        float paddingBottom { get; }
    }

    internal sealed class ResolvedStyle : IResolvedStyle
    {
        readonly Element m_Element;

        internal ResolvedStyle(Element element) { m_Element = element; }

        public float width => m_Element.layout.width;
        public float height => m_Element.layout.height;
        public float left => m_Element.layout.x;
        public float top => m_Element.layout.y;
        public Color backgroundColor => m_Element.ResolvedStyleColor("background-color");
        public Color color => m_Element.ResolvedStyleColor("color");
        public float opacity => m_Element.ResolvedStyleNumber("opacity");
        public float flexGrow => m_Element.ResolvedStyleNumber("flex-grow");
        public float flexShrink => m_Element.ResolvedStyleNumber("flex-shrink");
        public float fontSize => m_Element.ResolvedStyleNumber("font-size");
        public DisplayStyle display => StyleWords.Parse<DisplayStyle>(m_Element.ResolvedStyleWord("display"));
        public Visibility visibility => StyleWords.Parse<Visibility>(m_Element.ResolvedStyleWord("visibility"));
        public float borderLeftWidth => m_Element.ResolvedStyleNumber("border-left-width");
        public float borderTopWidth => m_Element.ResolvedStyleNumber("border-top-width");
        public float borderRightWidth => m_Element.ResolvedStyleNumber("border-right-width");
        public float borderBottomWidth => m_Element.ResolvedStyleNumber("border-bottom-width");
        public float paddingLeft => m_Element.ResolvedStyleNumber("padding-left");
        public float paddingTop => m_Element.ResolvedStyleNumber("padding-top");
        public float paddingRight => m_Element.ResolvedStyleNumber("padding-right");
        public float paddingBottom => m_Element.ResolvedStyleNumber("padding-bottom");
    }

    public partial class Element
    {
        public IStyle style => new InlineStyle(this);
        public IResolvedStyle resolvedStyle => new ResolvedStyle(this);
    }
}
