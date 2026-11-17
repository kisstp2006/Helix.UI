using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Helix.Interop;
using Helix.Scripting;

namespace HelixEngine.UI
{
    public enum PropagationPhase
    {
        None = 0,
        TrickleDown = 1,
        BubbleUp = 3,
    }

    public enum TrickleDown
    {
        NoTrickleDown = 0,
        TrickleDown = 1,
    }

    public enum NavigationMoveDirection
    {
        None,
        Left,
        Up,
        Right,
        Down,
        Next,
        Previous,
    }

    public delegate void EventCallback<in TEventType>(TEventType evt);
    public delegate void EventCallback<in TEventType, in TCallbackArgs>(TEventType evt, TCallbackArgs userArgs);

    public interface IEventHandler
    {
        void SendEvent(EventBase e);
        void HandleEvent(EventBase evt);
        bool HasTrickleDownHandlers();
        bool HasBubbleUpHandlers();
    }

    // The panel's event as the native side holds it (its layout checked against the native one when first used).
    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeElementHandle
    {
        public uint index, generation;
        public bool IsValid => index != uint.MaxValue;
        public static readonly NativeElementHandle None = new NativeElementHandle { index = uint.MaxValue, generation = 0 };
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeEvent
    {
        public ushort type;
        public NativeElementHandle target, currentTarget, relatedTarget;
        public byte phase;
        public double timestamp;
        public byte modifiers;
        public int pointerId;
        public byte pointerKind;
        [MarshalAs(UnmanagedType.U1)] public bool isPrimary;
        public int button;
        public uint pressedButtons;
        public int clickCount;
        public Vector2 position, localPosition, deltaPosition, delta;
        public ushort keyCode;
        public uint character;
        public byte direction;
        public Vector2 move;
        [MarshalAs(UnmanagedType.U1)] public bool propagationStopped;
        [MarshalAs(UnmanagedType.U1)] public bool immediatePropagationStopped;
        public Rect oldRect, newRect;

        public static NativeEvent Empty()
        {
            var e = new NativeEvent();
            e.type = ushort.MaxValue;
            e.target = e.currentTarget = e.relatedTarget = NativeElementHandle.None;
            e.pointerId = -1;
            e.isPrimary = true;
            e.button = -1;
            return e;
        }
    }

    // The base of every event: its target and current target, its phase, its stops, its time.
    public abstract unsafe class EventBase : IDisposable
    {
        internal NativeEvent* m_Native;
        internal NativeEvent m_Own = NativeEvent.Empty();
        bool m_Dispatching;

        protected EventBase() { }

        internal ref NativeEvent Data
        {
            get
            {
                if (m_Native != null)
                    return ref *m_Native;
                return ref m_Own;
            }
        }

        public abstract long eventTypeId { get; }
        public virtual bool bubbles => Events.Info(eventTypeId).bubbles;
        public virtual bool tricklesDown => Events.Info(eventTypeId).tricklesDown;
        public long timestamp => (long)(Data.timestamp * 1000.0);
        public IEventHandler target
        {
            get => Events.ElementOf(Data.target);
            set => Data.target = value is Element e ? e.NativeHandle : NativeElementHandle.None;
        }
        public virtual IEventHandler currentTarget => Events.ElementOf(Data.currentTarget);
        public PropagationPhase propagationPhase => Data.phase == 1 ? PropagationPhase.TrickleDown : Data.phase == 2 ? PropagationPhase.BubbleUp : PropagationPhase.None;
        public bool dispatch => m_Dispatching;
        public bool isPropagationStopped => Data.propagationStopped;
        public bool isImmediatePropagationStopped => Data.immediatePropagationStopped;
        public bool pooled { get; internal set; }

        public void StopPropagation() => Data.propagationStopped = true;

        public void StopImmediatePropagation()
        {
            Data.propagationStopped = true;
            Data.immediatePropagationStopped = true;
        }

        protected virtual void Init() => m_Own = NativeEvent.Empty();
        protected internal virtual void PreDispatch(IPanel panel) { }
        protected internal virtual void PostDispatch(IPanel panel) { }

        internal void Attach(NativeEvent* native)
        {
            m_Native = native;
            m_Dispatching = true;
        }

        internal void Detach()
        {
            m_Native = null;
            m_Dispatching = false;
        }

        public virtual void Dispose() { }

        public static long RegisterEventType() => Events.RegisterCustom();
    }

    // The events of one type: their id (the panel's type of the class's name), the pool.
    public abstract class EventBase<T> : EventBase where T : EventBase<T>, new()
    {
        static long s_TypeId = -1;
        static readonly Stack<T> s_Pool = new Stack<T>();
        static Func<T> s_Create;

        public static long TypeId()
        {
            if (s_TypeId < 0)
                s_TypeId = Events.TypeIdOf(typeof(T));
            return s_TypeId;
        }

        public override long eventTypeId => TypeId();

        protected static void SetCreateFunction(Func<T> createMethod) => s_Create = createMethod;

        public static T GetPooled()
        {
            T e = s_Pool.Count > 0 ? s_Pool.Pop() : (s_Create != null ? s_Create() : new T());
            e.Init();
            e.m_Own.type = (ushort)TypeId();
            e.pooled = true;
            return e;
        }

        public sealed override void Dispose()
        {
            if (!pooled)
                return;
            pooled = false;
            Detach();
            s_Pool.Push((T)this);
        }

        protected override void Init() => base.Init();
    }

    // ------------------------------------------------------------------------------------------- pointer events

    public interface IPointerEvent
    {
        int pointerId { get; }
        string pointerType { get; }
        bool isPrimary { get; }
        int button { get; }
        int pressedButtons { get; }
        Vector3 position { get; }
        Vector3 localPosition { get; }
        Vector3 deltaPosition { get; }
        float deltaTime { get; }
        int clickCount { get; }
        float pressure { get; }
        float tangentialPressure { get; }
        float altitudeAngle { get; }
        float azimuthAngle { get; }
        float twist { get; }
        Vector2 tilt { get; }
        PenStatus penStatus { get; }
        Vector2 radius { get; }
        Vector2 radiusVariance { get; }
        EventModifiers modifiers { get; }
        bool shiftKey { get; }
        bool ctrlKey { get; }
        bool commandKey { get; }
        bool altKey { get; }
        bool actionKey { get; }
    }

    public enum PenStatus
    {
        None = 0,
        Contact = 1,
        Barrel = 2,
        Inverted = 4,
        Eraser = 8,
    }

    public static class PointerType
    {
        public static readonly string mouse = "mouse";
        public static readonly string touch = "touch";
        public static readonly string pen = "pen";
        public static readonly string tracked = "tracked";
        public static readonly string unknown = "";
    }

    public static class PointerId
    {
        public static readonly int maxPointers = 32;
        public static readonly int invalidPointerId = -1;
        public static readonly int mousePointerId = 0;
        public static readonly int touchPointerIdBase = 1;
        public static readonly int touchPointerCount = 20;
        public static readonly int penPointerIdBase = 21;
        public static readonly int penPointerCount = 2;
        public static readonly int trackedPointerIdBase = 23;
        public static readonly int trackedPointerCount = 8;
    }

    // What every family with modifier keys shares.
    static class Modifiers
    {
        public static bool Shift(EventModifiers m) => (m & EventModifiers.Shift) != 0;
        public static bool Ctrl(EventModifiers m) => (m & EventModifiers.Control) != 0;
        public static bool Command(EventModifiers m) => (m & EventModifiers.Command) != 0;
        public static bool Alt(EventModifiers m) => (m & EventModifiers.Alt) != 0;
        // The platform's action key: Command on macOS, Control elsewhere.
        public static bool Action(EventModifiers m) => Ctrl(m) || Command(m);
    }

    public abstract class PointerEventBase<T> : EventBase<T>, IPointerEvent where T : PointerEventBase<T>, new()
    {
        public int pointerId { get => Data.pointerId; protected set => Data.pointerId = value; }
        public string pointerType
        {
            get => Data.pointerKind == 0 ? PointerType.mouse : Data.pointerKind == 1 ? PointerType.touch : PointerType.pen;
            protected set => Data.pointerKind = (byte)(value == PointerType.touch ? 1 : value == PointerType.pen ? 2 : 0);
        }
        public bool isPrimary { get => Data.isPrimary; protected set => Data.isPrimary = value; }
        public int button { get => Data.button; protected set => Data.button = value; }
        public int pressedButtons { get => (int)Data.pressedButtons; protected set => Data.pressedButtons = (uint)value; }
        public Vector3 position { get => new Vector3(Data.position.x, Data.position.y, 0); protected set => Data.position = new Vector2(value.x, value.y); }
        public Vector3 localPosition
        {
            get => new Vector3(Data.localPosition.x, Data.localPosition.y, 0);
            protected set => Data.localPosition = new Vector2(value.x, value.y);
        }
        public Vector3 deltaPosition
        {
            get => new Vector3(Data.deltaPosition.x, Data.deltaPosition.y, 0);
            protected set => Data.deltaPosition = new Vector2(value.x, value.y);
        }
        public float deltaTime { get; protected set; }
        public int clickCount { get => Data.clickCount; protected set => Data.clickCount = value; }
        public float pressure { get; protected set; }
        public float tangentialPressure { get; protected set; }
        public float altitudeAngle { get; protected set; }
        public float azimuthAngle { get; protected set; }
        public float twist { get; protected set; }
        public Vector2 tilt { get; protected set; }
        public PenStatus penStatus { get; protected set; }
        public Vector2 radius { get; protected set; }
        public Vector2 radiusVariance { get; protected set; }
        public EventModifiers modifiers { get => (EventModifiers)Data.modifiers; protected set => Data.modifiers = (byte)value; }
        public bool shiftKey => Modifiers.Shift(modifiers);
        public bool ctrlKey => Modifiers.Ctrl(modifiers);
        public bool commandKey => Modifiers.Command(modifiers);
        public bool altKey => Modifiers.Alt(modifiers);
        public bool actionKey => Modifiers.Action(modifiers);

        public static T GetPooled(IPointerEvent pointerEvent)
        {
            T e = GetPooled();
            e.pointerId = pointerEvent.pointerId;
            e.pointerType = pointerEvent.pointerType;
            e.isPrimary = pointerEvent.isPrimary;
            e.button = pointerEvent.button;
            e.pressedButtons = pointerEvent.pressedButtons;
            e.position = pointerEvent.position;
            e.localPosition = pointerEvent.localPosition;
            e.deltaPosition = pointerEvent.deltaPosition;
            e.clickCount = pointerEvent.clickCount;
            e.modifiers = pointerEvent.modifiers;
            return e;
        }
    }

    public sealed class PointerDownEvent : PointerEventBase<PointerDownEvent> { }
    public sealed class PointerMoveEvent : PointerEventBase<PointerMoveEvent> { }
    public sealed class PointerUpEvent : PointerEventBase<PointerUpEvent> { }
    public sealed class PointerCancelEvent : PointerEventBase<PointerCancelEvent> { }
    public sealed class PointerStationaryEvent : PointerEventBase<PointerStationaryEvent> { }
    public sealed class PointerEnterEvent : PointerEventBase<PointerEnterEvent> { }
    public sealed class PointerLeaveEvent : PointerEventBase<PointerLeaveEvent> { }
    public sealed class PointerOverEvent : PointerEventBase<PointerOverEvent> { }
    public sealed class PointerOutEvent : PointerEventBase<PointerOutEvent> { }
    public sealed class ClickEvent : PointerEventBase<ClickEvent> { }

    // --------------------------------------------------------------------------------------------- mouse events

    public interface IMouseEvent
    {
        EventModifiers modifiers { get; }
        Vector2 mousePosition { get; }
        Vector2 localMousePosition { get; }
        Vector2 mouseDelta { get; }
        int clickCount { get; }
        int button { get; }
        int pressedButtons { get; }
        bool shiftKey { get; }
        bool ctrlKey { get; }
        bool commandKey { get; }
        bool altKey { get; }
        bool actionKey { get; }
    }

    public abstract class MouseEventBase<T> : EventBase<T>, IMouseEvent where T : MouseEventBase<T>, new()
    {
        public EventModifiers modifiers { get => (EventModifiers)Data.modifiers; protected set => Data.modifiers = (byte)value; }
        public Vector2 mousePosition { get => Data.position; protected set => Data.position = value; }
        public Vector2 localMousePosition => Data.localPosition;
        public Vector2 mouseDelta { get => Data.deltaPosition; protected set => Data.deltaPosition = value; }
        public int clickCount { get => Data.clickCount; protected set => Data.clickCount = value; }
        public int button { get => Data.button; protected set => Data.button = value; }
        public int pressedButtons { get => (int)Data.pressedButtons; protected set => Data.pressedButtons = (uint)value; }
        public bool shiftKey => Modifiers.Shift(modifiers);
        public bool ctrlKey => Modifiers.Ctrl(modifiers);
        public bool commandKey => Modifiers.Command(modifiers);
        public bool altKey => Modifiers.Alt(modifiers);
        public bool actionKey => Modifiers.Action(modifiers);
    }

    public sealed class MouseDownEvent : MouseEventBase<MouseDownEvent> { }
    public sealed class MouseMoveEvent : MouseEventBase<MouseMoveEvent> { }
    public sealed class MouseUpEvent : MouseEventBase<MouseUpEvent> { }
    public sealed class MouseEnterEvent : MouseEventBase<MouseEnterEvent> { }
    public sealed class MouseLeaveEvent : MouseEventBase<MouseLeaveEvent> { }
    public sealed class MouseOverEvent : MouseEventBase<MouseOverEvent> { }
    public sealed class MouseOutEvent : MouseEventBase<MouseOutEvent> { }

    public sealed class WheelEvent : MouseEventBase<WheelEvent>
    {
        public const float scrollDeltaPerTick = 3;
        public Vector3 delta => new Vector3(Data.delta.x, Data.delta.y, 0);
    }

    // ----------------------------------------------------------------------------------------- capture events

    public interface IPointerCaptureEvent { }
    public interface IMouseCaptureEvent { }

    public abstract class PointerCaptureEventBase<T> : EventBase<T>, IPointerCaptureEvent where T : PointerCaptureEventBase<T>, new()
    {
        public IEventHandler relatedTarget => Events.ElementOf(Data.relatedTarget);
        public int pointerId => Data.pointerId;
    }

    public sealed class PointerCaptureEvent : PointerCaptureEventBase<PointerCaptureEvent> { }
    public sealed class PointerCaptureOutEvent : PointerCaptureEventBase<PointerCaptureOutEvent> { }

    public abstract class MouseCaptureEventBase<T> : PointerCaptureEventBase<T>, IMouseCaptureEvent where T : MouseCaptureEventBase<T>, new() { }

    public sealed class MouseCaptureEvent : MouseCaptureEventBase<MouseCaptureEvent> { }
    public sealed class MouseCaptureOutEvent : MouseCaptureEventBase<MouseCaptureOutEvent> { }

    // -------------------------------------------------------------------------------------------- keyboard

    public interface IKeyboardEvent
    {
        EventModifiers modifiers { get; }
        char character { get; }
        KeyCode keyCode { get; }
        bool shiftKey { get; }
        bool ctrlKey { get; }
        bool commandKey { get; }
        bool altKey { get; }
        bool actionKey { get; }
    }

    public abstract class KeyboardEventBase<T> : EventBase<T>, IKeyboardEvent where T : KeyboardEventBase<T>, new()
    {
        public EventModifiers modifiers { get => (EventModifiers)Data.modifiers; protected set => Data.modifiers = (byte)value; }
        public char character { get => (char)Data.character; protected set => Data.character = value; }
        public KeyCode keyCode { get => (KeyCode)Data.keyCode; protected set => Data.keyCode = (ushort)value; }
        public bool shiftKey => Modifiers.Shift(modifiers);
        public bool ctrlKey => Modifiers.Ctrl(modifiers);
        public bool commandKey => Modifiers.Command(modifiers);
        public bool altKey => Modifiers.Alt(modifiers);
        public bool actionKey => Modifiers.Action(modifiers);

        public static T GetPooled(char c, KeyCode keyCode, EventModifiers modifiers)
        {
            T e = GetPooled();
            e.character = c;
            e.keyCode = keyCode;
            e.modifiers = modifiers;
            return e;
        }
    }

    public sealed class KeyDownEvent : KeyboardEventBase<KeyDownEvent> { }
    public sealed class KeyUpEvent : KeyboardEventBase<KeyUpEvent> { }

    // ----------------------------------------------------------------------------------------------- focus

    public interface IFocusEvent
    {
        Focusable relatedTarget { get; }
        FocusChangeDirection direction { get; }
    }

    public class FocusChangeDirection
    {
        public static FocusChangeDirection unspecified { get; } = new FocusChangeDirection(-1);
        public static FocusChangeDirection none { get; } = new FocusChangeDirection(0);
        protected static FocusChangeDirection lastValue => none;
        readonly int m_Value;

        protected FocusChangeDirection(int value) { m_Value = value; }

        public static implicit operator int(FocusChangeDirection fcd) => fcd?.m_Value ?? 0;
    }

    public abstract class FocusEventBase<T> : EventBase<T>, IFocusEvent where T : FocusEventBase<T>, new()
    {
        public Focusable relatedTarget => Events.ElementOf(Data.relatedTarget);
        public FocusChangeDirection direction => FocusChangeDirection.unspecified;
        protected FocusController focusController => (target as Element)?.focusController;
    }

    public sealed class FocusOutEvent : FocusEventBase<FocusOutEvent> { }
    public sealed class FocusInEvent : FocusEventBase<FocusInEvent> { }
    public sealed class BlurEvent : FocusEventBase<BlurEvent> { }
    public sealed class FocusEvent : FocusEventBase<FocusEvent> { }

    // -------------------------------------------------------------------------------------------- navigation

    public interface INavigationEvent
    {
        EventModifiers modifiers { get; }
        bool shiftKey { get; }
        bool ctrlKey { get; }
        bool commandKey { get; }
        bool altKey { get; }
        bool actionKey { get; }
    }

    public abstract class NavigationEventBase<T> : EventBase<T>, INavigationEvent where T : NavigationEventBase<T>, new()
    {
        public EventModifiers modifiers { get => (EventModifiers)Data.modifiers; protected set => Data.modifiers = (byte)value; }
        public bool shiftKey => Modifiers.Shift(modifiers);
        public bool ctrlKey => Modifiers.Ctrl(modifiers);
        public bool commandKey => Modifiers.Command(modifiers);
        public bool altKey => Modifiers.Alt(modifiers);
        public bool actionKey => Modifiers.Action(modifiers);

        public static T GetPooled(EventModifiers modifiers = EventModifiers.None)
        {
            T e = EventBase<T>.GetPooled();
            e.modifiers = modifiers;
            return e;
        }
    }

    public sealed class NavigationMoveEvent : NavigationEventBase<NavigationMoveEvent>
    {
        public NavigationMoveDirection direction { get => (NavigationMoveDirection)Data.direction; private set => Data.direction = (byte)value; }
        public Vector2 move { get => Data.move; private set => Data.move = value; }

        public static NavigationMoveEvent GetPooled(NavigationMoveDirection direction, EventModifiers modifiers = EventModifiers.None)
        {
            NavigationMoveEvent e = GetPooled(modifiers);
            e.direction = direction;
            e.move = direction == NavigationMoveDirection.Left ? new Vector2(-1, 0) : direction == NavigationMoveDirection.Right ? new Vector2(1, 0)
                   : direction == NavigationMoveDirection.Up ? new Vector2(0, 1) : direction == NavigationMoveDirection.Down ? new Vector2(0, -1) : Vector2.zero;
            return e;
        }
    }

    public sealed class NavigationSubmitEvent : NavigationEventBase<NavigationSubmitEvent> { }
    public sealed class NavigationCancelEvent : NavigationEventBase<NavigationCancelEvent> { }

    // ----------------------------------------------------------------------------------------- drag and drop

    public interface IDragAndDropEvent { }

    public abstract class DragAndDropEventBase<T> : MouseEventBase<T>, IDragAndDropEvent where T : DragAndDropEventBase<T>, new() { }

    public sealed class DragEnterEvent : DragAndDropEventBase<DragEnterEvent> { }
    public sealed class DragLeaveEvent : DragAndDropEventBase<DragLeaveEvent> { }
    public sealed class DragUpdatedEvent : DragAndDropEventBase<DragUpdatedEvent> { }
    public sealed class DragPerformEvent : DragAndDropEventBase<DragPerformEvent> { }
    public sealed class DragExitedEvent : DragAndDropEventBase<DragExitedEvent> { }

    // -------------------------------------------------------------------------------- the panel's own events

    public sealed class GeometryChangedEvent : EventBase<GeometryChangedEvent>
    {
        public Rect oldRect => Data.oldRect;
        public Rect newRect => Data.newRect;
        public int layoutPass => 0;

        public static GeometryChangedEvent GetPooled(Rect oldRect, Rect newRect)
        {
            GeometryChangedEvent e = GetPooled();
            e.Data.oldRect = oldRect;
            e.Data.newRect = newRect;
            return e;
        }
    }

    public interface IPanelChangedEvent { }

    public sealed class AttachToPanelEvent : EventBase<AttachToPanelEvent>, IPanelChangedEvent
    {
        public IPanel originPanel => null;
        public IPanel destinationPanel => (target as Element)?.panel;
    }

    public sealed class DetachFromPanelEvent : EventBase<DetachFromPanelEvent>, IPanelChangedEvent
    {
        public IPanel originPanel => null;
        public IPanel destinationPanel => null;
    }

    // Helix's: the text an input method composes changed (the reference's text fields read it from the input).
    public sealed class CompositionEvent : EventBase<CompositionEvent> { }

    // ------------------------------------------------------------------------------------- the dispatching

    // The C# side of the panel's events: the native hooks set once, the event types by class, the elements' callbacks
    // reached from the native dispatch.
    internal static unsafe class Events
    {
        static bool s_Hooked;
        static delegate* unmanaged[Cdecl]<byte*, int, int, int, int> s_TypeId;
        static delegate* unmanaged[Cdecl]<ulong, int, int, int, void> s_Register;
        static delegate* unmanaged[Cdecl]<ulong, NativeEvent*, int> s_Send;
        static delegate* unmanaged[Cdecl]<ulong, void> s_Keep;
        static readonly Dictionary<long, (bool tricklesDown, bool bubbles)> s_Infos = new Dictionary<long, (bool, bool)>();
        static readonly Dictionary<ulong, WeakReference<Element>> s_Listeners = new Dictionary<ulong, WeakReference<Element>>();
        static int s_CustomCount;

        internal static void Hook()
        {
            if (s_Hooked)
                return;
            s_Hooked = true;
            int* layout = stackalloc int[12];
            ((delegate* unmanaged[Cdecl]<int*, void>)Host.FindExport("HelixUI.EventLayout"))(layout);
            CheckLayout(layout);
            s_TypeId = (delegate* unmanaged[Cdecl]<byte*, int, int, int, int>)Host.FindExport("HelixUI.EventTypeId");
            s_Register = (delegate* unmanaged[Cdecl]<ulong, int, int, int, void>)Host.FindExport("HelixUI.RegisterCallback");
            s_Send = (delegate* unmanaged[Cdecl]<ulong, NativeEvent*, int>)Host.FindExport("HelixUI.SendEvent");
            s_Keep = (delegate* unmanaged[Cdecl]<ulong, void>)Host.FindExport("HelixUI.KeepWrapper");
            delegate* unmanaged[Cdecl]<ulong, NativeEvent*, void> dispatch = &Dispatch;
            delegate* unmanaged[Cdecl]<double, void> tick = &Tick;
            ((delegate* unmanaged[Cdecl]<void*, void>)Host.FindExport("HelixUI.SetDispatcher"))(dispatch);
            ((delegate* unmanaged[Cdecl]<void*, void>)Host.FindExport("HelixUI.SetTick"))(tick);
        }

        static void CheckLayout(int* native)
        {
            int[] mine = { sizeof(NativeEvent), (int)Marshal.OffsetOf<NativeEvent>("target"), (int)Marshal.OffsetOf<NativeEvent>("relatedTarget"),
                           (int)Marshal.OffsetOf<NativeEvent>("timestamp"), (int)Marshal.OffsetOf<NativeEvent>("pointerId"),
                           (int)Marshal.OffsetOf<NativeEvent>("position"), (int)Marshal.OffsetOf<NativeEvent>("keyCode"),
                           (int)Marshal.OffsetOf<NativeEvent>("character"), (int)Marshal.OffsetOf<NativeEvent>("direction"),
                           (int)Marshal.OffsetOf<NativeEvent>("propagationStopped"), (int)Marshal.OffsetOf<NativeEvent>("oldRect"),
                           (int)Marshal.OffsetOf<NativeEvent>("newRect") };
            for (int i = 0; i < mine.Length; i++)
                if (mine[i] != native[i])
                    throw new InvalidOperationException($"The UI's event layout differs from the native one (field {i}: {mine[i]} against {native[i]}).");
        }

        // A generic event's type is its arguments' too (ChangeEvent<int> is not ChangeEvent<bool>).
        static string EventName(Type type)
        {
            if (!type.IsGenericType)
                return type.Name;
            var args = Array.ConvertAll(type.GetGenericArguments(), a => a.FullName ?? a.Name);
            return $"{type.Name}<{string.Join(",", args)}>";
        }

        internal static long TypeIdOf(Type type)
        {
            Hook();
            byte[] name = System.Text.Encoding.UTF8.GetBytes(EventName(type));
            fixed (byte* p = name)
                return s_TypeId(p, name.Length, 1, 1);
        }

        internal static long RegisterCustom()
        {
            Hook();
            byte[] name = System.Text.Encoding.UTF8.GetBytes($"HelixCustomEvent{++s_CustomCount}");
            fixed (byte* p = name)
                return s_TypeId(p, name.Length, 1, 1);
        }

        internal static (bool tricklesDown, bool bubbles) Info(long typeId)
        {
            return s_Infos.TryGetValue(typeId, out var info) ? info : (true, true);
        }

        internal static Element ElementOf(NativeElementHandle h) => h.IsValid ? Element.FromHandle(h.index, h.generation) : null;

        internal static void Listen(Element e, long typeId, bool trickleDown, bool add)
        {
            Hook();
            ulong id = ((INativeObject)e).HxId;
            if (add)
                s_Listeners[id] = new WeakReference<Element>(e);
            s_Register(id, (int)typeId, trickleDown ? 1 : 0, add ? 1 : 0);
        }

        // The element's C# object holds state: kept while the element is in a tree.
        internal static void Keep(Element e)
        {
            Hook();
            s_Keep(((INativeObject)e).HxId);
        }

        // The events C# sends, while they are dispatched: their callbacks get the same object (with its C# fields).
        static readonly List<EventBase> s_Sending = new List<EventBase>();

        internal static EventBase Sending(long typeId)
        {
            for (int i = s_Sending.Count; i-- > 0;)
                if (s_Sending[i].eventTypeId == typeId)
                    return s_Sending[i];
            return null;
        }

        internal static bool Send(Element target, EventBase e)
        {
            Hook();
            NativeEvent data = e.Data;
            s_Sending.Add(e);
            try
            {
                return s_Send(((INativeObject)target).HxId, &data) != 0;
            }
            finally
            {
                s_Sending.RemoveAt(s_Sending.Count - 1);
            }
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        static void Dispatch(ulong elementId, NativeEvent* native)
        {
            try
            {
                if (s_Listeners.TryGetValue(elementId, out var weak) && weak.TryGetTarget(out Element e))
                    e.InvokeCallbacks(native);
            }
            catch (Exception ex)
            {
                Debug.LogException(ex);
            }
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
        static void Tick(double time)
        {
            try
            {
                Scheduler.Tick((long)(time * 1000.0));
                DataBindings.Tick();
            }
            catch (Exception ex)
            {
                Debug.LogException(ex);
            }
        }
    }
}
