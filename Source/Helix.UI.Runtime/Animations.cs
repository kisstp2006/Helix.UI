using System;

namespace HelixEngine.UI.Experimental
{
    // The easing curves (t from 0 to 1).
    public static class Easing
    {
        const float HalfPi = MathF.PI / 2;

        public static float Step(float t) => t < 0.5f ? 0 : 1;
        public static float Linear(float t) => t;
        public static float InSine(float t) => MathF.Sin(HalfPi * (t - 1)) + 1;
        public static float OutSine(float t) => MathF.Sin(t * HalfPi);
        public static float InOutSine(float t) => (MathF.Sin(MathF.PI * (t - 0.5f)) + 1) * 0.5f;
        public static float InQuad(float t) => t * t;
        public static float OutQuad(float t) => t * (2 - t);
        public static float InOutQuad(float t)
        {
            t *= 2;
            return t < 1 ? t * t * 0.5f : -0.5f * ((t - 1) * (t - 3) - 1);
        }
        public static float InCubic(float t) => InPower(t, 3);
        public static float OutCubic(float t) => OutPower(t, 3);
        public static float InOutCubic(float t) => InOutPower(t, 3);
        public static float InPower(float t, int power) => MathF.Pow(t, power);
        public static float OutPower(float t, int power)
        {
            int sign = power % 2 == 0 ? -1 : 1;
            return sign * (MathF.Pow(t - 1, power) + sign);
        }
        public static float InOutPower(float t, int power)
        {
            t *= 2;
            if (t < 1)
                return InPower(t, power) * 0.5f;
            int sign = power % 2 == 0 ? -1 : 1;
            return sign * 0.5f * (MathF.Pow(t - 2, power) + sign * 2);
        }
        public static float InBounce(float t) => 1 - OutBounce(1 - t);
        public static float OutBounce(float t)
        {
            if (t < 4 / 11f)
                return 121 * t * t / 16;
            if (t < 8 / 11f)
                return 363 / 40f * t * t - 99 / 10f * t + 17 / 5f;
            if (t < 9 / 10f)
                return 4356 / 361f * t * t - 35442 / 1805f * t + 16061 / 1805f;
            return 54 / 5f * t * t - 513 / 25f * t + 268 / 25f;
        }
        public static float InOutBounce(float t) => t < 0.5f ? InBounce(t * 2) * 0.5f : OutBounce(t * 2 - 1) * 0.5f + 0.5f;
        public static float InElastic(float t)
        {
            if (t == 0 || t == 1)
                return t;
            const float p = 0.3f, s = p / 4;
            float power = MathF.Pow(2, 10 * (t - 1));
            return -(power * MathF.Sin((t - 1 - s) * 2 * MathF.PI / p));
        }
        public static float OutElastic(float t)
        {
            if (t == 0 || t == 1)
                return t;
            const float p = 0.3f, s = p / 4;
            return MathF.Pow(2, -10 * t) * MathF.Sin((t - s) * 2 * MathF.PI / p) + 1;
        }
        public static float InOutElastic(float t) => t < 0.5f ? InElastic(t * 2) * 0.5f : OutElastic(t * 2 - 1) * 0.5f + 0.5f;
        public static float InBack(float t)
        {
            const float s = 1.70158f;
            return t * t * ((s + 1) * t - s);
        }
        public static float OutBack(float t) => 1 - InBack(1 - t);
        public static float InOutBack(float t) => t < 0.5f ? InBack(t * 2) * 0.5f : OutBack(t * 2 - 1) * 0.5f + 0.5f;
        public static float InCirc(float t) => -(MathF.Sqrt(1 - t * t) - 1);
        public static float OutCirc(float t)
        {
            t -= 1;
            return MathF.Sqrt(1 - t * t);
        }
        public static float InOutCirc(float t)
        {
            t *= 2;
            if (t < 1)
                return -0.5f * (MathF.Sqrt(1 - t * t) - 1);
            t -= 2;
            return 0.5f * (MathF.Sqrt(1 - t * t) + 1);
        }
    }

