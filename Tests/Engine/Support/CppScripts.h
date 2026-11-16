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

    // An element type of a C++ class: made over its element (its handle refers to it in its constructor), its fields
    // set from markup, gone with its element.
    HX_CLASS(Namespace = "UIEngineTests", HxmlElement)
    class CppGauge : public ::HelixEngine::UI::Element
    {
    public:
        CppGauge()
        {
            ::HelixEngine::UI::Element bar = ::HelixEngine::UI::Element::New();
            bar.name = "bar";
            Add(bar);
            CppNotes().Push(Format("gauge-made:{}", i32(childCount)));
        }
        ~CppGauge() { CppNotes().Push(String("gauge-gone")); }

        HX_FIELD(HxmlAttribute) f32 value = 0;
        HX_FIELD(HxmlAttribute) String label;
        HX_FIELD(HxmlAttribute) i32 maxValue = 1;
        HX_FIELD(HxmlAttribute = "tint") Color color;
        // A property: markup sets it through its setter, where the class reacts.
        HX_PROPERTY(HxmlAttribute) f32 level() const { return level_; }
        void SetLevel(f32 v)
        {
            level_ = v;
            CppNotes().Push(Format("level:{}", v));
        }

    private:
        f32 level_ = 0;
    };

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
