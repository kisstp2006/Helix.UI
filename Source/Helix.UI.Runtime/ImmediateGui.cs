using System;
using System.Collections.Generic;

// The immediate GUI's C# written by hand: what C++ has no form for (the window functions as delegates, the scopes as
// IDisposable, a style named by a string, the state objects of a control). The controls themselves are the bound C++.
namespace HelixEngine
{
    public static partial class Gui
    {
        public delegate void WindowFunction(int id);

        // The window functions by id, called back when the GUI runs a window's contents.
        static readonly Dictionary<int, WindowFunction> s_Windows = new Dictionary<int, WindowFunction>();
        static bool s_Hooked;

        static void CallWindow(int id)
        {
            if (s_Windows.TryGetValue(id, out WindowFunction f) && f != null)
                f(id);
        }

        internal static void SetWindowFunction(int id, WindowFunction func)
        {
            if (func == null)
                throw new ArgumentNullException(nameof(func));
            if (!s_Hooked)
            {
                windowFunctionCalled += CallWindow;
                s_Hooked = true;
            }
            s_Windows[id] = func;
        }

        static Rect DoWindow(int id, Rect clientRect, WindowFunction func, GuiContent title, GuiStyle style, bool modal)
        {
            SetWindowFunction(id, func);
            return WindowInternal(id, clientRect, title, style, modal);
        }

        public static Rect Window(int id, Rect clientRect, WindowFunction func, string text) => DoWindow(id, clientRect, func, new GuiContent(text), skin.window, false);
        public static Rect Window(int id, Rect clientRect, WindowFunction func, Texture2D image) => DoWindow(id, clientRect, func, new GuiContent(image), skin.window, false);
        public static Rect Window(int id, Rect clientRect, WindowFunction func, GuiContent content) => DoWindow(id, clientRect, func, content, skin.window, false);
        public static Rect Window(int id, Rect clientRect, WindowFunction func, string text, GuiStyle style) => DoWindow(id, clientRect, func, new GuiContent(text), style, false);
        public static Rect Window(int id, Rect clientRect, WindowFunction func, Texture2D image, GuiStyle style) => DoWindow(id, clientRect, func, new GuiContent(image), style, false);
        public static Rect Window(int id, Rect clientRect, WindowFunction func, GuiContent title, GuiStyle style) => DoWindow(id, clientRect, func, title, style, false);
        public static Rect ModalWindow(int id, Rect clientRect, WindowFunction func, string text) => DoWindow(id, clientRect, func, new GuiContent(text), skin.window, true);
        public static Rect ModalWindow(int id, Rect clientRect, WindowFunction func, Texture2D image) => DoWindow(id, clientRect, func, new GuiContent(image), skin.window, true);
        public static Rect ModalWindow(int id, Rect clientRect, WindowFunction func, GuiContent content) => DoWindow(id, clientRect, func, content, skin.window, true);
        public static Rect ModalWindow(int id, Rect clientRect, WindowFunction func, string text, GuiStyle style) => DoWindow(id, clientRect, func, new GuiContent(text), style, true);
        public static Rect ModalWindow(int id, Rect clientRect, WindowFunction func, Texture2D image, GuiStyle style) => DoWindow(id, clientRect, func, new GuiContent(image), style, true);
        public static Rect ModalWindow(int id, Rect clientRect, WindowFunction func, GuiContent content, GuiStyle style) => DoWindow(id, clientRect, func, content, style, true);

        // A Begin and its End as a using block.
        public abstract class Scope : IDisposable
        {
            bool m_Disposed;

            protected abstract void CloseScope();

            ~Scope()
            {
                if (!m_Disposed)
                    Debug.LogError("A GUI scope was not disposed: use it in a using block.");
            }

            public void Dispose()
            {
                if (m_Disposed)
                    return;
                m_Disposed = true;
                GC.SuppressFinalize(this);
                CloseScope();
            }
        }