    public interface IValueAnimation
    {
        void Start();
        void Stop();
        void Recycle();
        bool isRunning { get; }
        int durationMs { get; set; }
    }

    // A value going from `from` to `to` over durationMs on its owner's schedule: each frame interpolated at the eased
    // time and given to valueUpdated.
    public sealed class ValueAnimation<T> : IValueAnimation
    {
        Element m_Owner;
        IElementScheduledItem m_Item;
        long m_StartedAt;
        bool m_FromSet, m_ToSet;
        T m_From, m_To;

        public int durationMs { get; set; } = 400;
        public Func<float, float> easingCurve { get; set; } = Easing.OutQuad;
        public bool isRunning { get; private set; }
        public Action onAnimationCompleted { get; set; }
        public bool autoRecycle { get; set; } = true;
        public Action<Element, T> valueUpdated { get; set; }
        public Func<Element, T> initialValue { get; set; }
        public Func<T, T, float, T> interpolator { get; set; }

        public T from
        {
            get => m_From;
            set
            {
                m_From = value;
                m_FromSet = true;
            }
        }

        public T to
        {
            get => m_To;
            set
            {
                m_To = value;
                m_ToSet = true;
            }
        }

        public static ValueAnimation<T> Create(Element e, Func<T, T, float, T> interpolator) => new ValueAnimation<T> { m_Owner = e, interpolator = interpolator };

        public ValueAnimation<T> Ease(Func<float, float> easing)
        {
            easingCurve = easing;
            return this;
        }

        public ValueAnimation<T> OnCompleted(Action callback)
        {
            onAnimationCompleted = callback;
            return this;
        }

        public ValueAnimation<T> KeepAlive()
        {
            autoRecycle = false;
            return this;
        }

        public void Start()
        {
            if (m_Owner == null)
                return;
            if (!m_FromSet && initialValue != null)
                from = initialValue(m_Owner);
            if (!m_ToSet)
                to = from;
            Stop();
            isRunning = true;
            m_StartedAt = -1;
            m_Item = m_Owner.schedule.Execute(Tick).Every(0);
        }

        void Tick(TimerState state)
        {
            if (!isRunning)
                return;
            if (m_StartedAt < 0)
                m_StartedAt = state.now;
            float t = durationMs <= 0 ? 1 : Math.Clamp((state.now - m_StartedAt) / (float)durationMs, 0, 1);
            float k = easingCurve != null ? easingCurve(t) : t;
            if (interpolator != null)
                valueUpdated?.Invoke(m_Owner, interpolator(from, to, k));
            if (t >= 1)
            {
                Stop();
                onAnimationCompleted?.Invoke();
                if (autoRecycle)
                    Recycle();
            }
        }

        public void Stop()
        {
            isRunning = false;
            m_Item?.Pause();
            m_Item = null;
        }

        public void Recycle()
        {
            Stop();
            valueUpdated = null;
            onAnimationCompleted = null;
        }
    }

    // The values of style properties an element's animation goes between (the ones set).
    public struct StyleValues
    {
        internal float? m_Top, m_Left, m_Width, m_Height, m_Right, m_Bottom, m_Opacity, m_FlexGrow, m_FlexShrink;
        internal float? m_MarginLeft, m_MarginTop, m_MarginRight, m_MarginBottom, m_PaddingLeft, m_PaddingTop, m_PaddingRight, m_PaddingBottom;
        internal float? m_BorderLeftWidth, m_BorderRightWidth, m_BorderTopWidth, m_BorderBottomWidth, m_BorderRadius, m_FontSize;
        internal Color? m_Color, m_BackgroundColor, m_BorderColor, m_HelixBackgroundImageTintColor;

