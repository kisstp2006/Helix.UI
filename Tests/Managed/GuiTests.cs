using HelixEngine;

namespace Tests
{
    // The immediate GUI from C#: controls in rects, a layout area with its scopes, a window with its function.
    public class CsGuiTests : GameBehaviour
    {
        bool m_Toggle;
        string m_Text = "ab";
        Rect m_Window = new Rect(200, 10, 120, 90);
        bool m_Painted;

        // The last control's rect on the screen, once (the test clicks it).
        readonly System.Collections.Generic.HashSet<string> m_Noted = new System.Collections.Generic.HashSet<string>();

        void NoteRect(string name, Event e)
        {
            if (e.type != EventType.Repaint || !m_Noted.Add(name))
                return;
            Rect r = GuiUtility.GuiToScreenRect(GuiLayoutUtility.GetLastRect());
            Notes.Note($"cs-rect-{name}:{(int)r.x}:{(int)r.y}:{(int)r.width}:{(int)r.height}");
        }

        void OnGui()
        {
            Event e = Event.current;
            if (Gui.Button(new Rect(10, 10, 80, 24), "Press"))
                Notes.Note("cs-button");
            bool t = Gui.Toggle(new Rect(10, 40, 80, 20), m_Toggle, "Toggle");
            if (t != m_Toggle)
            {
                m_Toggle = t;
                Notes.Note($"cs-toggle:{t}");
            }
            using (new GuiLayout.AreaScope(new Rect(10, 70, 160, 120)))
            {
                using (new GuiLayout.HorizontalScope())
                {
                    if (GuiLayout.Button("A"))
                        Notes.Note("cs-layout-a");
                    NoteRect("a", e);
                    if (GuiLayout.Button("B", GuiLayout.Width(40)))
                        Notes.Note("cs-layout-b");
                }
                string text = GuiLayout.TextField(m_Text);
                NoteRect("field", e);
                if (text != m_Text)
                {
                    m_Text = text;
                    Notes.Note($"cs-text:{text}");
                }
            }
            m_Window = Gui.Window(3, m_Window, id =>
            {
                if (Gui.Button(new Rect(8, 30, 80, 20), "In"))
                    Notes.Note("cs-window-button");
                Gui.DragWindow();
            }, "Win");
            if (e.type == EventType.Repaint && !m_Painted)
            {
                m_Painted = true;
                GuiStyle named = "button";
                Notes.Note($"cs-repaint:{Gui.skin.button.name}:{named.name}:{useGuiLayout}");
            }
        }
    }
}