        public class GroupScope : Scope
        {
            public GroupScope(Rect position) => BeginGroup(position);
            public GroupScope(Rect position, string text) => BeginGroup(position, text);
            public GroupScope(Rect position, GuiStyle style) => BeginGroup(position, style);
            public GroupScope(Rect position, GuiContent content, GuiStyle style) => BeginGroup(position, content, style);
            public GroupScope(Rect position, string text, GuiStyle style) => BeginGroup(position, new GuiContent(text), style);
            public GroupScope(Rect position, Texture2D image, GuiStyle style) => BeginGroup(position, new GuiContent(image), style);
            protected override void CloseScope() => EndGroup();
        }

        public class ScrollViewScope : Scope
        {
            public Vector2 scrollPosition { get; private set; }
            public bool handleScrollWheel { get; set; } = true;

            public ScrollViewScope(Rect position, Vector2 scrollPosition, Rect viewRect) => this.scrollPosition = BeginScrollView(position, scrollPosition, viewRect);

            public ScrollViewScope(Rect position, Vector2 scrollPosition, Rect viewRect, bool alwaysShowHorizontal, bool alwaysShowVertical) =>
                this.scrollPosition = BeginScrollView(position, scrollPosition, viewRect, alwaysShowHorizontal, alwaysShowVertical);

            public ScrollViewScope(Rect position, Vector2 scrollPosition, Rect viewRect, GuiStyle horizontalScrollbar, GuiStyle verticalScrollbar) =>
                this.scrollPosition = BeginScrollView(position, scrollPosition, viewRect, horizontalScrollbar, verticalScrollbar);

            public ScrollViewScope(Rect position, Vector2 scrollPosition, Rect viewRect, bool alwaysShowHorizontal, bool alwaysShowVertical,
                                   GuiStyle horizontalScrollbar, GuiStyle verticalScrollbar) =>
                this.scrollPosition = BeginScrollView(position, scrollPosition, viewRect, alwaysShowHorizontal, alwaysShowVertical, horizontalScrollbar, verticalScrollbar);

            protected override void CloseScope() => EndScrollView(handleScrollWheel);
        }

        public class ClipScope : Scope
        {
            public ClipScope(Rect position) => BeginClip(position);
            public ClipScope(Rect position, Vector2 scrollOffset) => BeginClip(position, scrollOffset, Vector2.zero, false);
            protected override void CloseScope() => EndClip();
        }
    }

    public static partial class GuiLayout
    {
        static Rect DoWindow(int id, Rect screenRect, Gui.WindowFunction func, GuiContent title, GuiStyle style, GuiLayoutOption[] options)
        {
            Gui.SetWindowFunction(id, func);
            return WindowInternal(id, screenRect, title, style, options);
        }

        public static Rect Window(int id, Rect screenRect, Gui.WindowFunction func, string text, params GuiLayoutOption[] options) =>
            DoWindow(id, screenRect, func, new GuiContent(text), Gui.skin.window, options);
        public static Rect Window(int id, Rect screenRect, Gui.WindowFunction func, Texture2D image, params GuiLayoutOption[] options) =>
            DoWindow(id, screenRect, func, new GuiContent(image), Gui.skin.window, options);
        public static Rect Window(int id, Rect screenRect, Gui.WindowFunction func, GuiContent content, params GuiLayoutOption[] options) =>
            DoWindow(id, screenRect, func, content, Gui.skin.window, options);
        public static Rect Window(int id, Rect screenRect, Gui.WindowFunction func, string text, GuiStyle style, params GuiLayoutOption[] options) =>
            DoWindow(id, screenRect, func, new GuiContent(text), style, options);
        public static Rect Window(int id, Rect screenRect, Gui.WindowFunction func, Texture2D image, GuiStyle style, params GuiLayoutOption[] options) =>
            DoWindow(id, screenRect, func, new GuiContent(image), style, options);
        public static Rect Window(int id, Rect screenRect, Gui.WindowFunction func, GuiContent content, GuiStyle style, params GuiLayoutOption[] options) =>
            DoWindow(id, screenRect, func, content, style, options);

        public class HorizontalScope : Gui.Scope
        {
            public HorizontalScope(params GuiLayoutOption[] options) => BeginHorizontal(options);
            public HorizontalScope(GuiStyle style, params GuiLayoutOption[] options) => BeginHorizontal(style, options);
            public HorizontalScope(string text, GuiStyle style, params GuiLayoutOption[] options) => BeginHorizontal(text, style, options);
            protected override void CloseScope() => EndHorizontal();
        }

