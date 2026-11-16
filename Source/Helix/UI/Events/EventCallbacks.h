#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/UI/Events/UIEvent.h>

// The callbacks of elements: per element (by its slot, a callback of an element that was in the slot before is
// dropped), its type and phase. They belong to the element, not to a panel: an element keeps them when it moves.
namespace helix::ui
{
    class EventSystem;

    using EventCallback = void (*)(void* user, EventSystem& events, UIEvent& e);

    enum class TrickleDown : u8
    {
        NoTrickleDown,
        TrickleDown
    };

    class EventCallbacks
    {
    public:
        struct Callback
        {
            u32 generation = 0;
            EventTypeId type = kNoEventType;
            bool trickleDown = false;
            EventCallback function = nullptr;
            void* user = nullptr;
        };

        // The same callback, user and phase again is not added twice.
        void Register(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase);
        void Unregister(ElementHandle e, EventTypeId type, EventCallback callback, void* user, TrickleDown phase);
        // Whether the element has a callback of the type in the phase.
        bool Has(ElementHandle e, EventTypeId type, bool trickleDown) const;
        // The element's callbacks (the caller filters by type and phase; callbacks of a former element are skipped).
        Span<const Callback> Of(ElementHandle e) const;
        // All of an element's callbacks go (when it is destroyed).
        void Clear(ElementHandle e);

    private:
        Array<Array<Callback>> slots_;
    };
}
