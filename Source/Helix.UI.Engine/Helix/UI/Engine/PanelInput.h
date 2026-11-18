#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Math/Vector.h>

// The runtime panels as an event system outside the UI sees them. While the panels' input is redirected (the input
// configuration's AlwaysRedirect, or AutoRedirect while such an event system takes the input) they read none of the
// window's input themselves: the event system hands each panel the pointers its raycaster hit and the keys while the
// panel is selected. Panels go by ids that stay while they live; positions are the screen's pixels from the top left.
namespace helix::ui
{
    class EventSystem;
    class UIView;

    class PanelInput
    {
    public:
        // An event system that redirects the panels' input is there (AutoRedirect redirects only then).
        static void SetEventSystemPresent(bool value);
        // Whether the panels read the window's input themselves now.
        static bool ReadsOwnInput();

        // The panels that take input, the top-most first.
        static Array<u64> Panels();
        static bool Alive(u64 panel);
        // Its settings' name (or its first view's GameObject's).
        static String Name(u64 panel);
        static f32 SortingOrder(u64 panel);
        // Its place among the panels that take input (0 the top-most); -1 when it is gone.
        static i32 InputIndex(u64 panel);
        static bool IsWorld(u64 panel);
        // Where a screen point is on the panel and how far (0 on the screen); false when it is not on it.
        static bool ScreenToPanel(u64 panel, Vector2 screen, Vector2& at, f32& distance);
        // Whether an element is at the panel's point.
        static bool Pick(u64 panel, Vector2 at);
        static EventSystem* Events(u64 panel);
        // Its first view (null when it is gone).
        static UIView* FirstView(u64 panel);
        // The panel the keys go to when they are read by the panels themselves.
        static void SetKeyboardPanel(u64 panel);
    };
}
