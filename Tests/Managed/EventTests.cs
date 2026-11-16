using HelixEngine;
using HelixEngine.UI;

namespace Tests
{
    // Callbacks of C# on a view's elements: the input of the frames, events sent from C#, the panel's own events, the
    // scheduler, capture and focus.
    public class CsEventTests : GameBehaviour
    {
        int m_Frame;
        Element m_Button, m_Extra;

        void Start()
        {
            Element root = GetComponent<UIView>().rootElement;
            m_Button = root.Find("button");
            Notes.Note($"found:{m_Button?.name}:{root.childCount}");
            root.RegisterCallback<PointerDownEvent>(e => Notes.Note($"root-down:{e.propagationPhase}:{(e.target as Element).name}"), TrickleDown.TrickleDown);
            m_Button.RegisterCallback<PointerDownEvent>(e => Notes.Note($"button-down:{e.propagationPhase}:{e.button}:{e.position.x}"));
            m_Button.RegisterCallback<ClickEvent>(e => Notes.Note($"click:{e.clickCount}:{e.localPosition.x}"));
            m_Button.RegisterCallback<KeyDownEvent>(e => Notes.Note($"key:{e.keyCode}:{(int)e.character}"));
            m_Button.RegisterCallback<NavigationSubmitEvent>(e =>
            {
                Notes.Note($"submit:{(e.target as Element).name}");
                e.StopPropagation();
            });
            m_Button.RegisterCallback<FocusEvent>(e => Notes.Note($"focus:{(e.relatedTarget == null)}"));
            m_Extra = new Element { name = "extra" };
            m_Extra.RegisterCallback<AttachToPanelEvent>(e => Notes.Note($"attached:{(e.target as Element).name}"));
            m_Extra.RegisterCallback<GeometryChangedEvent>(e => Notes.Note($"geometry:{e.newRect.width}"));
            m_Extra.style.width = 40;
            root.Add(m_Extra);
            m_Extra.schedule.Execute(() => Notes.Note("scheduled"));
        }

        void Update()
        {
            m_Frame++;
            if (m_Frame != 2)
                return;
            Notes.Note($"focused:{(m_Button.focusController.focusedElement as Element)?.name}");
            using (var submit = NavigationSubmitEvent.GetPooled())
                m_Button.SendEvent(submit);
            m_Button.CapturePointer(PointerId.mousePointerId);
            Notes.Note($"capture:{m_Button.HasPointerCapture(PointerId.mousePointerId)}:{(m_Button.panel.GetCapturingElement(0) as Element)?.name}");
            m_Button.ReleasePointer(PointerId.mousePointerId);
            Notes.Note($"released:{m_Button.HasPointerCapture(PointerId.mousePointerId)}:{m_Button.panel.Pick(new Vector2(20, 20))?.name}");
        }
    }
}
