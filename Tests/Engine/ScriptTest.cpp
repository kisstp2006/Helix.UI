#include <Engine/Support/CppScripts.h>
#include <Engine/Support/UIHost.h>

#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Engine/Element.h>

using namespace helix;
using namespace helix::engine;
using namespace helix::ui;
using namespace helix::ui::testing;

HX_TEST(UIScripts, ElementsAreMadeNestedMovedAndQueriedFromCSharp)
{
    HX_REQUIRE(StartScripts());
    Notes().Clear();
    GameObject* host = GameObject::Create("Host");
    host->AddComponent(CSharp("CsElementTests").type);
    RunScriptPhase(ScriptMessage::Update);
    HX_CHECK(NotesAre({"children:3:acb:2:True", "order:bca", "placed:cba", "removed:2:True:ArgumentException:ArgumentOutOfRangeException:ArgumentOutOfRangeException",
                       "classes:True:False:card,big", "find:inner:inner:2:True", "ancestors:b:root:True:False",
                       "state:False:False:True:3:Ignore:False", "userData:True"}));
    DestroyImmediate(host);
}

HX_TEST(UIScripts, AnElementLivesWhileATreeOrAScriptHoldsIt)
{
    HX_REQUIRE(RegisterModules());
    ElementWorld& world = ElementWorld::Main();
    // C++ makes the objects here: one in a tree, one alone.
    auto* parent = new Element();
    auto* child = new Element();
    parent->Add(child);
    ElementHandle childHandle = child->handle();
    HX_CHECK(child->parent() == parent && parent->childCount() == 1);
    // The parent's object goes: the parent (no tree holds it) goes with its subtree, and with it the child's object.
    delete parent;
    HX_CHECK(!world.tree().IsAlive(childHandle));
    // An element of a panel's markup reached from C++: the same object each time.
    auto* a = new Element();
    ElementHandle h = a->handle();
    HX_CHECK(Element::Of(h) == a);
    delete a;
    HX_CHECK(!world.tree().IsAlive(h));
}

HX_TEST(UIScripts, CppScriptsReachElementsThroughTheScriptApi)
{
    HX_REQUIRE(RegisterModules());
    CppNotes().Clear();
    GameObject* host = GameObject::Create("CppHost");
    host->AddComponent(scripting::TypeOf<CppElementScript>());
    RunScriptPhase(ScriptMessage::Update);
    if (!CppNotes().Empty())
        std::printf("    %s\n", CppNotes()[0].CStr());
    HX_CHECK(CppNotes().Size() == 1 && CppNotes()[0] == StringView("cpp:2:b:true:true:1"));
    DestroyImmediate(host);
}
