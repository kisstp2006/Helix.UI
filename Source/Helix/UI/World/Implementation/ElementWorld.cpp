#include <Helix/UI/World/ElementWorld.h>

namespace helix::ui
{
    ElementWorld& ElementWorld::Main()
    {
        // Never destroyed: panels and scripts may let their elements go late at exit.
        static ElementWorld* world = new ElementWorld();
        return *world;
    }
}