        public float top { get => m_Top ?? 0; set => m_Top = value; }
        public float left { get => m_Left ?? 0; set => m_Left = value; }
        public float width { get => m_Width ?? 0; set => m_Width = value; }
        public float height { get => m_Height ?? 0; set => m_Height = value; }
        public float right { get => m_Right ?? 0; set => m_Right = value; }
        public float bottom { get => m_Bottom ?? 0; set => m_Bottom = value; }
        public float opacity { get => m_Opacity ?? 1; set => m_Opacity = value; }
        public float flexGrow { get => m_FlexGrow ?? 0; set => m_FlexGrow = value; }
        public float flexShrink { get => m_FlexShrink ?? 0; set => m_FlexShrink = value; }
        public float marginLeft { get => m_MarginLeft ?? 0; set => m_MarginLeft = value; }
        public float marginTop { get => m_MarginTop ?? 0; set => m_MarginTop = value; }
        public float marginRight { get => m_MarginRight ?? 0; set => m_MarginRight = value; }
        public float marginBottom { get => m_MarginBottom ?? 0; set => m_MarginBottom = value; }
        public float paddingLeft { get => m_PaddingLeft ?? 0; set => m_PaddingLeft = value; }
        public float paddingTop { get => m_PaddingTop ?? 0; set => m_PaddingTop = value; }
        public float paddingRight { get => m_PaddingRight ?? 0; set => m_PaddingRight = value; }
        public float paddingBottom { get => m_PaddingBottom ?? 0; set => m_PaddingBottom = value; }
        public float borderLeftWidth { get => m_BorderLeftWidth ?? 0; set => m_BorderLeftWidth = value; }
        public float borderRightWidth { get => m_BorderRightWidth ?? 0; set => m_BorderRightWidth = value; }
        public float borderTopWidth { get => m_BorderTopWidth ?? 0; set => m_BorderTopWidth = value; }
        public float borderBottomWidth { get => m_BorderBottomWidth ?? 0; set => m_BorderBottomWidth = value; }
        public float borderRadius { get => m_BorderRadius ?? 0; set => m_BorderRadius = value; }
        public float fontSize { get => m_FontSize ?? 0; set => m_FontSize = value; }
        public Color color { get => m_Color ?? default; set => m_Color = value; }
        public Color backgroundColor { get => m_BackgroundColor ?? default; set => m_BackgroundColor = value; }
        public Color borderColor { get => m_BorderColor ?? default; set => m_BorderColor = value; }
        public Color helixBackgroundImageTintColor { get => m_HelixBackgroundImageTintColor ?? default; set => m_HelixBackgroundImageTintColor = value; }

        static float? Lerp(float? a, float? b, float t) => a.HasValue && b.HasValue ? a.Value + (b.Value - a.Value) * t : b;
        static Color? Lerp(Color? a, Color? b, float t) => a.HasValue && b.HasValue ? Color.LerpUnclamped(a.Value, b.Value, t) : b;

