using System;
using System.Globalization;

namespace HelixEngine.UI
{
    public enum AngleUnit { Degree = 0, Gradian = 1, Radian = 2, Turn = 3 }
    public enum BackgroundPositionKeyword { Center = 0, Top = 1, Bottom = 2, Left = 3, Right = 4 }
    public enum Repeat { NoRepeat = 0, Space = 1, Round = 2, Repeat = 3 }
    public enum BackgroundSizeType { Length = 0, Cover = 1, Contain = 2 }
    public enum TextOverflowPosition { End = 0, Start = 1, Middle = 2 }
    public enum OverflowClipBox { PaddingBox = 0, ContentBox = 1 }

    public struct Angle : IEquatable<Angle>
    {
        public float value { get; set; }
        public AngleUnit unit { get; set; }

        public Angle(float value) : this(value, AngleUnit.Degree) { }

        public Angle(float value, AngleUnit unit)
        {
            this.value = value;
            this.unit = unit;
        }

        public static Angle Degrees(float value) => new Angle(value, AngleUnit.Degree);
        public static Angle Gradians(float value) => new Angle(value, AngleUnit.Gradian);
        public static Angle Radians(float value) => new Angle(value, AngleUnit.Radian);
        public static Angle Turns(float value) => new Angle(value, AngleUnit.Turn);

        public float ToDegrees() => unit switch
        {
            AngleUnit.Gradian => value * 0.9f,
            AngleUnit.Radian => value * (180f / MathF.PI),
            AngleUnit.Turn => value * 360f,
            _ => value,
        };
        public float ToGradians() => ToDegrees() / 0.9f;
        public float ToRadians() => ToDegrees() * (MathF.PI / 180f);
        public float ToTurns() => ToDegrees() / 360f;

        internal string Text => value.ToString(CultureInfo.InvariantCulture) + unit switch
        {
            AngleUnit.Gradian => "grad",
            AngleUnit.Radian => "rad",
            AngleUnit.Turn => "turn",
            _ => "deg",
        };

        public static implicit operator Angle(float value) => new Angle(value);
        public static bool operator ==(Angle lhs, Angle rhs) => lhs.Equals(rhs);
        public static bool operator !=(Angle lhs, Angle rhs) => !lhs.Equals(rhs);
        public bool Equals(Angle other) => value == other.value && unit == other.unit;
        public override bool Equals(object obj) => obj is Angle a && Equals(a);
        public override int GetHashCode() => HashCode.Combine(value, unit);
        public override string ToString() => Text;
    }

    public struct Translate : IEquatable<Translate>
    {
        public Length x { get; set; }
        public Length y { get; set; }
        public float z { get; set; }

        public Translate(Length x, Length y) : this(x, y, 0) { }

        public Translate(Length x, Length y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public static Translate None() => new Translate(new Length(0), new Length(0));
        public static implicit operator Translate(Vector3 v) => new Translate(new Length(v.x), new Length(v.y), v.z);
        public static implicit operator Translate(Vector2 v) => new Translate(new Length(v.x), new Length(v.y));
        public static bool operator ==(Translate lhs, Translate rhs) => lhs.Equals(rhs);
        public static bool operator !=(Translate lhs, Translate rhs) => !lhs.Equals(rhs);
        public bool Equals(Translate other) => x == other.x && y == other.y && z == other.z;
        public override bool Equals(object obj) => obj is Translate t && Equals(t);
        public override int GetHashCode() => HashCode.Combine(x, y, z);
        public override string ToString() => $"(x:{x}, y:{y}, z:{z.ToString(CultureInfo.InvariantCulture)})";
    }

    public struct Rotate : IEquatable<Rotate>
    {
        public Angle angle { get; set; }
        // Rotations turn about the screen's z axis (a UI's 2D rotation).
        public Vector3 axis { get; set; }

        public Rotate(Angle angle) : this(angle, new Vector3(0, 0, 1)) { }

        public Rotate(Angle angle, Vector3 axis)
        {
            this.angle = angle;
            this.axis = axis;
        }

        public Rotate(Quaternion quaternion)
        {
            quaternion.ToAngleAxis(out float degrees, out Vector3 a);
            angle = new Angle(degrees);
            axis = a;
        }