        public class VerticalScope : Gui.Scope
        {
            public VerticalScope(params GuiLayoutOption[] options) => BeginVertical(options);
            public VerticalScope(GuiStyle style, params GuiLayoutOption[] options) => BeginVertical(style, options);
            public VerticalScope(string text, GuiStyle style, params GuiLayoutOption[] options) => BeginVertical(text, style, options);
            protected override void CloseScope() => EndVertical();
        }

        public class AreaScope : Gui.Scope
        {
            public AreaScope(Rect screenRect) => BeginArea(screenRect);
            public AreaScope(Rect screenRect, string text) => BeginArea(screenRect, text);
            public AreaScope(Rect screenRect, string text, GuiStyle style) => BeginArea(screenRect, text, style);
            protected override void CloseScope() => EndArea();
        }

        public class ScrollViewScope : Gui.Scope
        {
            public Vector2 scrollPosition { get; private set; }
            public bool handleScrollWheel { get; set; } = true;

            public ScrollViewScope(Vector2 scrollPosition, params GuiLayoutOption[] options) => this.scrollPosition = BeginScrollView(scrollPosition, options);

            public ScrollViewScope(Vector2 scrollPosition, bool alwaysShowHorizontal, bool alwaysShowVertical, params GuiLayoutOption[] options) =>
                this.scrollPosition = BeginScrollView(scrollPosition, alwaysShowHorizontal, alwaysShowVertical, options);

            public ScrollViewScope(Vector2 scrollPosition, GuiStyle horizontalScrollbar, GuiStyle verticalScrollbar, params GuiLayoutOption[] options) =>
                this.scrollPosition = BeginScrollView(scrollPosition, horizontalScrollbar, verticalScrollbar, options);

            public ScrollViewScope(Vector2 scrollPosition, GuiStyle style, params GuiLayoutOption[] options) => this.scrollPosition = BeginScrollView(scrollPosition, style, options);

            protected override void CloseScope() => EndScrollView();
        }
    }

    public partial class GuiSkin : System.Collections.IEnumerable
    {
        // Its styles: the built-in controls' then the custom ones.
        public System.Collections.IEnumerator GetEnumerator() => AllStyles().GetEnumerator();
    }

    // The names of the commands of ValidateCommand and ExecuteCommand events.
    public static class EventCommandNames
    {
        public const string Cut = "Cut";
        public const string Copy = "Copy";
        public const string Paste = "Paste";
        public const string SelectAll = "SelectAll";
        public const string DeselectAll = "DeselectAll";
        public const string InvertSelection = "InvertSelection";
        public const string Duplicate = "Duplicate";
        public const string Rename = "Rename";
        public const string Delete = "Delete";
        public const string SoftDelete = "SoftDelete";
        public const string Find = "Find";
        public const string SelectChildren = "SelectChildren";
        public const string SelectPrefabRoot = "SelectPrefabRoot";
        public const string UndoRedoPerformed = "UndoRedoPerformed";
        public const string FrameSelected = "FrameSelected";
        public const string FrameSelectedWithLock = "FrameSelectedWithLock";
    }

    // Thrown to leave the GUI's pass at once (GuiUtility.ExitGui ends the pass's controls).
    public sealed class ExitGuiException : Exception
    {
        public ExitGuiException() { }
    }

    public partial class GuiStyle
    {
        // A style of the current skin by its name.
        public static implicit operator GuiStyle(string str) => Gui.skin.GetStyle(str);
    }

    public static partial class GuiUtility
    {
        // The state object of a control: one of its type per id, made on the first query.
        static readonly Dictionary<int, object> s_States = new Dictionary<int, object>();

        public static object GetStateObject(Type t, int controlID)
        {
            if (!s_States.TryGetValue(controlID, out object o) || o == null || o.GetType() != t)
            {
                o = Activator.CreateInstance(t);
                s_States[controlID] = o;
            }
            return o;
        }

        public static object QueryStateObject(Type t, int controlID) =>
            s_States.TryGetValue(controlID, out object o) && o != null && o.GetType() == t ? o : null;
    }
}
