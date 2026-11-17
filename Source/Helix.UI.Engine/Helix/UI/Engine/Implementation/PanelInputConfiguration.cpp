#include <Helix/UI/Engine/PanelInputConfiguration.h>

namespace helix::ui
{
    namespace
    {
        Array<PanelInputConfiguration*>& Enabled()
        {
            static Array<PanelInputConfiguration*>* list = new Array<PanelInputConfiguration*>();
            return *list;
        }
    }

    Array<engine::Camera*> PanelInputConfiguration::eventCameras() const
    {
        Array<engine::Camera*> out;
        for (const engine::PPtr<engine::Camera>& c : eventCameras_)
            if (engine::Camera* camera = c.Get())
                out.Push(camera);
        return out;
    }

    void PanelInputConfiguration::SetEventCameras(Span<engine::Camera* const> cameras)
    {
        eventCameras_.Clear();
        for (engine::Camera* c : cameras)
            eventCameras_.Push(engine::PPtr<engine::Camera>(c));
    }

    void PanelInputConfiguration::OnEnable() { Enabled().Push(this); }

    void PanelInputConfiguration::OnDisable()
    {
        Array<PanelInputConfiguration*>& list = Enabled();
        for (usize i = 0; i < list.Size(); i++)
            if (list[i] == this)
            {
                list.RemoveAt(i);
                break;
            }
    }

    PanelInputConfiguration* PanelInputConfiguration::current()
    {
        Array<PanelInputConfiguration*>& list = Enabled();
        return list.Empty() ? nullptr : list[0];
    }
}
