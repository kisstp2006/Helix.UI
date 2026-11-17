using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Helix.Interop;
using Helix.Scripting;
using HelixEngine.Internal;

namespace HelixEngine.UI
{
    public partial class UIPanelSettings
    {
        // The settings' functions by their objects' ids, for the panels' input.
        static readonly Dictionary<ulong, Delegate> s_Functions = new Dictionary<ulong, Delegate>();
        static bool s_Registered;

        // The panel point of a screen point (pixels from the bottom left; NaN when it is not on the panel): a texture
        // panel shown in the scene takes pointers through it.
        public void SetScreenToPanelSpaceFunction(Func<Vector2, Vector2> screentoPanelSpaceFunction) => SetFunction(screentoPanelSpaceFunction, 1);

        // The same with the distance from the screen in z, against the world space panels' hits.
        public void SetScreenToPanelSpaceFunction3D(Func<Vector2, Vector3> screenToPanelSpaceFunction) => SetFunction(screenToPanelSpaceFunction, 2);

        void SetFunction(Delegate function, int kind)
        {
            Register();
            ulong id = ((INativeObject)this).HxId;
            if (function == null)
            {
                s_Functions.Remove(id);
                SetScreenToPanelKindInternal(0);
                return;
            }
            s_Functions[id] = function;
            SetScreenToPanelKindInternal(kind);
        }

        static unsafe void Register()
        {
            if (s_Registered)
                return;
            s_Registered = true;
            void* set = Host.FindExport("HelixUI.SetScreenToPanelCallback");
            if (set == null)
                return;
            delegate* unmanaged[Cdecl]<ulong, float, float, float*, int> map = &Map;
            ((delegate* unmanaged[Cdecl]<void*, void>)set)(map);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static unsafe int Map(ulong id, float x, float y, float* result)
        {
            try
            {
                if (!s_Functions.TryGetValue(id, out Delegate function))
                    return 0;
                // The native side's points are from the top left.
                var screen = new Vector2(x, Screen.height - y);
                Vector3 r;
                if (function is Func<Vector2, Vector3> f3)
                    r = f3(screen);
                else
                {
                    Vector2 p = ((Func<Vector2, Vector2>)function)(screen);
                    r = new Vector3(p.x, p.y, 0);
                }
                result[0] = r.x;
                result[1] = r.y;
                result[2] = r.z;
                return float.IsNaN(r.x) || float.IsNaN(r.y) ? 0 : 1;
            }
            catch (Exception e)
            {
                Debug.LogException(e);
                return 0;
            }
        }
    }
}

namespace HelixEngine.UI
{
    public partial class UIView
    {
        // A nested view's root: in the flow of its parent's panel (Relative) or over it (Absolute).
        public Position position
        {
            get => PositionInternal() == 0 ? Position.Relative : Position.Absolute;
            set => SetPositionInternal(value == Position.Relative ? 0 : 1);
        }
    }
}

namespace HelixEngine.UI
{
    public partial class PanelInputConfiguration
    {
        public enum PanelInputRedirection
        {
            AutoRedirect = 0,
            Never = 1,
            AlwaysRedirect = 2,
        }

        public PanelInputRedirection panelInputRedirection
        {
            get => (PanelInputRedirection)PanelInputRedirectionInternal();
            set => SetPanelInputRedirectionInternal((int)value);
        }
    }
}
