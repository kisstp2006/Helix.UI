#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/World/ElementWorld.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::ui;

namespace
{
    // A root in a world as a panel makes one: filling its box, not picked itself.
    ElementHandle Root(ElementWorld& world)
    {
        ElementHandle root = world.tree().Create(Intern("Element"));
        world.tree().SetPicking(root, PickingMode::Ignore);
        world.styles().SetInlineStyle(root, "width: 100%; height: 100%;");
        return root;
    }

    ElementHandle Box(ElementWorld& world, f32 x, f32 y, f32 size)
    {
        ElementHandle e = world.tree().Create(Intern("Element"));
        String style = Format("position: absolute; left: {}px; top: {}px; width: {}px; height: {}px;", x, y, size, size);
        world.styles().SetInlineStyle(e, style.View());
        return e;
    }
}

HX_TEST(ElementWorld, AnElementKeepsItsCallbacksWhenItMovesToAnotherPanel)
{
    ElementWorld world;
    ElementHandle a = Root(world), b = Root(world);
    EventSystem first(world.tree(), world.layout(), world.styles(), &world.callbacks());
    EventSystem second(world.tree(), world.layout(), world.styles(), &world.callbacks());
    first.SetRoot(a);
    second.SetRoot(b);
    // An element made apart from any panel, its callback registered before it is in one.
    ElementHandle box = Box(world, 10, 10, 20);
    i32 clicks = 0;
    world.callbacks().Register(box, Events().click, [](void* u, EventSystem&, UIEvent&) { (*static_cast<i32*>(u))++; }, &clicks,
                               TrickleDown::NoTrickleDown);
    world.tree().Append(a, box);
    auto update = [&](ElementHandle root, f32 size) {
        world.styles().Update(root, 0);
        world.layout().Update(root, size, size);
    };
    update(a, 100);
    update(b, 50);
    first.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(15, 15), 0, 0);
    first.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(15, 15), 0, 0.05);
    HX_CHECK(clicks == 1 && !second.Pick(Vector2(15, 15)).IsValid());
    // Moved: the second panel picks it and its callback still runs; the first no longer has it.
    world.tree().Append(b, box);
    update(a, 100);
    update(b, 50);
    HX_CHECK(second.Pick(Vector2(15, 15)) == box && !first.Pick(Vector2(15, 15)).IsValid());
    second.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(15, 15), 0, 1);
    second.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(15, 15), 0, 1.05);
    HX_CHECK(clicks == 2);
    // Each root is laid out in its own size.
    HX_CHECK(world.layout().WorldRect(a).width == 100 && world.layout().WorldRect(b).width == 50);
}

HX_TEST(ElementWorld, ASlotUsedAgainHasNoneOfTheOldElementsCallbacks)
{
    ElementWorld world;
    ElementHandle root = Root(world);
    EventSystem events(world.tree(), world.layout(), world.styles(), &world.callbacks());
    events.SetRoot(root);
    ElementHandle old = Box(world, 0, 0, 10);
    i32 calls = 0;
    world.callbacks().Register(old, Events().click, [](void* u, EventSystem&, UIEvent&) { (*static_cast<i32*>(u))++; }, &calls,
                               TrickleDown::NoTrickleDown);
    world.tree().Destroy(old);
    ElementHandle fresh = Box(world, 0, 0, 10);
    HX_CHECK(fresh.index == old.index && fresh.generation != old.generation);
    HX_CHECK(!world.callbacks().Has(fresh, Events().click, false));
    world.tree().Append(root, fresh);
    world.styles().Update(root, 0);
    world.layout().Update(root, 100, 100);
    events.PointerDown(PointerId::mouse, PointerKind::Mouse, 0, Vector2(5, 5), 0, 0);
    events.PointerUp(PointerId::mouse, PointerKind::Mouse, 0, Vector2(5, 5), 0, 0.05);
    HX_CHECK(calls == 0);
}
