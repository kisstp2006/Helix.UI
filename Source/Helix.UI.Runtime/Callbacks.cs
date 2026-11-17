using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using Helix.Interop;

namespace HelixEngine.UI
{
    // The callbacks of an element on the C# side: each entry by event type and phase; the native side calls the
    // element once per type and phase it listens to.
    public abstract partial class CallbackEventHandler : IEventHandler
    {
        sealed class Entry
        {
            public long typeId;
            public bool trickleDown;
            public Delegate callback;
            public Func<EventBase> make;
            public Action<EventBase> invoke;
            public bool once;
        }

        List<Entry> m_Callbacks;

        internal Element Self => this as Element;

        public void RegisterCallback<TEventType>(EventCallback<TEventType> callback, TrickleDown useTrickleDown = TrickleDown.NoTrickleDown)
            where TEventType : EventBase<TEventType>, new()
        {
            if (callback == null)
                throw new ArgumentException("callback parameter is null");
            Add(EventBase<TEventType>.TypeId(), useTrickleDown, callback, () => EventBase<TEventType>.GetPooled(), e => callback((TEventType)e), false);
        }

        public void RegisterCallback<TEventType, TUserArgsType>(EventCallback<TEventType, TUserArgsType> callback, TUserArgsType userArgs,
                                                                TrickleDown useTrickleDown = TrickleDown.NoTrickleDown)
            where TEventType : EventBase<TEventType>, new()
        {
            if (callback == null)
                throw new ArgumentException("callback parameter is null");
            Add(EventBase<TEventType>.TypeId(), useTrickleDown, callback, () => EventBase<TEventType>.GetPooled(), e => callback((TEventType)e, userArgs), false);
        }

        public void RegisterCallbackOnce<TEventType>(EventCallback<TEventType> callback, TrickleDown useTrickleDown = TrickleDown.NoTrickleDown)
            where TEventType : EventBase<TEventType>, new()
        {
            if (callback == null)
                throw new ArgumentException("callback parameter is null");
            Add(EventBase<TEventType>.TypeId(), useTrickleDown, callback, () => EventBase<TEventType>.GetPooled(), e => callback((TEventType)e), true);
        }

        public void UnregisterCallback<TEventType>(EventCallback<TEventType> callback, TrickleDown useTrickleDown = TrickleDown.NoTrickleDown)
            where TEventType : EventBase<TEventType>, new()
        {
            Remove(EventBase<TEventType>.TypeId(), useTrickleDown, callback);
        }

        public void UnregisterCallback<TEventType, TUserArgsType>(EventCallback<TEventType, TUserArgsType> callback,
                                                                  TrickleDown useTrickleDown = TrickleDown.NoTrickleDown)
            where TEventType : EventBase<TEventType>, new()
        {
            Remove(EventBase<TEventType>.TypeId(), useTrickleDown, callback);
        }

        public bool HasTrickleDownHandlers() => m_Callbacks != null && m_Callbacks.Exists(c => c.trickleDown);
        public bool HasBubbleUpHandlers() => m_Callbacks != null && m_Callbacks.Exists(c => !c.trickleDown);

        // Sends the event to the element through its panel (as the reference: from its own panel, after the event
        // being dispatched).
        public void SendEvent(EventBase e)
        {
            if (e == null || Self == null)
                return;
            Events.Send(Self, e);
        }

        void IEventHandler.HandleEvent(EventBase evt) => SendEvent(evt);

        // Run on this element during the trickle down phase, right after its own trickle down callbacks (controls override).
        protected virtual void HandleEventTrickleDown(EventBase evt) { }
        protected virtual void HandleEventBubbleUp(EventBase evt) { }

        void Add(long typeId, TrickleDown phase, Delegate callback, Func<EventBase> make, Action<EventBase> invoke, bool once)
        {
            m_Callbacks ??= new List<Entry>();
            bool trickle = phase == TrickleDown.TrickleDown;
            foreach (Entry c in m_Callbacks)
                if (c.typeId == typeId && c.trickleDown == trickle && c.callback.Equals(callback))
                    return;
            bool first = !m_Callbacks.Exists(c => c.typeId == typeId && c.trickleDown == trickle);
            m_Callbacks.Add(new Entry { typeId = typeId, trickleDown = trickle, callback = callback, make = make, invoke = invoke, once = once });
            if (first && Self != null)
                Events.Listen(Self, typeId, trickle, true);
        }

        void Remove(long typeId, TrickleDown phase, Delegate callback)
        {
            if (m_Callbacks == null)
                return;
            bool trickle = phase == TrickleDown.TrickleDown;
            int i = m_Callbacks.FindIndex(c => c.typeId == typeId && c.trickleDown == trickle && c.callback.Equals(callback));
            if (i < 0)
                return;
            m_Callbacks.RemoveAt(i);
            if (!m_Callbacks.Exists(c => c.typeId == typeId && c.trickleDown == trickle) && Self != null)
                Events.Listen(Self, typeId, trickle, false);
        }

        // The native dispatch reached this element for one type and phase.
        internal unsafe void InvokeCallbacks(NativeEvent* native)
        {
            if (m_Callbacks == null)
                return;
            long typeId = native->type;
            bool trickle = native->phase == 1;
            Entry[] list = m_Callbacks.FindAll(c => c.typeId == typeId && c.trickleDown == trickle).ToArray();
            if (list.Length == 0)
                return;
            // An event C# sent is the object it made; a native one is made for the callbacks.
            EventBase sent = Events.Sending(typeId);
            EventBase evt = sent ?? list[0].make();
            evt.Attach(native);
            try
            {
                foreach (Entry c in list)
                {
                    if (c.once)
                        Remove(c.typeId, trickle ? TrickleDown.TrickleDown : TrickleDown.NoTrickleDown, c.callback);
                    c.invoke(evt);
                    if (evt.isImmediatePropagationStopped)
                        break;
                }
                if (trickle)
                    HandleEventTrickleDown(evt);
                else
                    HandleEventBubbleUp(evt);
            }
            finally
            {
                evt.Detach();
                if (sent == null)
                    evt.Dispose();
            }
        }
    }