        internal static StyleValues Interpolate(StyleValues a, StyleValues b, float t) => new StyleValues
        {
            m_Top = Lerp(a.m_Top, b.m_Top, t), m_Left = Lerp(a.m_Left, b.m_Left, t), m_Width = Lerp(a.m_Width, b.m_Width, t),
            m_Height = Lerp(a.m_Height, b.m_Height, t), m_Right = Lerp(a.m_Right, b.m_Right, t), m_Bottom = Lerp(a.m_Bottom, b.m_Bottom, t),
            m_Opacity = Lerp(a.m_Opacity, b.m_Opacity, t), m_FlexGrow = Lerp(a.m_FlexGrow, b.m_FlexGrow, t), m_FlexShrink = Lerp(a.m_FlexShrink, b.m_FlexShrink, t),
            m_MarginLeft = Lerp(a.m_MarginLeft, b.m_MarginLeft, t), m_MarginTop = Lerp(a.m_MarginTop, b.m_MarginTop, t),
            m_MarginRight = Lerp(a.m_MarginRight, b.m_MarginRight, t), m_MarginBottom = Lerp(a.m_MarginBottom, b.m_MarginBottom, t),
            m_PaddingLeft = Lerp(a.m_PaddingLeft, b.m_PaddingLeft, t), m_PaddingTop = Lerp(a.m_PaddingTop, b.m_PaddingTop, t),
            m_PaddingRight = Lerp(a.m_PaddingRight, b.m_PaddingRight, t), m_PaddingBottom = Lerp(a.m_PaddingBottom, b.m_PaddingBottom, t),
            m_BorderLeftWidth = Lerp(a.m_BorderLeftWidth, b.m_BorderLeftWidth, t), m_BorderRightWidth = Lerp(a.m_BorderRightWidth, b.m_BorderRightWidth, t),
            m_BorderTopWidth = Lerp(a.m_BorderTopWidth, b.m_BorderTopWidth, t), m_BorderBottomWidth = Lerp(a.m_BorderBottomWidth, b.m_BorderBottomWidth, t),
            m_BorderRadius = Lerp(a.m_BorderRadius, b.m_BorderRadius, t), m_FontSize = Lerp(a.m_FontSize, b.m_FontSize, t),
            m_Color = Lerp(a.m_Color, b.m_Color, t), m_BackgroundColor = Lerp(a.m_BackgroundColor, b.m_BackgroundColor, t),
            m_BorderColor = Lerp(a.m_BorderColor, b.m_BorderColor, t),
            m_HelixBackgroundImageTintColor = Lerp(a.m_HelixBackgroundImageTintColor, b.m_HelixBackgroundImageTintColor, t),
        };

        internal void Apply(Element e)
        {
            IStyle s = e.style;
            if (m_Top.HasValue) s.top = m_Top.Value;
            if (m_Left.HasValue) s.left = m_Left.Value;
            if (m_Width.HasValue) s.width = m_Width.Value;
            if (m_Height.HasValue) s.height = m_Height.Value;
            if (m_Right.HasValue) s.right = m_Right.Value;
            if (m_Bottom.HasValue) s.bottom = m_Bottom.Value;
            if (m_Opacity.HasValue) s.opacity = m_Opacity.Value;
            if (m_FlexGrow.HasValue) s.flexGrow = m_FlexGrow.Value;
            if (m_FlexShrink.HasValue) s.flexShrink = m_FlexShrink.Value;
            if (m_MarginLeft.HasValue) s.marginLeft = m_MarginLeft.Value;
            if (m_MarginTop.HasValue) s.marginTop = m_MarginTop.Value;
            if (m_MarginRight.HasValue) s.marginRight = m_MarginRight.Value;
            if (m_MarginBottom.HasValue) s.marginBottom = m_MarginBottom.Value;
            if (m_PaddingLeft.HasValue) s.paddingLeft = m_PaddingLeft.Value;
            if (m_PaddingTop.HasValue) s.paddingTop = m_PaddingTop.Value;
            if (m_PaddingRight.HasValue) s.paddingRight = m_PaddingRight.Value;
            if (m_PaddingBottom.HasValue) s.paddingBottom = m_PaddingBottom.Value;
            if (m_BorderLeftWidth.HasValue) s.borderLeftWidth = m_BorderLeftWidth.Value;
            if (m_BorderRightWidth.HasValue) s.borderRightWidth = m_BorderRightWidth.Value;
            if (m_BorderTopWidth.HasValue) s.borderTopWidth = m_BorderTopWidth.Value;
            if (m_BorderBottomWidth.HasValue) s.borderBottomWidth = m_BorderBottomWidth.Value;
            if (m_BorderRadius.HasValue)
            {
                s.borderTopLeftRadius = m_BorderRadius.Value;
                s.borderTopRightRadius = m_BorderRadius.Value;
                s.borderBottomLeftRadius = m_BorderRadius.Value;
                s.borderBottomRightRadius = m_BorderRadius.Value;
            }
            if (m_FontSize.HasValue) s.fontSize = m_FontSize.Value;
            if (m_Color.HasValue) s.color = m_Color.Value;
            if (m_BackgroundColor.HasValue) s.backgroundColor = m_BackgroundColor.Value;
            if (m_BorderColor.HasValue)
            {
                s.borderLeftColor = m_BorderColor.Value;
                s.borderTopColor = m_BorderColor.Value;
                s.borderRightColor = m_BorderColor.Value;
                s.borderBottomColor = m_BorderColor.Value;
            }
            if (m_HelixBackgroundImageTintColor.HasValue) s.helixBackgroundImageTintColor = m_HelixBackgroundImageTintColor.Value;
        }