        public static Rotate None() => new Rotate(new Angle(0));
        public static implicit operator Rotate(Quaternion quaternion) => new Rotate(quaternion);
        public static bool operator ==(Rotate lhs, Rotate rhs) => lhs.Equals(rhs);
        public static bool operator !=(Rotate lhs, Rotate rhs) => !lhs.Equals(rhs);
        public bool Equals(Rotate other) => angle == other.angle && axis == other.axis;
        public override bool Equals(object obj) => obj is Rotate r && Equals(r);
        public override int GetHashCode() => HashCode.Combine(angle, axis);
        public override string ToString() => angle.ToString();
    }

    public struct Scale : IEquatable<Scale>
    {
        public Vector3 value { get; set; }

        public Scale(Vector2 scale) => value = new Vector3(scale.x, scale.y, 1);
        public Scale(Vector3 scale) => value = scale;

        public static Scale None() => new Scale(new Vector3(1, 1, 1));
        public static implicit operator Scale(Vector2 scale) => new Scale(scale);
        public static implicit operator Scale(Vector3 scale) => new Scale(scale);
        public static bool operator ==(Scale lhs, Scale rhs) => lhs.Equals(rhs);
        public static bool operator !=(Scale lhs, Scale rhs) => !lhs.Equals(rhs);
        public bool Equals(Scale other) => value == other.value;
        public override bool Equals(object obj) => obj is Scale s && Equals(s);
        public override int GetHashCode() => value.GetHashCode();
        public override string ToString() => value.ToString();
    }

    public struct TransformOrigin : IEquatable<TransformOrigin>
    {
        public Length x { get; set; }
        public Length y { get; set; }
        public float z { get; set; }

        public TransformOrigin(Length x, Length y) : this(x, y, 0) { }

        public TransformOrigin(Length x, Length y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public static TransformOrigin Initial() => new TransformOrigin(Length.Percent(50), Length.Percent(50));
        public static bool operator ==(TransformOrigin lhs, TransformOrigin rhs) => lhs.Equals(rhs);
        public static bool operator !=(TransformOrigin lhs, TransformOrigin rhs) => !lhs.Equals(rhs);
        public bool Equals(TransformOrigin other) => x == other.x && y == other.y && z == other.z;
        public override bool Equals(object obj) => obj is TransformOrigin t && Equals(t);
        public override int GetHashCode() => HashCode.Combine(x, y, z);
        public override string ToString() => $"(x:{x}, y:{y}, z:{z.ToString(CultureInfo.InvariantCulture)})";
    }

    public struct BackgroundPosition : IEquatable<BackgroundPosition>
    {
        public BackgroundPositionKeyword keyword;
        public Length offset;

        public BackgroundPosition(BackgroundPositionKeyword keyword) : this(keyword, new Length(0)) { }

        public BackgroundPosition(BackgroundPositionKeyword keyword, Length offset)
        {
            this.keyword = keyword;
            this.offset = offset;
        }

        public static bool operator ==(BackgroundPosition style1, BackgroundPosition style2) => style1.Equals(style2);
        public static bool operator !=(BackgroundPosition style1, BackgroundPosition style2) => !style1.Equals(style2);
        public bool Equals(BackgroundPosition other) => keyword == other.keyword && offset == other.offset;
        public override bool Equals(object obj) => obj is BackgroundPosition p && Equals(p);
        public override int GetHashCode() => HashCode.Combine(keyword, offset);
        public override string ToString() => $"(type:{keyword} x:{offset})";
    }

    public struct BackgroundRepeat : IEquatable<BackgroundRepeat>
    {
        public Repeat x;
        public Repeat y;

        public BackgroundRepeat(Repeat repeatX, Repeat repeatY)
        {
            x = repeatX;
            y = repeatY;
        }

        public static bool operator ==(BackgroundRepeat style1, BackgroundRepeat style2) => style1.Equals(style2);
        public static bool operator !=(BackgroundRepeat style1, BackgroundRepeat style2) => !style1.Equals(style2);
        public bool Equals(BackgroundRepeat other) => x == other.x && y == other.y;
        public override bool Equals(object obj) => obj is BackgroundRepeat r && Equals(r);
        public override int GetHashCode() => HashCode.Combine(x, y);
        public override string ToString() => $"(x:{x}, y:{y})";
    }

    public struct BackgroundSize : IEquatable<BackgroundSize>
    {
        public BackgroundSizeType sizeType { get; set; }
        public Length x { get; set; }
        public Length y { get; set; }

        public BackgroundSize(Length sizeX, Length sizeY)
        {
            sizeType = BackgroundSizeType.Length;
            x = sizeX;
            y = sizeY;
        }

