#pragma once

#include <Helix/Core/Strings/Format.h>
// The engine's script API before the UI's (the UI's classes derive from the engine's).
#include <HelixEngine/HelixEngineApi.h>
#include <HelixEngine.UI/HelixUIApi.h>

// A C++ script of the tests on the UI's C++ script API (the test program's game module, UIEngineTests).
namespace helix::ui::testing
{
    inline Array<String>& CppNotes()
    {
        static Array<String> notes;
        return notes;
    }

    HX_CLASS(Namespace = "UIEngineTests")
    class CppElementScript : public ::HelixEngine::GameBehaviour
    {
    public:
        void Start()
        {
            // The script API's handle (helix::ui::Element is the engine's class).
            using Element = ::HelixEngine::UI::Element;
            Element root = Element::New();
            Element a = Element::New();
            Element b = Element::New();
            a.name = "a";
            b.name = "b";
            root.Add(a);
            root.Add(b);
            b.SendToBack();
            a.AddToClassList("card");
            a.focusable = true;
            String first = root.ElementAtChecked(0).name;
            CppNotes().Push(Format("cpp:{}:{}:{}:{}:{}", i32(root.childCount), first.View(), a.ClassListContains("card"), bool(a.focusable),
                                   root.IndexOf(a)));
        }
    };
}
