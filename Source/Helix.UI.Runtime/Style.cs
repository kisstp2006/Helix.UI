using System;
using System.Collections.Generic;
using System.Globalization;

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
            [typeof(TextOverflowPosition)] = new[] { "end", "start", "middle" },
            [typeof(OverflowClipBox)] = new[] { "padding-box", "content-box" },
            [typeof(Repeat)] = new[] { "no-repeat", "space", "round", "repeat" },
            [typeof(BackgroundPositionKeyword)] = new[] { "center", "top", "bottom", "left", "right" },
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
        StyleTranslate translate { get; set; }
        StyleRotate rotate { get; set; }
        StyleScale scale { get; set; }
        StyleTransformOrigin transformOrigin { get; set; }
        StyleBackground backgroundImage { get; set; }
        StyleBackgroundPosition backgroundPositionX { get; set; }
        StyleBackgroundPosition backgroundPositionY { get; set; }
        StyleBackgroundRepeat backgroundRepeat { get; set; }
        StyleBackgroundSize backgroundSize { get; set; }
        StyleTextShadow textShadow { get; set; }
        StyleCursor cursor { get; set; }
        StyleColor helixTextOutlineColor { get; set; }
        StyleFloat helixTextOutlineWidth { get; set; }
        StyleEnum<TextOverflowPosition> helixTextOverflowPosition { get; set; }
        StyleEnum<OverflowClipBox> helixOverflowClipBox { get; set; }
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

        // ------------------------------------------------------------------------- properties of several values

        Length LengthAt(string property, int index)
        {
            int unit = m_Element.InlineStyleUnitAt(property, index);
            float v = m_Element.InlineStyleNumberAt(property, index);
            return unit == 0 ? Length.Auto() : new Length(v, unit == 2 ? LengthUnit.Percent : LengthUnit.Pixel);
        }

        static string Rgba(Color c) => $"rgba({Number(c.r * 255)}, {Number(c.g * 255)}, {Number(c.b * 255)}, {Number(c.a)})";

        public StyleTranslate translate
        {
            get
            {
                StyleKeyword k = Unset("translate");
                return k != StyleKeyword.Undefined ? k : new Translate(LengthAt("translate", 0), LengthAt("translate", 1));
            }
            set
            {
                if (Keyword("translate", value.keyword))
                    m_Element.SetInlineStyleText("translate", value.keyword == StyleKeyword.None ? "none" : $"{value.value.x} {value.value.y}");
            }
        }

        public StyleRotate rotate
        {
            get
            {
                StyleKeyword k = Unset("rotate");
                return k != StyleKeyword.Undefined ? k : new Rotate(new Angle(m_Element.InlineStyleNumberAt("rotate", 0)));
            }
            set
            {
                if (Keyword("rotate", value.keyword))
                    m_Element.SetInlineStyleText("rotate", value.keyword == StyleKeyword.None ? "none" : value.value.angle.Text);
            }
        }

        public StyleScale scale
        {
            get
            {
                StyleKeyword k = Unset("scale");
                return k != StyleKeyword.Undefined ? k : new Scale(new Vector2(m_Element.InlineStyleNumberAt("scale", 0), m_Element.InlineStyleNumberAt("scale", 1)));
            }
            set
            {
                if (Keyword("scale", value.keyword))
                    m_Element.SetInlineStyleText("scale", value.keyword == StyleKeyword.None ? "none" : $"{Number(value.value.value.x)} {Number(value.value.value.y)}");
            }
        }

        public StyleTransformOrigin transformOrigin
        {
            get
            {
                StyleKeyword k = Unset("transform-origin");
                return k != StyleKeyword.Undefined ? k : new TransformOrigin(LengthAt("transform-origin", 0), LengthAt("transform-origin", 1));
            }
            set
            {
                if (Keyword("transform-origin", value.keyword))
                    m_Element.SetInlineStyleText("transform-origin", $"{value.value.x} {value.value.y}");
            }
        }

        public StyleBackground backgroundImage
        {
            get
            {
                StyleKeyword k = Unset("background-image");
                return k != StyleKeyword.Undefined ? k : new StyleBackground(m_Element.InlineStyleTexture("background-image"));
            }
            set
            {
                if (Keyword("background-image", value.keyword))
                    m_Element.SetInlineStyleTexture("background-image", value.keyword == StyleKeyword.None ? null : value.value.texture, default);
            }
        }

        StyleBackgroundPosition GetPosition(string property)
        {
            StyleKeyword k = Unset(property);
            if (k != StyleKeyword.Undefined)
                return k;
            return new BackgroundPosition(StyleWords.Parse<BackgroundPositionKeyword>(m_Element.InlineStyleWordAt(property, 0)), LengthAt(property, 1));
        }

        void SetPosition(string property, StyleBackgroundPosition v)
        {
            if (Keyword(property, v.keyword))
                m_Element.SetInlineStyleText(property, $"{StyleWords.Of(v.value.keyword)} {v.value.offset}");
        }

        public StyleBackgroundPosition backgroundPositionX { get => GetPosition("background-position-x"); set => SetPosition("background-position-x", value); }
        public StyleBackgroundPosition backgroundPositionY { get => GetPosition("background-position-y"); set => SetPosition("background-position-y", value); }

        public StyleBackgroundRepeat backgroundRepeat
        {
            get
            {
                StyleKeyword k = Unset("background-repeat");
                if (k != StyleKeyword.Undefined)
                    return k;
                int n = m_Element.InlineStyleCount("background-repeat");
                Repeat x = StyleWords.Parse<Repeat>(m_Element.InlineStyleWordAt("background-repeat", 0));
                return new BackgroundRepeat(x, n > 1 ? StyleWords.Parse<Repeat>(m_Element.InlineStyleWordAt("background-repeat", 1)) : x);
            }
            set
            {
                if (Keyword("background-repeat", value.keyword))
                    m_Element.SetInlineStyleText("background-repeat", $"{StyleWords.Of(value.value.x)} {StyleWords.Of(value.value.y)}");
            }
        }

        public StyleBackgroundSize backgroundSize
        {
            get
            {
                StyleKeyword k = Unset("background-size");
                if (k != StyleKeyword.Undefined)
                    return k;
                string type = m_Element.InlineStyleWordAt("background-size", 0);
                if (type == "cover" || type == "contain")
                    return new BackgroundSize(type == "cover" ? BackgroundSizeType.Cover : BackgroundSizeType.Contain);
                return new BackgroundSize(LengthAt("background-size", 1), LengthAt("background-size", 2));
            }
            set
            {
                if (!Keyword("background-size", value.keyword))
                    return;
                BackgroundSize b = value.value;
                string text = b.sizeType == BackgroundSizeType.Cover ? "cover" : b.sizeType == BackgroundSizeType.Contain ? "contain" : $"{b.x} {b.y}";
                m_Element.SetInlineStyleText("background-size", text);
            }
        }

        public StyleTextShadow textShadow
        {
            get
            {
                StyleKeyword k = Unset("text-shadow");
                if (k != StyleKeyword.Undefined)
                    return k;
                return new TextShadow
                {
                    offset = new Vector2(m_Element.InlineStyleNumberAt("text-shadow", 0), m_Element.InlineStyleNumberAt("text-shadow", 1)),
                    blurRadius = m_Element.InlineStyleNumberAt("text-shadow", 2),
                    color = m_Element.InlineStyleColorAt("text-shadow", 3),
                };
            }
            set
            {
                if (!Keyword("text-shadow", value.keyword))
                    return;
                TextShadow t = value.value;
                m_Element.SetInlineStyleText("text-shadow", $"{Number(t.offset.x)}px {Number(t.offset.y)}px {Number(t.blurRadius)}px {Rgba(t.color)}");
            }
        }

        public StyleCursor cursor
        {
            get
            {
                StyleKeyword k = Unset("cursor");
                if (k != StyleKeyword.Undefined)
                    return k;
                return new Cursor
                {
                    texture = m_Element.InlineStyleTexture("cursor"),
                    hotspot = new Vector2(m_Element.InlineStyleNumberAt("cursor", 1), m_Element.InlineStyleNumberAt("cursor", 2)),
                };
            }
            set
            {
                if (Keyword("cursor", value.keyword))
                    m_Element.SetInlineStyleTexture("cursor", value.value.texture, value.value.hotspot);
            }
        }

        public StyleColor helixTextOutlineColor { get => GetColor("-helix-text-outline-color"); set => SetColor("-helix-text-outline-color", value); }
        public StyleFloat helixTextOutlineWidth { get => GetFloat("-helix-text-outline-width"); set => SetLengthFloat("-helix-text-outline-width", value); }
        public StyleEnum<TextOverflowPosition> helixTextOverflowPosition
        {
            get => GetEnum<TextOverflowPosition>("-helix-text-overflow-position");
            set => SetEnum("-helix-text-overflow-position", value);
        }
        public StyleEnum<OverflowClipBox> helixOverflowClipBox
        {
            get => GetEnum<OverflowClipBox>("-helix-overflow-clip-box");
            set => SetEnum("-helix-overflow-clip-box", value);
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
        Vector3 translate { get; }
        Rotate rotate { get; }
        Scale scale { get; }
        Vector3 transformOrigin { get; }
        Background backgroundImage { get; }
        BackgroundPosition backgroundPositionX { get; }
        BackgroundPosition backgroundPositionY { get; }
        BackgroundRepeat backgroundRepeat { get; }
        BackgroundSize backgroundSize { get; }
        TextShadow textShadow { get; }
        Color helixTextOutlineColor { get; }
        float helixTextOutlineWidth { get; }
        TextOverflowPosition helixTextOverflowPosition { get; }
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

        // A computed length in pixels (percentages of a size).
        float Pixels(string property, int index, float size)
        {
            float v = m_Element.ResolvedStyleNumberAt(property, index);
            int unit = m_Element.ResolvedStyleUnitAt(property, index);
            return unit == 2 ? v * size / 100f : unit == 1 ? v : 0;
        }

        Length LengthAt(string property, int index)
        {
            int unit = m_Element.ResolvedStyleUnitAt(property, index);
            float v = m_Element.ResolvedStyleNumberAt(property, index);
            return unit == 0 ? Length.Auto() : new Length(v, unit == 2 ? LengthUnit.Percent : LengthUnit.Pixel);
        }

        BackgroundPosition PositionOf(string property) =>
            new BackgroundPosition(StyleWords.Parse<BackgroundPositionKeyword>(m_Element.ResolvedStyleWordAt(property, 0)), LengthAt(property, 1));

        public Vector3 translate
        {
            get
            {
                Rect r = m_Element.layout;
                return new Vector3(Pixels("translate", 0, r.width), Pixels("translate", 1, r.height), 0);
            }
        }
        public Rotate rotate => new Rotate(new Angle(m_Element.ResolvedStyleNumberAt("rotate", 0)));
        public Scale scale => new Scale(new Vector2(m_Element.ResolvedStyleNumberAt("scale", 0), m_Element.ResolvedStyleNumberAt("scale", 1)));
        public Vector3 transformOrigin
        {
            get
            {
                Rect r = m_Element.layout;
                return new Vector3(Pixels("transform-origin", 0, r.width), Pixels("transform-origin", 1, r.height), 0);
            }
        }
        public Background backgroundImage => Background.FromTexture2D(m_Element.ResolvedStyleTexture("background-image"));
        public BackgroundPosition backgroundPositionX => PositionOf("background-position-x");
        public BackgroundPosition backgroundPositionY => PositionOf("background-position-y");
        public BackgroundRepeat backgroundRepeat => new BackgroundRepeat(StyleWords.Parse<Repeat>(m_Element.ResolvedStyleWordAt("background-repeat", 0)),
                                                                         StyleWords.Parse<Repeat>(m_Element.ResolvedStyleWordAt("background-repeat", 1)));
        public BackgroundSize backgroundSize
        {
            get
            {
                string type = m_Element.ResolvedStyleWordAt("background-size", 0);
                if (type == "cover" || type == "contain")
                    return new BackgroundSize(type == "cover" ? BackgroundSizeType.Cover : BackgroundSizeType.Contain);
                return new BackgroundSize(LengthAt("background-size", 1), LengthAt("background-size", 2));
            }
        }
        public TextShadow textShadow => new TextShadow
        {
            offset = new Vector2(m_Element.ResolvedStyleNumberAt("text-shadow", 0), m_Element.ResolvedStyleNumberAt("text-shadow", 1)),
            blurRadius = m_Element.ResolvedStyleNumberAt("text-shadow", 2),
            color = m_Element.ResolvedStyleColorAt("text-shadow", 3),
        };
        public Color helixTextOutlineColor => m_Element.ResolvedStyleColor("-helix-text-outline-color");
        public float helixTextOutlineWidth => m_Element.ResolvedStyleNumber("-helix-text-outline-width");
        public TextOverflowPosition helixTextOverflowPosition =>
            StyleWords.Parse<TextOverflowPosition>(m_Element.ResolvedStyleWord("-helix-text-overflow-position"));
    }

    public partial class Element
    {
        public IStyle style => new InlineStyle(this);
        public IResolvedStyle resolvedStyle => new ResolvedStyle(this);
    }
}