    public partial class Element
    {
        internal NativeElementHandle NativeHandle => new NativeElementHandle { index = handleIndex(), generation = handleGeneration() };

        // The panel the element is in (null outside of one).
        public IPanel panel
        {
            get
            {
                Element root = PanelRootInternal();
                return root == null ? null : RuntimePanel.Of(root);
            }
        }

        public FocusController focusController => (panel as RuntimePanel)?.focusController;
    }

    public enum ContextType
    {
        Player = 0,
        Editor = 1,
    }

    public interface IPanel : IDisposable
    {
        Element visualTree { get; }
        FocusController focusController { get; }
        ContextType contextType { get; }
        float scaledPixelsPerPoint { get; }
        bool isDirty { get; }
        Element Pick(Vector2 point);
        Element PickAll(Vector2 point, List<Element> picked);
    }

    // A panel as scripts see it: its root, focus and picking.
    internal sealed class RuntimePanel : IPanel
    {
        static readonly ConditionalWeakTable<Element, RuntimePanel> s_Panels = new ConditionalWeakTable<Element, RuntimePanel>();
        readonly Element m_Root;

        RuntimePanel(Element root)
        {
            m_Root = root;
            focusController = new FocusController(root);
        }

        internal static RuntimePanel Of(Element root) => s_Panels.GetValue(root, r => new RuntimePanel(r));

        public Element visualTree => m_Root;
        public FocusController focusController { get; }
        public ContextType contextType => ContextType.Player;
        public float scaledPixelsPerPoint => UIView.PanelScaleInternal(m_Root);
        public bool isDirty => true;
        public Element Pick(Vector2 point) => m_Root.PickInternal(point);

        public Element PickAll(Vector2 point, List<Element> picked)
        {
            Element[] all = m_Root.PickAllInternal(point);
            picked?.Clear();
            picked?.AddRange(all);
            return all.Length > 0 ? all[0] : null;
        }

        public void Dispose() { }
    }

    // Positions of the screen and of the world in a panel's pixels.
    public static class RuntimePanelUtils
    {
        // A position of the screen (pixels from the top left) in the panel.
        public static Vector2 ScreenToPanel(IPanel panel, Vector2 screenPosition)
        {
            float scale = panel == null ? 1 : panel.scaledPixelsPerPoint;
            return scale > 0 ? screenPosition / scale : screenPosition;
        }

        // Where a point of the world is seen by a camera, in the panel.
        public static Vector2 CameraTransformWorldToPanel(IPanel panel, Vector3 worldPosition, Camera camera)
        {
            Vector3 screen = camera.WorldToScreenPoint(worldPosition);
            return ScreenToPanel(panel, new Vector2(screen.x, Screen.height - screen.y));
        }

        // The panel rectangle of a box of the world (its top left corner and its size, x right and y down).
        public static Rect CameraTransformWorldToPanelRect(IPanel panel, Vector3 worldPosition, Vector2 worldSize, Camera camera)
        {
            Vector2 topLeft = CameraTransformWorldToPanel(panel, worldPosition, camera);
            Vector2 bottomRight = CameraTransformWorldToPanel(panel, new Vector3(worldPosition.x + worldSize.x, worldPosition.y - worldSize.y, worldPosition.z), camera);
            return new Rect(topLeft, bottomRight - topLeft);
        }
    }

    // The focus of a panel.
    public class FocusController
    {
        readonly Element m_Root;

        internal FocusController(Element root) { m_Root = root; }

        public Focusable focusedElement => m_Root.FocusedElementInternal();

        // The reference's way to keep an event from moving the focus; the panel moves the focus itself after a press.
        public void IgnoreEvent(EventBase evt) { }
    }

    public static class PointerCaptureHelper
    {
        public static bool HasPointerCapture(this IEventHandler handler, int pointerId) => handler is Element e && e.HasPointerCaptureInternal(pointerId);

        public static void CapturePointer(this IEventHandler handler, int pointerId)
        {
            if (handler is Element e)
                e.CapturePointerInternal(pointerId);
        }

        public static void ReleasePointer(this IEventHandler handler, int pointerId)
        {
            if (handler is Element e)
                e.ReleasePointerInternal(pointerId);
        }

        public static IEventHandler GetCapturingElement(this IPanel panel, int pointerId) => panel?.visualTree?.CapturingElementInternal(pointerId);

        public static void ReleasePointer(this IPanel panel, int pointerId)
        {
            if (panel?.visualTree?.CapturingElementInternal(pointerId) is Element e)
                e.ReleasePointerInternal(pointerId);
        }
    }

    public static class MouseCaptureController
    {
        static Element s_Captured;

        public static bool IsMouseCaptured() => s_Captured != null && s_Captured.HasPointerCaptureInternal(PointerId.mousePointerId);
        public static bool HasMouseCapture(this IEventHandler handler) => handler is Element e && e.HasPointerCaptureInternal(PointerId.mousePointerId);

        public static void CaptureMouse(this IEventHandler handler)
        {
            if (handler is Element e && e.CapturePointerInternal(PointerId.mousePointerId))
                s_Captured = e;
        }

        public static void ReleaseMouse(this IEventHandler handler)
        {
            if (handler is Element e)
            {
                e.ReleasePointerInternal(PointerId.mousePointerId);
                if (s_Captured == e)
                    s_Captured = null;
            }
        }
    }
}
