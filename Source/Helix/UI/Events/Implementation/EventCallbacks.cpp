#include <Helix/UI/Events/EventCallbacks.h>

namespace helix::ui
{
    void EventCallbacks::Register(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase)
    {
        if (!e.IsValid() || !callback)
            return;
        if (slots_.Size() <= e.index)
            slots_.Resize(e.index + 1);
        Array<Callback>& list = slots_[e.index];
        // Callbacks of an element that was in this slot before go.
        for (usize i = 0; i < list.Size();)
            if (list[i].generation != e.generation)
                list.RemoveAt(i);
            else
                i++;
        bool trickle = phase == TrickleDown::TrickleDown;
        for (const Callback& c : list)
            if (c.type == type && c.function == callback && c.user == user && c.trickleDown == trickle)
                return;
        list.Push(Callback{e.generation, type, trickle, callback, user});
    }

    void EventCallbacks::Unregister(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase)
    {
        if (e.index >= slots_.Size())
            return;
        Array<Callback>& list = slots_[e.index];
        bool trickle = phase == TrickleDown::TrickleDown;
        for (usize i = 0; i < list.Size(); i++)
            if (list[i].generation == e.generation && list[i].type == type && list[i].function == callback && list[i].user == user &&
                list[i].trickleDown == trickle)
            {
                list.RemoveAt(i);
                return;
            }
    }

    bool EventCallbacks::Has(ElementHandle e, EventTypeId type, bool trickleDown) const
    {
        for (const Callback& c : Of(e))
            if (c.generation == e.generation && c.type == type && c.trickleDown == trickleDown)
                return true;
        return false;
    }

    Span<const EventCallbacks::Callback> EventCallbacks::Of(ElementHandle e) const
    {
        if (e.index >= slots_.Size())
            return {};
        return Span<const Callback>(slots_[e.index].Data(), slots_[e.index].Size());
    }

    void EventCallbacks::Clear(ElementHandle e)
    {
        if (e.index < slots_.Size())
            slots_[e.index].Clear();
    }
}