        public BackgroundSize(BackgroundSizeType sizeType)
        {
            this.sizeType = sizeType;
            x = Length.Auto();
            y = Length.Auto();
        }

        public static bool operator ==(BackgroundSize style1, BackgroundSize style2) => style1.Equals(style2);
        public static bool operator !=(BackgroundSize style1, BackgroundSize style2) => !style1.Equals(style2);
        public bool Equals(BackgroundSize other) => sizeType == other.sizeType && x == other.x && y == other.y;
        public override bool Equals(object obj) => obj is BackgroundSize s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(sizeType, x, y);
        public override string ToString() => $"(sizeType:{sizeType} x:{x}, y:{y})";
    }

    public struct TextShadow : IEquatable<TextShadow>
    {
        public Vector2 offset;
        public float blurRadius;
        public Color color;

        public static bool operator ==(TextShadow style1, TextShadow style2) => style1.Equals(style2);
        public static bool operator !=(TextShadow style1, TextShadow style2) => !style1.Equals(style2);
        public bool Equals(TextShadow other) => offset == other.offset && blurRadius == other.blurRadius && color == other.color;
        public override bool Equals(object obj) => obj is TextShadow t && Equals(t);
        public override int GetHashCode() => HashCode.Combine(offset, blurRadius, color);
        public override string ToString() => $"offset={offset}, blurRadius={blurRadius.ToString(CultureInfo.InvariantCulture)}, color={color}";
    }

    public struct Cursor : IEquatable<Cursor>
    {
        public Texture2D texture { get; set; }
        public Vector2 hotspot { get; set; }

        public static bool operator ==(Cursor style1, Cursor style2) => style1.Equals(style2);
        public static bool operator !=(Cursor style1, Cursor style2) => !style1.Equals(style2);
        public bool Equals(Cursor other) => ReferenceEquals(texture, other.texture) && hotspot == other.hotspot;
        public override bool Equals(object obj) => obj is Cursor c && Equals(c);
        public override int GetHashCode() => HashCode.Combine(texture, hotspot);
        public override string ToString() => $"texture={texture}, hotspot={hotspot}";
    }

    // A background image: a texture (the other kinds of the reference's come with their assets).
    // An image of a style: a texture, or a render texture a camera draws into.
    public struct Background : IEquatable<Background>
    {
        public Texture2D texture { get; set; }
        public RenderTexture renderTexture { get; set; }

        public static Background FromTexture2D(Texture2D t) => new Background { texture = t };
        public static Background FromRenderTexture(RenderTexture rt) => new Background { renderTexture = rt };

        // The one it holds, as the style holds any texture.
        internal static Background FromTextureInternal(Texture t) => t switch
        {
            Texture2D t2 => FromTexture2D(t2),
            RenderTexture rt => FromRenderTexture(rt),
            _ => default,
        };

        public Object GetSelectedImage() => texture != null ? texture : renderTexture;
        public bool IsEmpty() => texture == null && renderTexture == null;
        internal Texture selectedTextureInternal => texture != null ? texture : renderTexture;

        public static bool operator ==(Background lhs, Background rhs) => lhs.Equals(rhs);
        public static bool operator !=(Background lhs, Background rhs) => !lhs.Equals(rhs);
        public static implicit operator Background(Texture2D v) => FromTexture2D(v);
        public bool Equals(Background other) => ReferenceEquals(texture, other.texture) && ReferenceEquals(renderTexture, other.renderTexture);
        public override bool Equals(object obj) => obj is Background b && Equals(b);
        public override int GetHashCode() => GetSelectedImage() is Object o ? o.GetHashCode() : 0;
        public override string ToString() => GetSelectedImage()?.ToString() ?? "";
    }

    // ------------------------------------------------------------------------------------------- their style values

