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

    // Controls made and driven from a C++ script: built through the script API, their values typed, their events
    // (clicked, valueChanged, activeTabChanged) heard.
    HX_CLASS(Namespace = "UIEngineTests")
    class CppControlScript : public ::HelixEngine::GameBehaviour
    {
    public:
        void Start()
        {
            namespace api = ::HelixEngine::UI;
            api::UIView view = GetComponent<api::UIView>();
            api::Element root = view.rootElement;
            api::Button button = api::Button::New();
            button.name = "cpp-button";
            button.text = "Go";
            root.Add(button);
            button.clicked.Add({[](void*) { CppNotes().Push(String("cpp-clicked")); }, nullptr});
            api::Toggle toggle = api::Toggle::New();
            toggle.name = "cpp-toggle";
            toggle.text = "Shadows";
            root.Add(toggle);
            toggle.valueChanged.Add({[](void*) { CppNotes().Push(String("cpp-toggle-changed")); }, nullptr});
            api::Slider slider = api::Slider::New();
            slider.name = "cpp-slider";
            slider.label = "Volume";
            root.Add(slider);
            slider.valueChanged.Add({[](void*) { CppNotes().Push(String("cpp-slider-changed")); }, nullptr});
            slider.value = 40.0f;
            api::TextField text = api::TextField::New();
            text.value = "Helix";
            root.Add(text);
            api::IntegerField number = api::IntegerField::New();
            number.value = 7;
            root.Add(number);
            api::Foldout foldout = api::Foldout::New();
            foldout.text = "More";
            root.Add(foldout);
            foldout.valueChanged.Add({[](void*) { CppNotes().Push(String("cpp-foldout-changed")); }, nullptr});
            foldout.value = false;
            CppNotes().Push(Format("cpp-values:{}:{}:{}:{}:{}", f32(slider.value), String(text.value).View(), i32(number.value),
                                   bool(toggle.value), bool(foldout.value)));
        }
    };

    // The test's OnGui for a C++ script (the native GUI's calls), told the script's GameObject.
    inline void (*&GuiHook())(StringView name)
    {
        static void (*hook)(StringView) = nullptr;
        return hook;
    }

    HX_CLASS(Namespace = "UIEngineTests")
    class CppGuiHook : public ::HelixEngine::GameBehaviour
    {
    public:
        void OnGui()
        {
            if (GuiHook())
            {
                String n = name;
                GuiHook()(n.View());
            }
        }
    };

    // The immediate GUI from the C++ script API: a button, a layout area, a window whose function is the event's.
    HX_CLASS(Namespace = "UIEngineTests")
    class CppGuiScript : public ::HelixEngine::GameBehaviour
    {
    public:
        void Start()
        {
            token_ = ::HelixEngine::Gui::windowFunctionCalled.Add({[](void*, ::helix::i32 id) {
                                                                       if (id != 7)
                                                                           return;
                                                                       ::HelixEngine::Gui::Label(::helix::Rect(8, 24, 100, 20), "Inside");
                                                                       if (::HelixEngine::Gui::Button(::helix::Rect(8, 48, 80, 20), "Close"))
                                                                           CppNotes().Push(String("cpp-window-button"));
                                                                       ::HelixEngine::Gui::DragWindow();
                                                                   },
                                                                   nullptr});
        }

        void OnDestroy() { ::HelixEngine::Gui::windowFunctionCalled.Remove(token_); }

        void OnGui()
        {
            namespace api = ::HelixEngine;
            if (api::Gui::Button(::helix::Rect(10, 10, 80, 24), "Press"))
                CppNotes().Push(String("cpp-gui-button"));
            api::GuiLayout::BeginArea(::helix::Rect(10, 50, 160, 100));
            if (api::GuiLayout::Button("Layout"))
                CppNotes().Push(String("cpp-layout-button"));
            api::GuiLayout::EndArea();
            api::GuiStyle windowStyle = api::Gui::skin.Get().window;
            window_ = api::Gui::WindowInternal(7, window_, api::GuiContent::New("Win"), windowStyle, false);
        }

    private:
        ::helix::Rect window_ = ::helix::Rect(200, 10, 120, 90);
        ::helix::u64 token_ = 0;
    };
}
