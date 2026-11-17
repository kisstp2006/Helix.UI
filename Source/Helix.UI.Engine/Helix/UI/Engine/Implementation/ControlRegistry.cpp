#include <Helix/UI/Engine/Containers.h>
#include <Helix/UI/Engine/Controls.h>
#include <Helix/UI/Engine/Fields.h>
#include <Helix/UI/Markup/Hxml.h>

namespace helix::ui
{
    namespace
    {
        // The element objects' own attributes of every element (the markup's trees outside of the main world have
        // no objects: they are taken and dropped).
        Element* ObjectOf(ElementTree& tree, ElementHandle e)
        {
            ElementWorld& world = ElementWorld::Main();
            return &tree == &world.tree() ? Element::Of(world, e) : nullptr;
        }

        bool TooltipAttribute(ElementTree& tree, ElementHandle e, StringView, StringView value)
        {
            if (Element* x = ObjectOf(tree, e))
                x->SetTooltip(value);
            return true;
        }

        bool ViewDataKeyAttribute(ElementTree& tree, ElementHandle e, StringView, StringView value)
        {
            if (Element* x = ObjectOf(tree, e))
                x->SetViewDataKey(value);
            return true;
        }
    }

    void RegisterControls()
    {
        static bool done = false;
        if (done)
            return;
        done = true;
        RegisterCommonAttribute("tooltip", &TooltipAttribute);
        RegisterCommonAttribute("view-data-key", &ViewDataKeyAttribute);
        RegisterControl<BindableElement>("BindableElement");
        RegisterControl<TextElement>("TextElement");
        RegisterControl<Label>("Label");
        RegisterControl<Button>("Button");
        RegisterControl<RepeatButton>("RepeatButton");
        RegisterControl<Image>("Image");
        RegisterControl<Box>("Box");
        RegisterControl<GroupBox>("GroupBox");
        RegisterControl<HelpBox>("HelpBox");
        RegisterControl<PopupWindow>("PopupWindow");
        RegisterControl<ProgressBar>("ProgressBar");
        RegisterControl<AbstractBaseField>("AbstractBaseField");
        RegisterControl<Toggle>("Toggle");
        RegisterControl<RadioButton>("RadioButton");
        RegisterControl<RadioButtonGroup>("RadioButtonGroup");
        RegisterControl<ToggleButtonGroup>("ToggleButtonGroup");
        RegisterControl<Slider>("Slider");
        RegisterControl<SliderInt>("SliderInt");
        RegisterControl<MinMaxSlider>("MinMaxSlider");
        RegisterControl<DropdownField>("DropdownField");
        RegisterControl<EnumField>("EnumField");
        RegisterControl<MaskField>("MaskField");
        RegisterControl<Mask64Field>("Mask64Field");
        RegisterControl<TextField>("TextField");
        RegisterControl<IntegerField>("IntegerField");
        RegisterControl<LongField>("LongField");
        RegisterControl<UnsignedIntegerField>("UnsignedIntegerField");
        RegisterControl<UnsignedLongField>("UnsignedLongField");
        RegisterControl<FloatField>("FloatField");
        RegisterControl<DoubleField>("DoubleField");
        RegisterControl<Hash128Field>("Hash128Field");
        RegisterControl<Vector2Field>("Vector2Field");
        RegisterControl<Vector3Field>("Vector3Field");
        RegisterControl<Vector4Field>("Vector4Field");
        RegisterControl<Vector2IntField>("Vector2IntField");
        RegisterControl<Vector3IntField>("Vector3IntField");
        RegisterControl<RectField>("RectField");
        RegisterControl<RectIntField>("RectIntField");
        RegisterControl<BoundsField>("BoundsField");
        RegisterControl<BoundsIntField>("BoundsIntField");
        RegisterControl<Foldout>("Foldout");
        RegisterControl<Scroller>("Scroller");
        RegisterControl<ScrollView>("ScrollView");
        RegisterControl<Tab>("Tab");
        RegisterControl<TabView>("TabView");
        RegisterControl<TwoPaneSplitView>("TwoPaneSplitView");
    }

    namespace
    {
        // The controls' markup types are there once the module is loaded.
        struct ControlRegistrar
        {
            ControlRegistrar() { RegisterControls(); }
        } registrar;
    }
}
