#pragma once

#include <Helix/UI/Events/EventCallbacks.h>
#include <Helix/UI/Layout/Layout.h>
#include <Helix/UI/Styles/StyleEngine.h>

// The element world: the elements of a process in one tree with their layout, styles and callbacks. An element exists
// without a panel and keeps itself, its callbacks and its state when it moves between panels; a panel is a root in the
// world with its own size, event state and drawing (each laid out and styled from its root).
namespace helix::ui
{
    class ElementWorld
    {
    public:
        ElementWorld() = default;
        ElementWorld(const ElementWorld&) = delete;
        ElementWorld& operator=(const ElementWorld&) = delete;

        ElementTree& tree() { return tree_; }
        LayoutEngine& layout() { return layout_; }
        StyleEngine& styles() { return styles_; }
        EventCallbacks& callbacks() { return callbacks_; }

        // The process's world, where the panels and the scripts' elements live.
        static ElementWorld& Main();

    private:
        ElementTree tree_;
        LayoutEngine layout_{tree_};
        StyleEngine styles_{tree_, layout_};
        EventCallbacks callbacks_;
    };
}