    public struct StyleTranslate : IStyleValue<Translate>, IEquatable<StyleTranslate>
    {
        public Translate value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleTranslate(Translate v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleTranslate(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleTranslate(StyleKeyword keyword) => new StyleTranslate(keyword);
        public static implicit operator StyleTranslate(Translate v) => new StyleTranslate(v);
        public static bool operator ==(StyleTranslate lhs, StyleTranslate rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleTranslate lhs, StyleTranslate rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleTranslate other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleTranslate s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleRotate : IStyleValue<Rotate>, IEquatable<StyleRotate>
    {
        public Rotate value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleRotate(Rotate v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleRotate(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleRotate(StyleKeyword keyword) => new StyleRotate(keyword);
        public static implicit operator StyleRotate(Rotate v) => new StyleRotate(v);
        public static bool operator ==(StyleRotate lhs, StyleRotate rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleRotate lhs, StyleRotate rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleRotate other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleRotate s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleScale : IStyleValue<Scale>, IEquatable<StyleScale>
    {
        public Scale value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleScale(Scale v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleScale(Vector2 v) : this(new Scale(v)) { }

        public StyleScale(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleScale(StyleKeyword keyword) => new StyleScale(keyword);
        public static implicit operator StyleScale(Vector2 v) => new StyleScale(v);
        public static implicit operator StyleScale(Scale v) => new StyleScale(v);
        public static bool operator ==(StyleScale lhs, StyleScale rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleScale lhs, StyleScale rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleScale other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleScale s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleTransformOrigin : IStyleValue<TransformOrigin>, IEquatable<StyleTransformOrigin>
    {
        public TransformOrigin value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleTransformOrigin(TransformOrigin v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleTransformOrigin(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleTransformOrigin(StyleKeyword keyword) => new StyleTransformOrigin(keyword);
        public static implicit operator StyleTransformOrigin(TransformOrigin v) => new StyleTransformOrigin(v);
        public static bool operator ==(StyleTransformOrigin lhs, StyleTransformOrigin rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleTransformOrigin lhs, StyleTransformOrigin rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleTransformOrigin other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleTransformOrigin s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleBackgroundPosition : IStyleValue<BackgroundPosition>, IEquatable<StyleBackgroundPosition>
    {
        public BackgroundPosition value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleBackgroundPosition(BackgroundPosition v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleBackgroundPosition(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleBackgroundPosition(StyleKeyword keyword) => new StyleBackgroundPosition(keyword);
        public static implicit operator StyleBackgroundPosition(BackgroundPosition v) => new StyleBackgroundPosition(v);
        public static bool operator ==(StyleBackgroundPosition lhs, StyleBackgroundPosition rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleBackgroundPosition lhs, StyleBackgroundPosition rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleBackgroundPosition other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleBackgroundPosition s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleBackgroundRepeat : IStyleValue<BackgroundRepeat>, IEquatable<StyleBackgroundRepeat>
    {
        public BackgroundRepeat value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleBackgroundRepeat(BackgroundRepeat v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleBackgroundRepeat(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleBackgroundRepeat(StyleKeyword keyword) => new StyleBackgroundRepeat(keyword);
        public static implicit operator StyleBackgroundRepeat(BackgroundRepeat v) => new StyleBackgroundRepeat(v);
        public static bool operator ==(StyleBackgroundRepeat lhs, StyleBackgroundRepeat rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleBackgroundRepeat lhs, StyleBackgroundRepeat rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleBackgroundRepeat other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleBackgroundRepeat s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleBackgroundSize : IStyleValue<BackgroundSize>, IEquatable<StyleBackgroundSize>
    {
        public BackgroundSize value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleBackgroundSize(BackgroundSize v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleBackgroundSize(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleBackgroundSize(StyleKeyword keyword) => new StyleBackgroundSize(keyword);
        public static implicit operator StyleBackgroundSize(BackgroundSize v) => new StyleBackgroundSize(v);
        public static bool operator ==(StyleBackgroundSize lhs, StyleBackgroundSize rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleBackgroundSize lhs, StyleBackgroundSize rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleBackgroundSize other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleBackgroundSize s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleTextShadow : IStyleValue<TextShadow>, IEquatable<StyleTextShadow>
    {
        public TextShadow value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleTextShadow(TextShadow v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleTextShadow(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleTextShadow(StyleKeyword keyword) => new StyleTextShadow(keyword);
        public static implicit operator StyleTextShadow(TextShadow v) => new StyleTextShadow(v);
        public static bool operator ==(StyleTextShadow lhs, StyleTextShadow rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleTextShadow lhs, StyleTextShadow rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleTextShadow other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleTextShadow s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleCursor : IStyleValue<Cursor>, IEquatable<StyleCursor>
    {
        public Cursor value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleCursor(Cursor v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleCursor(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleCursor(StyleKeyword keyword) => new StyleCursor(keyword);
        public static implicit operator StyleCursor(Cursor v) => new StyleCursor(v);
        public static bool operator ==(StyleCursor lhs, StyleCursor rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleCursor lhs, StyleCursor rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleCursor other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleCursor s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    public struct StyleBackground : IStyleValue<Background>, IEquatable<StyleBackground>
    {
        public Background value { get; set; }
        public StyleKeyword keyword { get; set; }

        public StyleBackground(Background v)
        {
            value = v;
            keyword = StyleKeyword.Undefined;
        }

        public StyleBackground(Texture2D v) : this(Background.FromTexture2D(v)) { }
        public StyleBackground(RenderTexture v) : this(Background.FromRenderTexture(v)) { }

        public StyleBackground(StyleKeyword keyword)
        {
            value = default;
            this.keyword = keyword;
        }

        public static implicit operator StyleBackground(StyleKeyword keyword) => new StyleBackground(keyword);
        public static implicit operator StyleBackground(Background v) => new StyleBackground(v);
        public static implicit operator StyleBackground(Texture2D v) => new StyleBackground(v);
        public static bool operator ==(StyleBackground lhs, StyleBackground rhs) => lhs.Equals(rhs);
        public static bool operator !=(StyleBackground lhs, StyleBackground rhs) => !lhs.Equals(rhs);
        public bool Equals(StyleBackground other) => keyword == other.keyword && value == other.value;
        public override bool Equals(object obj) => obj is StyleBackground s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(value, keyword);
        public override string ToString() => keyword == StyleKeyword.Undefined ? value.ToString() : keyword.ToString();
    }

    // ------------------------------------------------------------------------------------------- custom properties

    public struct CustomStyleProperty<T> : IEquatable<CustomStyleProperty<T>>
    {
        public string name { get; }

        public CustomStyleProperty(string propertyName)
        {
            if (string.IsNullOrEmpty(propertyName) || !propertyName.StartsWith("--", StringComparison.Ordinal))
                throw new ArgumentException($"Custom style property \"{propertyName}\" must start with \"--\" prefix.");
            name = propertyName;
        }

        public static bool operator ==(CustomStyleProperty<T> a, CustomStyleProperty<T> b) => a.Equals(b);
        public static bool operator !=(CustomStyleProperty<T> a, CustomStyleProperty<T> b) => !a.Equals(b);
        public bool Equals(CustomStyleProperty<T> other) => name == other.name;
        public override bool Equals(object obj) => obj is CustomStyleProperty<T> p && Equals(p);
        public override int GetHashCode() => name != null ? name.GetHashCode() : 0;
    }

    // The custom properties (--name) an element's rules and inline style declare, their var() replaced.
    public interface ICustomStyle
    {
        bool TryGetValue(CustomStyleProperty<float> property, out float value);
        bool TryGetValue(CustomStyleProperty<int> property, out int value);
        bool TryGetValue(CustomStyleProperty<bool> property, out bool value);
        bool TryGetValue(CustomStyleProperty<Color> property, out Color value);
        bool TryGetValue(CustomStyleProperty<Texture2D> property, out Texture2D value);
        bool TryGetValue(CustomStyleProperty<string> property, out string value);
    }

    internal sealed class CustomStyle : ICustomStyle
    {
        readonly Element m_Element;

        internal CustomStyle(Element element) { m_Element = element; }

        bool Text(string name, out string text)
        {
            text = null;
            if (name == null || !m_Element.HasCustomStyle(name))
                return false;
            text = m_Element.CustomStyleText(name);
            return true;
        }

        // A number, or a dimension (its unit dropped).
        static bool Number(string text, out float value)
        {
            int end = 0;
            while (end < text.Length && (char.IsDigit(text[end]) || text[end] == '.' || text[end] == '-' || text[end] == '+' || text[end] == 'e' || text[end] == 'E'))
                end++;
            return float.TryParse(text.Substring(0, end), NumberStyles.Float, CultureInfo.InvariantCulture, out value);
        }

        public bool TryGetValue(CustomStyleProperty<float> property, out float value)
        {
            value = 0;
            return Text(property.name, out string t) && Number(t, out value);
        }

        public bool TryGetValue(CustomStyleProperty<int> property, out int value)
        {
            value = 0;
            if (!Text(property.name, out string t) || !Number(t, out float f))
                return false;
            value = (int)f;
            return true;
        }

        public bool TryGetValue(CustomStyleProperty<bool> property, out bool value)
        {
            value = false;
            if (!Text(property.name, out string t))
                return false;
            if (t == "true" || t == "false")
            {
                value = t == "true";
                return true;
            }
            return false;
        }

        public bool TryGetValue(CustomStyleProperty<Color> property, out Color value)
        {
            value = default;
            return Text(property.name, out string t) && StyleText.TryColor(t, out value);
        }

        public bool TryGetValue(CustomStyleProperty<Texture2D> property, out Texture2D value)
        {
            value = property.name != null ? m_Element.CustomStyleTexture(property.name) : null;
            return value != null;
        }

        public bool TryGetValue(CustomStyleProperty<string> property, out string value)
        {
            if (!Text(property.name, out value))
                return false;
            // A quoted string without its quotes.
            if (value.Length >= 2 && (value[0] == '"' || value[0] == '\'') && value[value.Length - 1] == value[0])
                value = value.Substring(1, value.Length - 2);
            return true;
        }
    }

    // Colors of HSS text (custom properties hold text).
    static class StyleText
    {
        static readonly (string name, uint rgb)[] s_Names =
        {
            ("black", 0x000000), ("white", 0xFFFFFF), ("red", 0xFF0000), ("green", 0x008000), ("lime", 0x00FF00), ("blue", 0x0000FF),
            ("yellow", 0xFFFF00), ("cyan", 0x00FFFF), ("aqua", 0x00FFFF), ("magenta", 0xFF00FF), ("fuchsia", 0xFF00FF), ("gray", 0x808080),
            ("grey", 0x808080), ("silver", 0xC0C0C0), ("maroon", 0x800000), ("olive", 0x808000), ("navy", 0x000080), ("purple", 0x800080),
            ("teal", 0x008080), ("orange", 0xFFA500), ("pink", 0xFFC0CB), ("brown", 0xA52A2A), ("gold", 0xFFD700), ("indigo", 0x4B0082),
            ("violet", 0xEE82EE),
        };

        public static bool TryColor(string text, out Color color)
        {
            color = default;
            text = text.Trim();
            if (text == "transparent")
            {
                color = new Color(0, 0, 0, 0);
                return true;
            }
            foreach (var (name, rgb) in s_Names)
                if (name == text)
                {
                    color = new Color(((rgb >> 16) & 255) / 255f, ((rgb >> 8) & 255) / 255f, (rgb & 255) / 255f, 1);
                    return true;
                }
            if (text.StartsWith("#", StringComparison.Ordinal))
            {
                string h = text.Substring(1);
                if (!uint.TryParse(h, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint v))
                    return false;
                switch (h.Length)
                {
                case 3: color = new Color(((v >> 8) & 15) * 17 / 255f, ((v >> 4) & 15) * 17 / 255f, (v & 15) * 17 / 255f, 1); return true;
                case 4: color = new Color(((v >> 12) & 15) * 17 / 255f, ((v >> 8) & 15) * 17 / 255f, ((v >> 4) & 15) * 17 / 255f, (v & 15) * 17 / 255f); return true;
                case 6: color = new Color(((v >> 16) & 255) / 255f, ((v >> 8) & 255) / 255f, (v & 255) / 255f, 1); return true;
                case 8: color = new Color(((v >> 24) & 255) / 255f, ((v >> 16) & 255) / 255f, ((v >> 8) & 255) / 255f, (v & 255) / 255f); return true;
                default: return false;
                }
            }
            if (text.StartsWith("rgb", StringComparison.Ordinal))
            {
                int open = text.IndexOf('('), close = text.IndexOf(')');
                if (open < 0 || close < open)
                    return false;
                string[] parts = text.Substring(open + 1, close - open - 1).Split(',');
                if (parts.Length < 3)
                    return false;
                float[] c = { 0, 0, 0, 1 };
                for (int i = 0; i < parts.Length && i < 4; i++)
                {
                    string p = parts[i].Trim();
                    bool percent = p.EndsWith("%", StringComparison.Ordinal);
                    if (!float.TryParse(percent ? p.TrimEnd('%') : p, NumberStyles.Float, CultureInfo.InvariantCulture, out float x))
                        return false;
                    c[i] = i < 3 ? (percent ? x / 100f : x / 255f) : (percent ? x / 100f : x);
                }
                color = new Color(c[0], c[1], c[2], c[3]);
                return true;
            }
            return false;
        }
    }

    public sealed class CustomStyleResolvedEvent : EventBase<CustomStyleResolvedEvent>
    {
        public ICustomStyle customStyle => (target as Element)?.customStyle;
    }

    public partial class Element
    {
        public ICustomStyle customStyle => new CustomStyle(this);
    }
}