        // The element's current values of the properties set here.
        internal StyleValues Read(Element e)
        {
            IResolvedStyle r = e.resolvedStyle;
            var v = new StyleValues();
            if (m_Top.HasValue) v.m_Top = r.top;
            if (m_Left.HasValue) v.m_Left = r.left;
            if (m_Width.HasValue) v.m_Width = r.width;
            if (m_Height.HasValue) v.m_Height = r.height;
            if (m_Opacity.HasValue) v.m_Opacity = r.opacity;
            if (m_FlexGrow.HasValue) v.m_FlexGrow = r.flexGrow;
            if (m_FlexShrink.HasValue) v.m_FlexShrink = r.flexShrink;
            if (m_Color.HasValue) v.m_Color = r.color;
            if (m_BackgroundColor.HasValue) v.m_BackgroundColor = r.backgroundColor;
            if (m_FontSize.HasValue) v.m_FontSize = r.fontSize;
            return v;
        }
    }

    // The animations of an element's style and transform (element.experimental.animation).
    public interface ITransitionAnimations
    {
        ValueAnimation<float> Start(float from, float to, int durationMs, Action<Element, float> onValueChanged);
        ValueAnimation<Rect> Start(Rect from, Rect to, int durationMs, Action<Element, Rect> onValueChanged);
        ValueAnimation<Color> Start(Color from, Color to, int durationMs, Action<Element, Color> onValueChanged);
        ValueAnimation<Vector3> Start(Vector3 from, Vector3 to, int durationMs, Action<Element, Vector3> onValueChanged);
        ValueAnimation<Vector2> Start(Vector2 from, Vector2 to, int durationMs, Action<Element, Vector2> onValueChanged);
        ValueAnimation<Quaternion> Start(Quaternion from, Quaternion to, int durationMs, Action<Element, Quaternion> onValueChanged);
        ValueAnimation<StyleValues> Start(StyleValues from, StyleValues to, int durationMs);
        ValueAnimation<StyleValues> Start(StyleValues to, int durationMs);
        ValueAnimation<float> Start(Func<Element, float> fromValueGetter, float to, int durationMs, Action<Element, float> onValueChanged);
        ValueAnimation<Rect> Layout(Rect to, int durationMs);
        ValueAnimation<Vector2> TopLeft(Vector2 to, int durationMs);
        ValueAnimation<Vector2> Size(Vector2 to, int durationMs);
        ValueAnimation<float> Scale(float to, int duration);
        ValueAnimation<Vector3> Position(Vector3 to, int duration);
        ValueAnimation<Quaternion> Rotation(Quaternion to, int duration);
    }

    public interface IExperimentalFeatures
    {
        ITransitionAnimations animation { get; }
    }

    internal sealed class ElementAnimations : ITransitionAnimations, IExperimentalFeatures
    {
        readonly Element m_Element;
        public ElementAnimations(Element element) { m_Element = element; }
        public ITransitionAnimations animation => this;

        ValueAnimation<T> Run<T>(T from, T to, int durationMs, Func<T, T, float, T> lerp, Action<Element, T> update)
        {
            ValueAnimation<T> a = ValueAnimation<T>.Create(m_Element, lerp);
            a.from = from;
            a.to = to;
            a.durationMs = durationMs;
            a.valueUpdated = update;
            a.Start();
            return a;
        }

        static float Lerp(float a, float b, float t) => a + (b - a) * t;
        static Rect Lerp(Rect a, Rect b, float t) => new Rect(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.width, b.width, t), Lerp(a.height, b.height, t));

