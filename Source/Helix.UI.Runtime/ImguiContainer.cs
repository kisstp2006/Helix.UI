using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Helix.Scripting;

namespace HelixEngine.UI
{
    /// <summary>An element that draws immediate GUI content (Gui and GuiLayout calls of its onGuiHandler) in its
    /// rectangle: the handler runs for the element's pointer, wheel and key events (after a Layout pass) and for its
    /// Repaint, which draws into the panel with the other elements (in their order, under their clip).</summary>
    public class ImguiContainer : Element, IDisposable
    {
        public static readonly string hssClassName = "helix-imgui-container";

        static readonly Dictionary<ulong, ImguiContainer> s_Containers = new Dictionary<ulong, ImguiContainer>();
        static ulong s_Next;

        readonly ulong m_Key = ++s_Next;
        bool m_Disposed;
        int m_Captured = -1;

        public Action onGuiHandler { get; set; }
        public ContextType contextType { get; set; } = ContextType.Editor;
        public bool cullingEnabled { get; set; }

        public ImguiContainer() : this(null) { }

        public ImguiContainer(Action onGuiHandler)
        {
            this.onGuiHandler = onGuiHandler;
            AddToClassList(hssClassName);
            focusable = true;
            s_Containers[m_Key] = this;
            generateVisualContent += Repaint;
            RegisterCallback<PointerDownEvent>(e => Pointer(e, EventType.MouseDown, e.button, e.clickCount));
            RegisterCallback<PointerUpEvent>(e => Pointer(e, EventType.MouseUp, e.button, e.clickCount));
            RegisterCallback<PointerMoveEvent>(e => Pointer(e, e.pressedButtons != 0 ? EventType.MouseDrag : EventType.MouseMove, PressedButton(e.pressedButtons), 0));
            RegisterCallback<WheelEvent>(e => Send(e, EventType.ScrollWheel, e.mousePosition, e.delta, 0, 0, e.modifiers, KeyCode.None, '\0'));
            RegisterCallback<KeyDownEvent>(e => Send(e, EventType.KeyDown, Vector2.zero, Vector3.zero, 0, 0, e.modifiers, e.keyCode, e.character));
            RegisterCallback<KeyUpEvent>(e => Send(e, EventType.KeyUp, Vector2.zero, Vector3.zero, 0, 0, e.modifiers, e.keyCode, e.character));
            RegisterCallback<DetachFromPanelEvent>(_ => ReleaseCapture());
            // Immediate content may change every frame: drawn again each frame while in a panel.
            schedule.Execute(() =>
            {
                if (panel != null && !m_Disposed)
                    MarkDirtyRepaint();
            }).Every(0);
        }

        static int PressedButton(int pressed)
        {
            for (int b = 0; b < 3; b++)
                if ((pressed & (1 << b)) != 0)
                    return b;
            return 0;
        }

        /// <summary>The layout of the GUI's content is made again (GuiLayout's sizes changed).</summary>
        public void MarkDirtyLayout() => MarkDirtyRepaint();

        public void Dispose()
        {
            if (m_Disposed)
                return;
            m_Disposed = true;
            generateVisualContent -= Repaint;
            ReleaseCapture();
            s_Containers.Remove(m_Key);
            GuiContainerNative.Remove(m_Key);
        }

        void Pointer<T>(PointerEventBase<T> e, EventType type, int button, int clicks) where T : PointerEventBase<T>, new()
        {
            bool used = Send(e, type, new Vector2(e.position.x, e.position.y), e.deltaPosition, button, clicks, e.modifiers, KeyCode.None, '\0');
            // A control that takes the pointer (a slider, a window's drag) keeps it until the button is up.
            if (type == EventType.MouseDown && used && m_Captured < 0)
            {
                m_Captured = e.pointerId;
                this.CapturePointer(e.pointerId);
            }
            else if (type == EventType.MouseUp && m_Captured == e.pointerId)
                ReleaseCapture();
        }

        void ReleaseCapture()
        {
            if (m_Captured >= 0)
            {
                this.ReleasePointer(m_Captured);
                m_Captured = -1;
            }
        }

        unsafe bool Send(EventBase e, EventType type, Vector2 position, Vector3 delta, int button, int clicks, EventModifiers modifiers, KeyCode key, char character)
        {
            if (m_Disposed || onGuiHandler == null || GuiContainerNative.send == null)
                return false;
            Rect r = worldBound;
            float* rect = stackalloc float[4] { r.x, r.y, r.width, r.height };
            float* p = stackalloc float[2] { position.x, position.y };
            float* d = stackalloc float[2] { delta.x, delta.y };
            bool used = GuiContainerNative.send(m_Key, rect, (int)type, p, d, button, clicks, (int)modifiers, (int)key, character) != 0;
            if (used)
            {
                e.StopPropagation();
                MarkDirtyRepaint();
            }
            return used;
        }

        unsafe void Repaint(MeshGenerationContext mgc)
        {
            if (m_Disposed || onGuiHandler == null || GuiContainerNative.repaint == null)
                return;
            if (cullingEnabled && panel != null)
            {
                Rect view = panel.visualTree.worldBound, r = worldBound;
                if (r.x > view.x + view.width || r.y > view.y + view.height || r.x + r.width < view.x || r.y + r.height < view.y)
                    return;
            }
            mgc.Flush();
            Rect box = localBound;
            GuiContainerNative.repaint(m_Key, mgc.nativeContext, box.width, box.height);
        }

        internal static void Invoke(ulong key)
        {
            if (!s_Containers.TryGetValue(key, out ImguiContainer c) || c.onGuiHandler == null)
                return;
            try
            {
                c.onGuiHandler();
            }
            catch (Exception ex) when (!(ex is ExitGuiException))
            {
                Debug.LogException(ex);
            }
        }
    }

    // The native GUI's container functions (Helix.UI's exports).
    internal static unsafe class GuiContainerNative
    {
        internal static delegate* unmanaged[Cdecl]<ulong, float*, int, float*, float*, int, int, int, int, int, int> send;
        internal static delegate* unmanaged[Cdecl]<ulong, ulong, float, float, void> repaint;
        static delegate* unmanaged[Cdecl]<ulong, void> s_Remove;

#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Start()
        {
            void* set = Host.FindExport("HelixUI.SetGuiContainerCallback");
            if (set == null)
                return;
            send = (delegate* unmanaged[Cdecl]<ulong, float*, int, float*, float*, int, int, int, int, int, int>)Host.FindExport("HelixUI.GuiContainerEvent");
            repaint = (delegate* unmanaged[Cdecl]<ulong, ulong, float, float, void>)Host.FindExport("HelixUI.GuiContainerRepaint");
            s_Remove = (delegate* unmanaged[Cdecl]<ulong, void>)Host.FindExport("HelixUI.GuiContainerRemove");
            delegate* unmanaged[Cdecl]<ulong, void> call = &Call;
            ((delegate* unmanaged[Cdecl]<void*, void>)set)(call);
        }

        internal static void Remove(ulong key)
        {
            if (s_Remove != null)
                s_Remove(key);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static void Call(ulong key) => ImguiContainer.Invoke(key);
    }
}
