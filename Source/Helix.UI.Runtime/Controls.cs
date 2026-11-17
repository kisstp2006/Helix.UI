using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Helix.Interop;
using Helix.Scripting;
using HelixEngine.Internal;

namespace HelixEngine.UI
{
    // ------------------------------------------------------------------------------------------- notifications

    // What the native controls did (clicked, a value changed by the user, a tab picked) reaches their C# objects.
    internal static unsafe class ControlNotifications
    {
#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Start()
        {
            void* set = Host.FindExport("HelixUI.SetControlNotify");
            if (set == null)
                return;
            delegate* unmanaged[Cdecl]<uint, uint, int, void> notify = &Notify;
            ((delegate* unmanaged[Cdecl]<void*, void>)set)(notify);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static void Notify(uint index, uint generation, int what)
        {
            try
            {
                Element.FromHandle(index, generation)?.OnControlNotify(what);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }
    }

    public partial class Element
    {
        internal const int NotifyClicked = 1, NotifyValueChanged = 2, NotifySelected = 3;
        internal virtual void OnControlNotify(int what) { }
        internal void KeepForCallbacks() => Events.Keep(this);
    }

    // ---------------------------------------------------------------------------------------------- the events

    public interface IChangeEvent { }

    public class ChangeEvent<T> : EventBase<ChangeEvent<T>>, IChangeEvent
    {
        public T previousValue { get; protected set; }
        public T newValue { get; protected set; }

        public static ChangeEvent<T> GetPooled(T previousValue, T newValue)
        {
            ChangeEvent<T> e = GetPooled();
            e.previousValue = previousValue;
            e.newValue = newValue;
            return e;
        }

        protected override void Init()
        {
            base.Init();
            previousValue = default;
            newValue = default;
        }
    }

    // The text a text field's input changed (before its ChangeEvent).
    public class InputEvent : EventBase<InputEvent>
    {
        public string previousData { get; protected set; }
        public string newData { get; protected set; }

        public static InputEvent GetPooled(string previousData, string newData)
        {
            InputEvent e = GetPooled();
            e.previousData = previousData;
            e.newData = newData;
            return e;
        }

        protected override void Init()
        {
            base.Init();
            previousData = null;
            newData = null;
        }
    }

    public interface INotifyValueChanged<T>
    {
        T value { get; set; }
        void SetValueWithoutNotify(T newValue);
    }

    public static class INotifyValueChangedExtensions
    {
        public static bool RegisterValueChangedCallback<T>(this INotifyValueChanged<T> control, EventCallback<ChangeEvent<T>> callback)
        {
            if (control is not CallbackEventHandler handler)
                return false;
            handler.RegisterCallback(callback);
            return true;
        }

        public static bool UnregisterValueChangedCallback<T>(this INotifyValueChanged<T> control, EventCallback<ChangeEvent<T>> callback)
        {
            if (control is not CallbackEventHandler handler)
                return false;
            handler.UnregisterCallback(callback);
            return true;
        }
    }

    // ------------------------------------------------------------------------------------------- manipulators

    public interface IManipulator
    {
        Element target { get; set; }
    }

    public abstract class Manipulator : IManipulator
    {
        Element m_Target;

        protected abstract void RegisterCallbacksOnTarget();
        protected abstract void UnregisterCallbacksFromTarget();

        public Element target
        {
            get => m_Target;
            set
            {
                if (m_Target != null)
                    UnregisterCallbacksFromTarget();
                m_Target = value;
                if (m_Target != null)
                    RegisterCallbacksOnTarget();
            }
        }
    }

    public struct ManipulatorActivationFilter
    {
        public MouseButton button { get; set; }
        public EventModifiers modifiers { get; set; }
        public int clickCount { get; set; }
    }

    public enum MouseButton
    {
        LeftMouse = 0,
        RightMouse = 1,
        MiddleMouse = 2,
    }

    // A manipulator started by the mouse buttons (and modifiers) of its activators.
    public abstract class MouseManipulator : Manipulator
    {
        public List<ManipulatorActivationFilter> activators { get; } = new List<ManipulatorActivationFilter>();

        static bool Matches(ManipulatorActivationFilter a, int button, EventModifiers modifiers, int clickCount) =>
            (int)a.button == button && (a.modifiers == EventModifiers.None || (modifiers & a.modifiers) == a.modifiers) && (a.clickCount <= 0 || a.clickCount == clickCount);

        protected bool CanStartManipulation(IMouseEvent e)
        {
            if (activators.Count == 0)
                return e.button == 0;
            return activators.Exists(a => Matches(a, e.button, e.modifiers, e.clickCount));
        }

        protected bool CanStopManipulation(IMouseEvent e) => e != null && (activators.Count == 0 || activators.Exists(a => (int)a.button == e.button));
    }

    public abstract class PointerManipulator : MouseManipulator
    {
        protected bool CanStartManipulation(IPointerEvent e)
        {
            if (activators.Count == 0)
                return e.button == 0;
            foreach (ManipulatorActivationFilter a in activators)
                if ((int)a.button == e.button && (a.modifiers == EventModifiers.None || (e.modifiers & a.modifiers) == a.modifiers) &&
                    (a.clickCount <= 0 || a.clickCount == e.clickCount))
                    return true;
            return false;
        }

        protected bool CanStopManipulation(IPointerEvent e) => e != null && (activators.Count == 0 || activators.Exists(a => (int)a.button == e.button));
    }

    // Clicks of its target: a press and a release over it (repeated while held when it has an interval). A
    // button's clicks come from its native side.
    public class Clickable : PointerManipulator
    {
        public event Action<EventBase> clickedWithEventInfo;
        public event Action clicked;
        readonly long m_Delay, m_Interval;
        internal bool m_Native;
        int m_Pointer = -1;
        IElementScheduledItem m_Repeater;

        public Clickable(Action handler, long delay, long interval)
        {
            clicked = handler;
            m_Delay = delay;
            m_Interval = interval;
        }

        public Clickable(Action<EventBase> handler) { clickedWithEventInfo = handler; }
        public Clickable(Action handler) { clicked = handler; }

        public Vector2 lastMousePosition { get; private set; }
        public bool active => m_Pointer >= 0;

        internal void InvokeNative()
        {
            clicked?.Invoke();
            clickedWithEventInfo?.Invoke(null);
        }

        protected void Invoke(EventBase evt)
        {
            clicked?.Invoke();
            clickedWithEventInfo?.Invoke(evt);
        }

        protected override void RegisterCallbacksOnTarget()
        {
            if (target is Button)
            {
                // A button presses and clicks natively.
                m_Native = true;
                target.KeepForCallbacks();
                return;
            }
            target.RegisterCallback<PointerDownEvent>(OnDown);
            target.RegisterCallback<PointerMoveEvent>(OnMove);
            target.RegisterCallback<PointerUpEvent>(OnUp);
        }

        protected override void UnregisterCallbacksFromTarget()
        {
            if (m_Native)
                return;
            target.UnregisterCallback<PointerDownEvent>(OnDown);
            target.UnregisterCallback<PointerMoveEvent>(OnMove);
            target.UnregisterCallback<PointerUpEvent>(OnUp);
        }

        void OnDown(PointerDownEvent e)
        {
            if (!CanStartManipulation(e))
                return;
            m_Pointer = e.pointerId;
            lastMousePosition = e.localPosition;
            target.CapturePointer(e.pointerId);
            if (m_Interval > 0)
            {
                Invoke(e);
                m_Repeater = target.schedule.Execute(() => clicked?.Invoke()).Every(m_Interval).StartingIn(m_Delay);
            }
            e.StopPropagation();
        }

        void OnMove(PointerMoveEvent e)
        {
            if (e.pointerId == m_Pointer)
                lastMousePosition = e.localPosition;
        }

        void OnUp(PointerUpEvent e)
        {
            if (e.pointerId != m_Pointer)
                return;
            m_Pointer = -1;
            target.ReleasePointer(e.pointerId);
            m_Repeater?.Pause();
            m_Repeater = null;
            if (m_Interval <= 0 && target.ContainsPoint(target.WorldToLocal(new Vector2(e.position.x, e.position.y))))
                Invoke(e);
            e.StopPropagation();
        }
    }

    public static class ElementExtensions
    {
        public static void AddManipulator(this Element ele, IManipulator manipulator)
        {
            if (manipulator != null)
                manipulator.target = ele;
        }

        public static void RemoveManipulator(this Element ele, IManipulator manipulator)
        {
            if (manipulator != null && manipulator.target == ele)
                manipulator.target = null;
        }
    }

    // ----------------------------------------------------------------------------------------- simple controls

    public partial class TextElement
    {
        public static readonly string hssClassName = "helix-text-element";
        public TextElement(string text) : this() { this.text = text; }
    }

    public partial class Label
    {
        public new static readonly string hssClassName = "helix-label";
    }

    public partial class Button
    {
        public new static readonly string hssClassName = "helix-button";
        public static readonly string iconHssClassName = hssClassName + "--with-icon";
        public static readonly string imageHSSClassName = hssClassName + "__image";
        Clickable m_Clickable;

        public Button(Action clickEvent) : this() { clicked += clickEvent; }

        public Clickable clickable
        {
            get
            {
                if (m_Clickable == null)
                    clickable = new Clickable((Action)null);
                return m_Clickable;
            }
            set
            {
                if (m_Clickable != null && m_Clickable.target == this)
                    this.RemoveManipulator(m_Clickable);
                m_Clickable = value;
                if (m_Clickable != null)
                    this.AddManipulator(m_Clickable);
            }
        }

        public event Action clicked
        {
            add => clickable.clicked += value;
            remove => clickable.clicked -= value;
        }

        internal override void OnControlNotify(int what)
        {
            if (what == NotifyClicked)
                m_Clickable?.InvokeNative();
        }
    }

    public partial class RepeatButton
    {
        public new static readonly string hssClassName = "helix-repeat-button";

        public RepeatButton(Action clickEvent, long delay, long interval) : this() { SetAction(clickEvent, delay, interval); }

        public void SetAction(Action clickEvent, long delay, long interval)
        {
            this.delay = delay;
            this.interval = interval;
            clickable = new Clickable(clickEvent);
        }
    }

    public partial class Image
    {
        public static readonly string hssClassName = "helix-image";
    }

    public partial class Box
    {
        public static readonly string hssClassName = "helix-box";
    }

    public partial class GroupBox
    {
        public static readonly string hssClassName = "helix-group-box";
        public static readonly string labelHssClassName = hssClassName + "__label";
    }

    public partial class HelpBox
    {
        public static readonly string hssClassName = "helix-help-box";
        public static readonly string labelHssClassName = hssClassName + "__label";
        public static readonly string iconHssClassName = hssClassName + "__icon";
        public static readonly string iconInfoHssClassName = "helix-icon-info";
        public static readonly string iconwarningHssClassName = "helix-icon-warning";
        public static readonly string iconErrorHssClassName = "helix-icon-error";
    }

    public partial class ProgressBar
    {
        public static readonly string hssClassName = "helix-progress-bar";
        public static readonly string containerHssClassName = hssClassName + "__container";
        public static readonly string titleHssClassName = hssClassName + "__title";
        public static readonly string titleContainerHssClassName = hssClassName + "__title-container";
        public static readonly string progressHssClassName = hssClassName + "__progress";
        public static readonly string backgroundHssClassName = hssClassName + "__background";
    }

    public partial class PopupWindow
    {
        public new static readonly string hssClassName = "helix-popup-window";
        public static readonly string contentHssClassName = hssClassName + "__content-container";
    }

    // ------------------------------------------------------------------------------------------ the containers

    public partial class Foldout : INotifyValueChanged<bool>
    {
        public static readonly string hssClassName = "helix-foldout";
        public static readonly string toggleHssClassName = hssClassName + "__toggle";
        public static readonly string contentHssClassName = hssClassName + "__content";
        public static readonly string inputHssClassName = hssClassName + "__input";
        public static readonly string checkmarkHssClassName = hssClassName + "__checkmark";
        public static readonly string textHssClassName = hssClassName + "__text";

        internal override void OnControlNotify(int what)
        {
            if (what != NotifyValueChanged || panel == null)
                return;
            using ChangeEvent<bool> e = ChangeEvent<bool>.GetPooled(PreviousValueInternal(), value);
            e.target = this;
            SendEvent(e);
        }
    }

    public partial class Scroller
    {
        public static readonly string hssClassName = "helix-scroller";
        public static readonly string horizontalVariantHssClassName = hssClassName + "--horizontal";
        public static readonly string verticalVariantHssClassName = hssClassName + "--vertical";
        public static readonly string sliderHssClassName = hssClassName + "__slider";
        public static readonly string lowButtonHssClassName = hssClassName + "__low-button";
        public static readonly string highButtonHssClassName = hssClassName + "__high-button";
        Action<float> m_ValueChanged;

        public Scroller(float lowValue, float highValue, Action<float> valueChanged, SliderDirection direction = SliderDirection.Vertical) : this()
        {
            this.direction = direction;
            this.lowValue = lowValue;
            this.highValue = highValue;
            this.valueChanged += valueChanged;
        }

        public event Action<float> valueChanged
        {
            add
            {
                KeepForCallbacks();
                m_ValueChanged += value;
            }
            remove => m_ValueChanged -= value;
        }

        internal override void OnControlNotify(int what)
        {
            if (what == NotifyValueChanged)
                m_ValueChanged?.Invoke(value);
        }
    }

    public partial class ScrollView
    {
        public static readonly string hssClassName = "helix-scroll-view";
        public static readonly string viewportHssClassName = hssClassName + "__content-viewport";
        public static readonly string contentHssClassName = hssClassName + "__content-container";
        public static readonly string hScrollerHssClassName = hssClassName + "__horizontal-scroller";
        public static readonly string vScrollerHssClassName = hssClassName + "__vertical-scroller";
    }

    public partial class TabView
    {
        public static readonly string hssClassName = "helix-tab-view";
        public static readonly string headerContainerClassName = hssClassName + "__header-container";
        public static readonly string contentContainerHssClassName = hssClassName + "__content-container";
        Action<Tab, Tab> m_ActiveTabChanged;

        public event Action<Tab, Tab> activeTabChanged
        {
            add
            {
                KeepForCallbacks();
                m_ActiveTabChanged += value;
            }
            remove => m_ActiveTabChanged -= value;
        }

        internal override void OnControlNotify(int what)
        {
            if (what == NotifySelected)
                m_ActiveTabChanged?.Invoke(PreviousTabInternal(), activeTab);
        }
    }

    public partial class Tab
    {
        public static readonly string hssClassName = "helix-tab";
        public static readonly string tabHeaderHssClassName = hssClassName + "__header";
        public static readonly string tabHeaderLabelHssClassName = tabHeaderHssClassName + "-label";
        public static readonly string contentHssClassName = hssClassName + "__content-container";
    }

    public partial class TwoPaneSplitView
    {
        public static readonly string hssClassName = "helix-two-pane-split-view";
        public static readonly string contentContainerClassName = hssClassName + "__content-container";
        public static readonly string horizontalClassName = hssClassName + "--horizontal";
        public static readonly string verticalClassName = hssClassName + "--vertical";
    }

    // ---------------------------------------------------------------------------------------------- the fields

    // A field's value: the native field's slots (each base's layout) or, for a field only C# knows, its own.
    public abstract class BaseField<TValueType> : AbstractBaseField, INotifyValueChanged<TValueType>
    {
        public static readonly string hssClassName = "helix-base-field";
        public static readonly string labelHssClassName = hssClassName + "__label";
        public static readonly string inputHssClassName = hssClassName + "__input";
        public static readonly string noLabelVariantHssClassName = hssClassName + "--no-label";
        public static readonly string labelDraggerVariantHssClassName = labelHssClassName + "--with-dragger";
        public static readonly string mixedValueLabelHssClassName = labelHssClassName + "--mixed-value";
        public static readonly string alignedFieldHssClassName = hssClassName + "__aligned";
        TValueType m_Value, m_Previous;

        protected internal BaseField(ObjectId id) : base(id) { }

        protected BaseField(string label, Element visualInput) : base()
        {
            this.label = label;
            if (visualInput != null)
                VisualInputInternal().Add(visualInput);
        }

        protected Element visualInput
        {
            get => VisualInputInternal();
            set
            {
                Element input = VisualInputInternal();
                input.Clear();
                if (value != null)
                    input.Add(value);
            }
        }

        // The value as the native slots hold it (a field only C# knows keeps it here).
        protected virtual TValueType ReadValue() => m_Value;
        protected virtual void WriteValue(TValueType v) => m_Value = v;
        protected virtual TValueType ReadPreviousValue() => m_Previous;

        protected TValueType rawValue
        {
            get => ReadValue();
            set => WriteValue(value);
        }

        public virtual TValueType value
        {
            get => ReadValue();
            set
            {
                TValueType previous = ReadValue();
                if (EqualityComparer<TValueType>.Default.Equals(previous, value))
                    return;
                m_Previous = previous;
                SetValueWithoutNotify(value);
                SendChange(previous, ReadValue());
            }
        }

        public virtual void SetValueWithoutNotify(TValueType newValue) => WriteValue(newValue);

        internal void SendChange(TValueType previous, TValueType now)
        {
            if (panel == null)
                return;
            using ChangeEvent<TValueType> e = ChangeEvent<TValueType>.GetPooled(previous, now);
            e.target = this;
            SendEvent(e);
        }

        internal override void OnControlNotify(int what)
        {
            if (what == NotifyValueChanged)
                SendChange(ReadPreviousValue(), ReadValue());
        }
    }

    public abstract class BaseBoolField : BaseField<bool>
    {
        protected internal BaseBoolField(ObjectId id) : base(id) { }
        public BaseBoolField(string label) : base(label, null) { }

        protected override bool ReadValue() => IntegerInternal(0) != 0;
        protected override void WriteValue(bool v) => SetIntegerInternal(0, v ? 1 : 0);
        protected override bool ReadPreviousValue() => PreviousIntegerInternal(0) != 0;

        public string text
        {
            get => StringInternal();
            set => SetStringInternal(value ?? "");
        }

        public bool toggleOnLabelClick { get; set; } = true;
    }

    public partial class Toggle
    {
        public new static readonly string hssClassName = "helix-toggle";
        public static readonly string checkmarkHssClassName = hssClassName + "__checkmark";
        public static readonly string textHssClassName = hssClassName + "__text";
        public Toggle(string label) : this() { this.label = label; }
    }

    public partial class RadioButton
    {
        public new static readonly string hssClassName = "helix-radio-button";
        public static readonly string checkmarkBackgroundHssClassName = hssClassName + "__checkmark-background";
        public static readonly string checkmarkHssClassName = hssClassName + "__checkmark";
        public static readonly string textHssClassName = hssClassName + "__text";
        public RadioButton(string label) : this() { this.label = label; }
    }

    public partial class RadioButtonGroup
    {
        public new static readonly string hssClassName = "helix-radio-button-group";

        public RadioButtonGroup(string label, List<string> radioButtonsChoices = null) : this()
        {
            this.label = label;
            if (radioButtonsChoices != null)
                choices = radioButtonsChoices;
        }

        protected override int ReadValue() => (int)IntegerInternal(0);
        protected override void WriteValue(int v) => SetIntegerInternal(0, v);
        protected override int ReadPreviousValue() => (int)PreviousIntegerInternal(0);

        public IEnumerable<string> choices
        {
            get => ChoicesInternal().Length == 0 ? Array.Empty<string>() : ChoicesInternal().Split('\n');
            set => SetChoicesInternal(value == null ? "" : string.Join("\n", value));
        }
    }

    // Which of a toggle button group's buttons are on (bits of a 64 bit mask) and how many there are.
    public struct ToggleButtonGroupState : IEquatable<ToggleButtonGroupState>
    {
        ulong m_Data;
        int m_Length;

        public ToggleButtonGroupState(ulong optionsBitMask, int length)
        {
            m_Data = optionsBitMask;
            m_Length = length;
        }

        public int length => m_Length;

        public bool this[int index]
        {
            get => index >= 0 && index < 64 && ((m_Data >> index) & 1) != 0;
            set
            {
                if (index < 0 || index >= 64)
                    throw new ArgumentOutOfRangeException(nameof(index));
                m_Data = value ? m_Data | (1UL << index) : m_Data & ~(1UL << index);
            }
        }

        public Span<int> GetActiveOptions(Span<int> activeOptionsIndices)
        {
            int n = 0;
            for (int i = 0; i < m_Length && i < 64 && n < activeOptionsIndices.Length; i++)
                if (this[i])
                    activeOptionsIndices[n++] = i;
            return activeOptionsIndices.Slice(0, n);
        }

        public Span<int> GetInactiveOptions(Span<int> inactiveOptionsIndices)
        {
            int n = 0;
            for (int i = 0; i < m_Length && i < 64 && n < inactiveOptionsIndices.Length; i++)
                if (!this[i])
                    inactiveOptionsIndices[n++] = i;
            return inactiveOptionsIndices.Slice(0, n);
        }

        public void SetAllOptions() => m_Data = m_Length >= 64 ? ulong.MaxValue : (1UL << m_Length) - 1;
        public void ResetAllOptions() => m_Data = 0;
        public void ToggleAllOptions() => m_Data = ~m_Data & (m_Length >= 64 ? ulong.MaxValue : (1UL << m_Length) - 1);
        internal ulong mask => m_Data;

        public bool Equals(ToggleButtonGroupState other) => m_Data == other.m_Data && m_Length == other.m_Length;
        public override bool Equals(object obj) => obj is ToggleButtonGroupState s && Equals(s);
        public override int GetHashCode() => HashCode.Combine(m_Data, m_Length);
        public static bool operator ==(ToggleButtonGroupState a, ToggleButtonGroupState b) => a.Equals(b);
        public static bool operator !=(ToggleButtonGroupState a, ToggleButtonGroupState b) => !a.Equals(b);
        public override string ToString()
        {
            var chars = new char[m_Length];
            for (int i = 0; i < m_Length; i++)
                chars[i] = this[i] ? '1' : '0';
            return new string(chars);
        }
    }

    public partial class ToggleButtonGroup
    {
        public new static readonly string hssClassName = "helix-toggle-button-group";
        public static readonly string containerHssClassName = hssClassName + "__container";
        public static readonly string buttonGroupClassName = hssClassName + "__button";

        public ToggleButtonGroup(string label) : this() { this.label = label; }

        protected override ToggleButtonGroupState ReadValue() => new ToggleButtonGroupState((ulong)IntegerInternal(0), (int)IntegerInternal(1));

        protected override void WriteValue(ToggleButtonGroupState v)
        {
            SetIntegerInternal(1, v.length);
            SetIntegerInternal(0, (long)v.mask);
        }

        protected override ToggleButtonGroupState ReadPreviousValue() => new ToggleButtonGroupState((ulong)PreviousIntegerInternal(0), (int)PreviousIntegerInternal(1));

        public bool isMultipleSelection
        {
            get => SettingInternal(0) != 0;
            set => SetSettingInternal(0, value ? 1 : 0);
        }

        public bool allowEmptySelection
        {
            get => SettingInternal(1) != 0;
            set => SetSettingInternal(1, value ? 1 : 0);
        }
    }

    // ------------------------------------------------------------------------------------------------ sliders

    public abstract class BaseSlider<TValueType> : BaseField<TValueType> where TValueType : IComparable<TValueType>
    {
        public new static readonly string hssClassName = "helix-base-slider";
        public static readonly string horizontalVariantHssClassName = hssClassName + "--horizontal";
        public static readonly string verticalVariantHssClassName = hssClassName + "--vertical";
        public static readonly string dragContainerHssClassName = hssClassName + "__drag-container";
        public static readonly string trackerHssClassName = hssClassName + "__tracker";
        public static readonly string draggerHssClassName = hssClassName + "__dragger";
        public static readonly string fillHssClassName = hssClassName + "__fill";

        protected internal BaseSlider(ObjectId id) : base(id) { }

        static TValueType From(double d) => (TValueType)Convert.ChangeType(typeof(TValueType) == typeof(int) ? Math.Round(d) : d, typeof(TValueType), CultureInfo.InvariantCulture);
        static double To(TValueType v) => Convert.ToDouble(v, CultureInfo.InvariantCulture);

        protected override TValueType ReadValue() => From(NumberInternal(0));
        protected override void WriteValue(TValueType v)
        {
            double d = To(v), lo = Math.Min(SettingInternal(0), SettingInternal(1)), hi = Math.Max(SettingInternal(0), SettingInternal(1));
            SetNumberInternal(0, Math.Clamp(d, lo, hi));
        }
        protected override TValueType ReadPreviousValue() => From(PreviousNumberInternal(0));

        public TValueType lowValue
        {
            get => From(SettingInternal(0));
            set
            {
                SetSettingInternal(0, To(value));
                SetValueWithoutNotify(this.value);
            }
        }

        public TValueType highValue
        {
            get => From(SettingInternal(1));
            set
            {
                SetSettingInternal(1, To(value));
                SetValueWithoutNotify(this.value);
            }
        }

        public TValueType range => From(SettingInternal(1) - SettingInternal(0));

        public virtual float pageSize
        {
            get => (float)SettingInternal(2);
            set => SetSettingInternal(2, value);
        }

        public SliderDirection direction
        {
            get => SettingInternal(3) != 0 ? SliderDirection.Vertical : SliderDirection.Horizontal;
            set => SetSettingInternal(3, value == SliderDirection.Vertical ? 1 : 0);
        }

        public bool inverted
        {
            get => SettingInternal(4) != 0;
            set => SetSettingInternal(4, value ? 1 : 0);
        }

        public bool fill
        {
            get => SettingInternal(5) != 0;
            set => SetSettingInternal(5, value ? 1 : 0);
        }

        public bool showInputField
        {
            get => SettingInternal(7) != 0;
            set => SetSettingInternal(7, value ? 1 : 0);
        }
    }

    public partial class Slider
    {
        public new static readonly string hssClassName = "helix-slider";

        public Slider(float start, float end, SliderDirection direction = SliderDirection.Horizontal, float pageSize = 0) : this()
        {
            lowValue = start;
            highValue = end;
            this.direction = direction;
            this.pageSize = pageSize;
        }

        public Slider(string label, float start = 0, float end = 10, SliderDirection direction = SliderDirection.Horizontal, float pageSize = 0)
            : this(start, end, direction, pageSize)
        {
            this.label = label;
        }
    }

    public partial class SliderInt
    {
        public new static readonly string hssClassName = "helix-slider-int";

        public SliderInt(int start, int end, SliderDirection direction = SliderDirection.Horizontal, float pageSize = 0) : this()
        {
            lowValue = start;
            highValue = end;
            this.direction = direction;
            this.pageSize = pageSize;
        }

        public SliderInt(string label, int start = 0, int end = 10, SliderDirection direction = SliderDirection.Horizontal, float pageSize = 0)
            : this(start, end, direction, pageSize)
        {
            this.label = label;
        }
    }

    public partial class MinMaxSlider
    {
        public new static readonly string hssClassName = "helix-min-max-slider";

        public MinMaxSlider(float minValue, float maxValue, float minLimit, float maxLimit) : this()
        {
            lowLimit = minLimit;
            highLimit = maxLimit;
            value = new Vector2(minValue, maxValue);
        }

        public MinMaxSlider(string label, float minValue = 0, float maxValue = 10, float minLimit = float.MinValue, float maxLimit = float.MaxValue)
            : this(minValue, maxValue, minLimit, maxLimit)
        {
            this.label = label;
        }

        protected override Vector2 ReadValue() => new Vector2((float)NumberInternal(0), (float)NumberInternal(1));

        protected override void WriteValue(Vector2 v)
        {
            float lo = lowLimit, hi = highLimit;
            float a = Math.Clamp(v.x, lo, hi), b = Math.Clamp(v.y, a, hi);
            SetNumberInternal(0, a);
            SetNumberInternal(1, b);
        }

        protected override Vector2 ReadPreviousValue() => new Vector2((float)PreviousNumberInternal(0), (float)PreviousNumberInternal(1));

        public float minValue
        {
            get => value.x;
            set => this.value = new Vector2(value, this.value.y);
        }

        public float maxValue
        {
            get => value.y;
            set => this.value = new Vector2(this.value.x, value);
        }

        public float range => Math.Abs(highLimit - lowLimit);

        public float lowLimit
        {
            get => (float)SettingInternal(0);
            set => SetSettingInternal(0, value);
        }

        public float highLimit
        {
            get => (float)SettingInternal(1);
            set => SetSettingInternal(1, value);
        }
    }

    // ----------------------------------------------------------------------------------------- text input

    public abstract class TextInputBaseField<TValueType> : BaseField<TValueType>
    {
        public new static readonly string hssClassName = "helix-base-text-field";
        public static readonly string textInputHssName = "helix-text-input";
        public new static readonly string inputHssClassName = hssClassName + "__input";
        public static readonly string innerComponentsModifierName = "--inner-input-field-component";
        public static readonly string innerTextElementHssClassName = TextElement.hssClassName + innerComponentsModifierName;

        protected internal TextInputBaseField(ObjectId id) : base(id) { }

        public string text
        {
            get => TextInternal();
            set => SetTextInternal(value ?? "");
        }

        bool Option(int which) => TextOptionInternal(which) != 0;
        void Option(int which, bool on) => SetTextOptionInternal(which, on ? 1 : 0);

        public bool isReadOnly { get => Option(0); set => Option(0, value); }
        public bool isPasswordField { get => Option(1); set => Option(1, value); }
        public char maskChar { get => (char)TextOptionInternal(2); set => SetTextOptionInternal(2, value); }
        public int maxLength { get => TextOptionInternal(3); set => SetTextOptionInternal(3, value); }
        public bool isDelayed { get => Option(4); set => Option(4, value); }
        public bool selectAllOnFocus { get => Option(5); set => Option(5, value); }
        public bool selectAllOnMouseUp { get => Option(6); set => Option(6, value); }
        public bool doubleClickSelectsWord { get => Option(7); set => Option(7, value); }
        public bool tripleClickSelectsLine { get => Option(8); set => Option(8, value); }
        public int cursorIndex { get => TextOptionInternal(10); set => SetTextOptionInternal(10, value); }
        public int selectIndex { get => TextOptionInternal(11); set => SetTextOptionInternal(11, value); }
        public bool hideMobileInput { get; set; }
        public bool autoCorrection { get; set; }

        public void SelectAll() => SelectRangeInternal(text.Length, 0);
        public void SelectNone() => SelectRangeInternal(cursorIndex, cursorIndex);
        public void SelectRange(int cursorIndex, int selectionIndex) => SelectRangeInternal(cursorIndex, selectionIndex);
    }

    public partial class TextField
    {
        public new static readonly string hssClassName = "helix-text-field";
        public new static readonly string inputHssClassName = hssClassName + "__input";

        public TextField(string label) : this() { this.label = label; }

        public TextField(int maxLength, bool multiline, bool isPasswordField, char maskChar) : this()
        {
            this.maxLength = maxLength;
            this.multiline = multiline;
            this.isPasswordField = isPasswordField;
            this.maskChar = maskChar;
        }

        public TextField(string label, int maxLength, bool multiline, bool isPasswordField, char maskChar) : this(maxLength, multiline, isPasswordField, maskChar)
        {
            this.label = label;
        }

        protected override string ReadValue() => StringInternal();
        protected override void WriteValue(string v) => SetStringInternal(v ?? "");
        protected override string ReadPreviousValue() => PreviousStringInternal();

        public bool multiline
        {
            get => TextOptionInternal(9) != 0;
            set => SetTextOptionInternal(9, value ? 1 : 0);
        }

        internal override void OnControlNotify(int what)
        {
            if (what == NotifyValueChanged && panel != null)
                using (InputEvent e = InputEvent.GetPooled(PreviousStringInternal(), StringInternal()))
                {
                    e.target = this;
                    SendEvent(e);
                }
            base.OnControlNotify(what);
        }
    }

    // A number as text: its slots by its type (integers in the integer slot, floating ones in the number slot).
    public abstract class TextValueField<TValueType> : TextInputBaseField<TValueType>
    {
        protected internal TextValueField(ObjectId id) : base(id) { }

        static bool Floating => typeof(TValueType) == typeof(float) || typeof(TValueType) == typeof(double);

        static TValueType FromSlots(double number, long integer)
        {
            object v = typeof(TValueType) == typeof(int) ? (int)integer
                       : typeof(TValueType) == typeof(long) ? integer
                       : typeof(TValueType) == typeof(uint) ? (uint)(ulong)integer
                       : typeof(TValueType) == typeof(ulong) ? (ulong)integer
                       : typeof(TValueType) == typeof(float) ? (float)number
                       : (object)number;
            return (TValueType)v;
        }

        protected override TValueType ReadValue() => FromSlots(NumberInternal(0), IntegerInternal(0));
        protected override TValueType ReadPreviousValue() => FromSlots(PreviousNumberInternal(0), PreviousIntegerInternal(0));

        protected override void WriteValue(TValueType v)
        {
            if (Floating)
                SetNumberInternal(0, Convert.ToDouble(v, CultureInfo.InvariantCulture));
            else if (v is ulong u)
                SetIntegerInternal(0, (long)u);
            else
                SetIntegerInternal(0, Convert.ToInt64(v, CultureInfo.InvariantCulture));
        }

        public string formatString { get; set; }
    }

    public partial class IntegerField { public IntegerField(string label) : this() { this.label = label; } }
    public partial class LongField { public LongField(string label) : this() { this.label = label; } }
    public partial class UnsignedIntegerField { public UnsignedIntegerField(string label) : this() { this.label = label; } }
    public partial class UnsignedLongField { public UnsignedLongField(string label) : this() { this.label = label; } }
    public partial class FloatField { public FloatField(string label) : this() { this.label = label; } }
    public partial class DoubleField { public DoubleField(string label) : this() { this.label = label; } }

    public partial class Hash128Field
    {
        public Hash128Field(string label) : this() { this.label = label; }

        static Hash128 FromSlots(long a, long b) => new Hash128((ulong)a, (ulong)b);

        protected override Hash128 ReadValue() => FromSlots(IntegerInternal(0), IntegerInternal(1));
        protected override Hash128 ReadPreviousValue() => FromSlots(PreviousIntegerInternal(0), PreviousIntegerInternal(1));

        protected override void WriteValue(Hash128 v)
        {
            // The halves from its text (its first half's bytes from the lowest).
            string s = v.ToString();
            ulong a = 0, b = 0;
            for (int i = 0; i < 16; i++)
            {
                ulong by = Convert.ToUInt64(s.Substring(i * 2, 2), 16);
                if (i < 8)
                    a |= by << (i * 8);
                else
                    b |= by << ((i - 8) * 8);
            }
            SetIntegerInternal(1, (long)b);
            SetIntegerInternal(0, (long)a);
        }
    }

    // ------------------------------------------------------------------------------------------- composites

    public abstract class BaseCompositeField<TValueType, TField, TFieldValue> : BaseField<TValueType> where TField : TextValueField<TFieldValue>, new()
    {
        public new static readonly string hssClassName = "helix-composite-field";
        public static readonly string spacerHssClassName = hssClassName + "__field-spacer";
        public static readonly string multilineVariantHssClassName = hssClassName + "--multi-line";
        public static readonly string fieldGroupHssClassName = hssClassName + "__field-group";
        public static readonly string fieldHssClassName = hssClassName + "__field";
        public static readonly string firstFieldVariantHssClassName = fieldHssClassName + "--first";
        public static readonly string twoLinesVariantHssClassName = hssClassName + "--two-lines";

        protected internal BaseCompositeField(ObjectId id) : base(id) { }

        static bool Integers => typeof(TFieldValue) == typeof(int);
        double[] Read(bool previous)
        {
            int n = (int)SettingInternal(0);
            var parts = new double[n];
            for (int i = 0; i < n; i++)
                parts[i] = Integers ? (previous ? PreviousIntegerInternal(i) : IntegerInternal(i)) : (previous ? PreviousNumberInternal(i) : NumberInternal(i));
            return parts;
        }

        static TValueType Compose(double[] p)
        {
            object v = default(TValueType);
            switch (v)
            {
            case Vector2: v = new Vector2((float)p[0], (float)p[1]); break;
            case Vector3: v = new Vector3((float)p[0], (float)p[1], (float)p[2]); break;
            case Vector4: v = new Vector4((float)p[0], (float)p[1], (float)p[2], (float)p[3]); break;
            case Vector2Int: v = new Vector2Int((int)p[0], (int)p[1]); break;
            case Vector3Int: v = new Vector3Int((int)p[0], (int)p[1], (int)p[2]); break;
            case Rect: v = new Rect((float)p[0], (float)p[1], (float)p[2], (float)p[3]); break;
            case RectInt: v = new RectInt((int)p[0], (int)p[1], (int)p[2], (int)p[3]); break;
            case Bounds: v = new Bounds(new Vector3((float)p[0], (float)p[1], (float)p[2]), new Vector3((float)p[3] * 2, (float)p[4] * 2, (float)p[5] * 2)); break;
            case BoundsInt: v = new BoundsInt((int)p[0], (int)p[1], (int)p[2], (int)p[3], (int)p[4], (int)p[5]); break;
            }
            return (TValueType)v;
        }

        static double[] Split(TValueType value)
        {
            switch (value)
            {
            case Vector2 a: return new double[] { a.x, a.y };
            case Vector3 a: return new double[] { a.x, a.y, a.z };
            case Vector4 a: return new double[] { a.x, a.y, a.z, a.w };
            case Vector2Int a: return new double[] { a.x, a.y };
            case Vector3Int a: return new double[] { a.x, a.y, a.z };
            case Rect a: return new double[] { a.x, a.y, a.width, a.height };
            case RectInt a: return new double[] { a.x, a.y, a.width, a.height };
            case Bounds a: return new double[] { a.center.x, a.center.y, a.center.z, a.extents.x, a.extents.y, a.extents.z };
            case BoundsInt a: return new double[] { a.position.x, a.position.y, a.position.z, a.size.x, a.size.y, a.size.z };
            }
            return Array.Empty<double>();
        }

        protected override TValueType ReadValue() => Compose(Read(false));
        protected override TValueType ReadPreviousValue() => Compose(Read(true));

        protected override void WriteValue(TValueType v)
        {
            double[] p = Split(v);
            for (int i = 0; i < p.Length; i++)
            {
                if (Integers)
                    SetIntegerInternal(i, (long)p[i]);
                else
                    SetNumberInternal(i, p[i]);
            }
        }
    }

    public partial class Vector2Field { public Vector2Field(string label) : this() { this.label = label; } }
    public partial class Vector3Field { public Vector3Field(string label) : this() { this.label = label; } }
    public partial class Vector4Field { public Vector4Field(string label) : this() { this.label = label; } }
    public partial class Vector2IntField { public Vector2IntField(string label) : this() { this.label = label; } }
    public partial class Vector3IntField { public Vector3IntField(string label) : this() { this.label = label; } }
    public partial class RectField { public RectField(string label) : this() { this.label = label; } }
    public partial class RectIntField { public RectIntField(string label) : this() { this.label = label; } }
    public partial class BoundsField { public BoundsField(string label) : this() { this.label = label; } }
    public partial class BoundsIntField { public BoundsIntField(string label) : this() { this.label = label; } }

    // ------------------------------------------------------------------------------------------------ popups

    // A field that picks its value from a menu of choices (their text in the native menu).
    public abstract class BasePopupField<TValueType, TValueChoice> : BaseField<TValueType>
    {
        public new static readonly string hssClassName = "helix-base-popup-field";
        public static readonly string textHssClassName = hssClassName + "__text";
        public static readonly string arrowHssClassName = hssClassName + "__arrow";
        public new static readonly string labelHssClassName = hssClassName + "__label";
        public new static readonly string inputHssClassName = hssClassName + "__input";
        internal List<TValueChoice> m_Choices;

        protected internal BasePopupField(ObjectId id) : base(id) { }

        protected BasePopupField(string label) : base(label, null) { ShowMenuReady(); }

        void ShowMenuReady() => SetShownTextInternal("");

        public virtual List<TValueChoice> choices
        {
            get => m_Choices;
            set
            {
                m_Choices = value;
                SetChoicesInternal(value == null ? "" : string.Join("\n", value.ConvertAll(ChoiceText)));
                SetValueWithoutNotify(this.value);
            }
        }

        protected virtual string ChoiceText(TValueChoice choice) => choice?.ToString() ?? "";

        public string text => TextOfValue();
        protected abstract string TextOfValue();

        public void ShowMenu() => ShowMenuInternal();
    }

    public class PopupField<T> : BasePopupField<T, T>
    {
        public new static readonly string hssClassName = "helix-popup-field";
        public Func<T, string> formatSelectedValueCallback { get; set; }
        public Func<T, string> formatListItemCallback { get; set; }

        protected internal PopupField(ObjectId id) : base(id) { }
        public PopupField() : this(null) { }
        public PopupField(string label = null) : base(label) { SetIntegerInternal(0, -1); }

        public PopupField(List<T> choices, T defaultValue, Func<T, string> formatSelectedValueCallback = null, Func<T, string> formatListItemCallback = null)
            : this(null, choices, defaultValue, formatSelectedValueCallback, formatListItemCallback) { }

        public PopupField(string label, List<T> choices, T defaultValue, Func<T, string> formatSelectedValueCallback = null,
                          Func<T, string> formatListItemCallback = null) : this(label)
        {
            this.formatSelectedValueCallback = formatSelectedValueCallback;
            this.formatListItemCallback = formatListItemCallback;
            this.choices = choices;
            SetValueWithoutNotify(defaultValue);
        }

        public PopupField(List<T> choices, int defaultIndex, Func<T, string> formatSelectedValueCallback = null, Func<T, string> formatListItemCallback = null)
            : this(null, choices, defaultIndex, formatSelectedValueCallback, formatListItemCallback) { }

        public PopupField(string label, List<T> choices, int defaultIndex, Func<T, string> formatSelectedValueCallback = null,
                          Func<T, string> formatListItemCallback = null) : this(label)
        {
            this.formatSelectedValueCallback = formatSelectedValueCallback;
            this.formatListItemCallback = formatListItemCallback;
            this.choices = choices;
            index = defaultIndex;
        }

        protected override string ChoiceText(T choice) => formatListItemCallback != null ? formatListItemCallback(choice) : choice?.ToString() ?? "";

        public override List<T> choices
        {
            get
            {
                // A field the markup made: its choices are the native's text.
                if (m_Choices == null && typeof(T) == typeof(string) && ChoicesInternal().Length > 0)
                    m_Choices = (List<T>)(object)new List<string>(ChoicesInternal().Split('\n'));
                return m_Choices;
            }
            set => base.choices = value;
        }

        public virtual int index
        {
            get => (int)IntegerInternal(0);
            set
            {
                int i = value;
                this.value = i >= 0 && choices != null && i < choices.Count ? choices[i] : default;
            }
        }

        protected override T ReadValue()
        {
            int i = (int)IntegerInternal(0);
            List<T> c = choices;
            if (c != null && i >= 0 && i < c.Count)
                return c[i];
            return typeof(T) == typeof(string) ? (T)(object)StringInternal() : default;
        }

        protected override T ReadPreviousValue()
        {
            int i = (int)PreviousIntegerInternal(0);
            List<T> c = choices;
            if (c != null && i >= 0 && i < c.Count)
                return c[i];
            return typeof(T) == typeof(string) ? (T)(object)PreviousStringInternal() : default;
        }

        protected override void WriteValue(T v)
        {
            List<T> c = choices;
            int i = c == null ? -1 : c.FindIndex(x => EqualityComparer<T>.Default.Equals(x, v));
            if (typeof(T) == typeof(string))
                SetStringInternal((string)(object)v ?? "");
            SetIntegerInternal(0, i);
            string shown = i >= 0 ? (formatSelectedValueCallback != null ? formatSelectedValueCallback(v) : ChoiceText(v)) : (v?.ToString() ?? "");
            SetShownTextInternal(shown);
        }

        protected override string TextOfValue()
        {
            T v = ReadValue();
            return formatSelectedValueCallback != null ? formatSelectedValueCallback(v) : v?.ToString() ?? "";
        }
    }

    public partial class DropdownField
    {
        public DropdownField(string label) : this() { this.label = label; }

        public DropdownField(List<string> choices, string defaultValue, Func<string, string> formatSelectedValueCallback = null,
                             Func<string, string> formatListItemCallback = null) : this()
        {
            this.formatSelectedValueCallback = formatSelectedValueCallback;
            this.formatListItemCallback = formatListItemCallback;
            this.choices = choices;
            SetValueWithoutNotify(defaultValue);
        }

        public DropdownField(string label, List<string> choices, int defaultIndex, Func<string, string> formatSelectedValueCallback = null,
                             Func<string, string> formatListItemCallback = null) : this()
        {
            this.label = label;
            this.formatSelectedValueCallback = formatSelectedValueCallback;
            this.formatListItemCallback = formatListItemCallback;
            this.choices = choices;
            index = defaultIndex;
        }
    }

    public partial class EnumField
    {
        public new static readonly string hssClassName = "helix-enum-field";
        Type m_EnumType;
        Array m_Values;

        public EnumField(Enum defaultValue) : this() { Init(defaultValue); }
        public EnumField(string label, Enum defaultValue = null) : this()
        {
            this.label = label;
            if (defaultValue != null)
                Init(defaultValue);
        }

        public Type type => m_EnumType;

        public void Init(Enum defaultValue) => Init(defaultValue, false);

        public void Init(Enum defaultValue, bool includeObsoleteValues)
        {
            m_EnumType = defaultValue.GetType();
            m_Values = Enum.GetValues(m_EnumType);
            SetChoicesInternal(string.Join("\n", Enum.GetNames(m_EnumType)));
            SetValueWithoutNotify(defaultValue);
        }

        public string text => ReadValue()?.ToString() ?? "";

        protected override Enum ReadValue()
        {
            int i = (int)IntegerInternal(0);
            return m_Values != null && i >= 0 && i < m_Values.Length ? (Enum)m_Values.GetValue(i) : null;
        }

        protected override Enum ReadPreviousValue()
        {
            int i = (int)PreviousIntegerInternal(0);
            return m_Values != null && i >= 0 && i < m_Values.Length ? (Enum)m_Values.GetValue(i) : null;
        }

        protected override void WriteValue(Enum v)
        {
            if (v == null)
                return;
            if (m_EnumType != v.GetType())
                Init(v);
            int i = Array.IndexOf(m_Values, v);
            SetIntegerInternal(0, i);
        }
    }

    // A mask picked from a menu: Nothing, Everything, then the choices (each a bit, or its mask of choicesMasks).
    public abstract class BaseMaskField<TChoice> : BasePopupField<TChoice, string>
    {
        protected internal BaseMaskField(ObjectId id) : base(id) { }

        public virtual List<TChoice> choicesMasks { get; set; }

        static TChoice From(long v) => typeof(TChoice) == typeof(ulong) ? (TChoice)(object)(ulong)v : (TChoice)(object)(int)v;
        static long To(TChoice v) => v is ulong u ? (long)u : Convert.ToInt64(v, CultureInfo.InvariantCulture);

        protected override TChoice ReadValue() => From(IntegerInternal(0));
        protected override TChoice ReadPreviousValue() => From(PreviousIntegerInternal(0));
        protected override void WriteValue(TChoice v) => SetIntegerInternal(0, To(v));

        public override List<string> choices
        {
            get
            {
                if (m_Choices == null && ChoicesInternal().Length > 0)
                    m_Choices = new List<string>(ChoicesInternal().Split('\n'));
                return m_Choices;
            }
            set
            {
                m_Choices = value;
                SetChoicesInternal(value == null ? "" : string.Join("\n", value));
            }
        }

        protected override string TextOfValue()
        {
            long mask = To(ReadValue());
            List<string> c = choices ?? new List<string>();
            if (mask == 0)
                return "Nothing";
            int on = 0, last = -1;
            for (int i = 0; i < c.Count && i < 64; i++)
                if (((mask >> i) & 1) != 0)
                {
                    on++;
                    last = i;
                }
            return on == c.Count ? "Everything" : on == 1 ? c[last] : "Mixed...";
        }
    }

    public partial class MaskField
    {
        public new static readonly string hssClassName = "helix-mask-field";
        public MaskField(string label) : this() { this.label = label; }

        public MaskField(List<string> choices, int defaultMask, Func<string, string> formatSelectedValueCallback = null,
                         Func<string, string> formatListItemCallback = null) : this()
        {
            this.choices = choices;
            SetValueWithoutNotify(defaultMask);
        }

        public MaskField(string label, List<string> choices, int defaultMask, Func<string, string> formatSelectedValueCallback = null,
                         Func<string, string> formatListItemCallback = null) : this(choices, defaultMask)
        {
            this.label = label;
        }
    }

    public partial class Mask64Field
    {
        public new static readonly string hssClassName = "helix-mask64-field";
        public Mask64Field(string label) : this() { this.label = label; }

        public Mask64Field(List<string> choices, ulong defaultMask, Func<string, string> formatSelectedValueCallback = null,
                           Func<string, string> formatListItemCallback = null) : this()
        {
            this.choices = choices;
            SetValueWithoutNotify(defaultMask);
        }
    }
}