        public ValueAnimation<float> Start(float from, float to, int durationMs, Action<Element, float> onValueChanged) => Run(from, to, durationMs, Lerp, onValueChanged);
        public ValueAnimation<Rect> Start(Rect from, Rect to, int durationMs, Action<Element, Rect> onValueChanged) => Run(from, to, durationMs, Lerp, onValueChanged);
        public ValueAnimation<Color> Start(Color from, Color to, int durationMs, Action<Element, Color> onValueChanged) => Run(from, to, durationMs, Color.LerpUnclamped, onValueChanged);
        public ValueAnimation<Vector3> Start(Vector3 from, Vector3 to, int durationMs, Action<Element, Vector3> onValueChanged) => Run(from, to, durationMs, Vector3.LerpUnclamped, onValueChanged);
        public ValueAnimation<Vector2> Start(Vector2 from, Vector2 to, int durationMs, Action<Element, Vector2> onValueChanged) => Run(from, to, durationMs, Vector2.LerpUnclamped, onValueChanged);
        public ValueAnimation<Quaternion> Start(Quaternion from, Quaternion to, int durationMs, Action<Element, Quaternion> onValueChanged) => Run(from, to, durationMs, Quaternion.SlerpUnclamped, onValueChanged);
        public ValueAnimation<StyleValues> Start(StyleValues from, StyleValues to, int durationMs) => Run(from, to, durationMs, StyleValues.Interpolate, (e, v) => v.Apply(e));
        public ValueAnimation<StyleValues> Start(StyleValues to, int durationMs) => Start(to.Read(m_Element), to, durationMs);

        public ValueAnimation<float> Start(Func<Element, float> fromValueGetter, float to, int durationMs, Action<Element, float> onValueChanged)
        {
            ValueAnimation<float> a = ValueAnimation<float>.Create(m_Element, Lerp);
            a.initialValue = fromValueGetter;
            a.to = to;
            a.durationMs = durationMs;
            a.valueUpdated = onValueChanged;
            a.Start();
            return a;
        }

        public ValueAnimation<Rect> Layout(Rect to, int durationMs)
        {
            Rect from = m_Element.layout;
            return Start(from, to, durationMs, (e, r) =>
            {
                e.style.left = r.x;
                e.style.top = r.y;
                e.style.width = r.width;
                e.style.height = r.height;
            });
        }

        public ValueAnimation<Vector2> TopLeft(Vector2 to, int durationMs)
        {
            Rect l = m_Element.layout;
            return Start(new Vector2(l.x, l.y), to, durationMs, (e, v) =>
            {
                e.style.left = v.x;
                e.style.top = v.y;
            });
        }

        public ValueAnimation<Vector2> Size(Vector2 to, int durationMs)
        {
            Rect l = m_Element.layout;
            return Start(new Vector2(l.width, l.height), to, durationMs, (e, v) =>
            {
                e.style.width = v.x;
                e.style.height = v.y;
            });
        }

        public ValueAnimation<float> Scale(float to, int duration) =>
            Start(m_Element.resolvedStyle.scale.value.x, to, duration, (e, v) => e.style.scale = new Scale(new Vector3(v, v, 1)));

        public ValueAnimation<Vector3> Position(Vector3 to, int duration)
        {
            Translate t = m_Element.resolvedStyle.translate;
            return Start(new Vector3(t.x.value, t.y.value, t.z), to, duration, (e, v) => e.style.translate = new Translate(new Length(v.x), new Length(v.y), v.z));
        }

        public ValueAnimation<Quaternion> Rotation(Quaternion to, int duration) =>
            Start(Quaternion.identity, to, duration, (e, q) => e.style.rotate = new Rotate(q));
    }
}

namespace HelixEngine.UI
{
    public partial class Element
    {
        Experimental.ElementAnimations m_Experimental;
        public Experimental.IExperimentalFeatures experimental => m_Experimental ??= new Experimental.ElementAnimations(this);
    }
}
