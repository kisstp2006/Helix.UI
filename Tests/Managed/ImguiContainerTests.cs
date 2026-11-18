using HelixEngine;
using HelixEngine.UI;

namespace Tests
{
    // ImguiContainer from C#: a handler's controls drawn in the element's rectangle with the panel, a click on its
    // button by the element's pointer events, GuiLayout in the container's own layout.
    public class CsImguiContainerTests : GameBehaviour
    {
        sealed class Pointer : IPointerEvent
        {
            public int pointerId => 0;
            public string pointerType => HelixEngine.UI.PointerType.mouse;
            public bool isPrimary => true;
            public int button => 0;
            public int pressedButtons { get; set; }
            public Vector3 position { get; set; }
            public Vector3 localPosition => position;
            public Vector3 deltaPosition => Vector3.zero;
            public float deltaTime => 0;
            public int clickCount => 1;
            public float pressure => 0;
            public float tangentialPressure => 0;
            public float altitudeAngle => 0;
            public float azimuthAngle => 0;
            public float twist => 0;
            public Vector2 tilt => Vector2.zero;
            public HelixEngine.UI.PenStatus penStatus => HelixEngine.UI.PenStatus.None;
            public Vector2 radius => Vector2.zero;
            public Vector2 radiusVariance => Vector2.zero;
            public EventModifiers modifiers => EventModifiers.None;
            public bool shiftKey => false;
            public bool ctrlKey => false;
            public bool commandKey => false;
            public bool altKey => false;
            public bool actionKey => false;
        }

        ImguiContainer m_Container;
        int m_Frame, m_Repaints, m_Layouts;
        Rect m_LayoutRect;

        void Start()
        {
            Element page = GetComponent<UIView>().rootElement.Find("page");
            m_Container = new ImguiContainer(() =>
            {
                if (Event.current.type == EventType.Repaint)
                    m_Repaints++;
                if (Event.current.type == EventType.Layout)
                    m_Layouts++;
                if (Gui.Button(new Rect(10, 10, 80, 20), "Press"))
                    Notes.Note("pressed");
                GuiLayout.Space(40);
                GuiLayout.Label("Laid out");
                if (Event.current.type == EventType.Repaint)
                    m_LayoutRect = GuiLayoutUtility.GetLastRect();
            }) { name = "imgui" };
            m_Container.style.width = 200;
            m_Container.style.height = 100;
            m_Container.style.marginLeft = 30;
            page.Add(m_Container);
            Notes.Note($"class:{m_Container.ClassListContains(ImguiContainer.hssClassName)}");
        }

        void Send<T>(Vector2 local, int pressed) where T : PointerEventBase<T>, new()
        {
            Rect r = m_Container.worldBound;
            using T e = PointerEventBase<T>.GetPooled(new Pointer { position = new Vector3(r.x + local.x, r.y + local.y, 0), pressedButtons = pressed });
            m_Container.SendEvent(e);
        }

        void Update()
        {
            m_Frame++;
            if (m_Frame == 2)
            {
                Notes.Note($"repainted:{m_Repaints > 0}:{m_Layouts > 0}");
                // GuiLayout's label is under the space, in the container's own space.
                Notes.Note($"layout:{m_LayoutRect.y >= 40}:{m_LayoutRect.x < 20}");
                Send<PointerDownEvent>(new Vector2(30, 20), 1);
                Send<PointerUpEvent>(new Vector2(30, 20), 0);
                // Outside the button: nothing.
                Send<PointerDownEvent>(new Vector2(150, 80), 1);
                Send<PointerUpEvent>(new Vector2(150, 80), 0);
            }
            if (m_Frame == 3)
            {
                m_Container.Dispose();
                Notes.Note("disposed");
            }
        }
    }
}
