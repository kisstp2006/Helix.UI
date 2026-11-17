using System.Collections.Generic;
using HelixEngine;
using HelixEngine.UI;

namespace Tests
{
    public enum Fruit { Apple, Pear, Plum }

    // The controls from C#: their values and ChangeEvents, a button's clicks, the generic fields, the markup's
    // controls as C# objects.
    public class CsControlTests : GameBehaviour
    {
        Button m_Button;
        int m_Frame;

        void Start()
        {
            Element root = GetComponent<UIView>().rootElement;
            var toggle = new Toggle("Sound");
            root.Add(toggle);
            toggle.RegisterValueChangedCallback(e => Notes.Note($"toggle:{e.previousValue}:{e.newValue}"));
            toggle.value = true;
            toggle.SetValueWithoutNotify(false);
            toggle.text = "on";
            Notes.Note($"toggle-state:{toggle.value}:{toggle.text}:{toggle.label}:{toggle.ClassListContains(Toggle.hssClassName)}");

            var slider = new Slider("Volume", 0, 10);
            root.Add(slider);
            slider.RegisterValueChangedCallback(e => Notes.Note($"slider:{e.previousValue}:{e.newValue}"));
            slider.value = 25;
            slider.lowValue = 2;
            Notes.Note($"slider-state:{slider.value}:{slider.lowValue}:{slider.highValue}");

            var integer = new IntegerField("Count");
            root.Add(integer);
            integer.RegisterValueChangedCallback(e => Notes.Note($"int:{e.previousValue}:{e.newValue}:{integer.text}"));
            integer.value = 7;

            var vector = new Vector3Field("Position");
            root.Add(vector);
            vector.RegisterValueChangedCallback(e => Notes.Note($"vector:{e.newValue.x}:{e.newValue.y}:{e.newValue.z}"));
            vector.value = new Vector3(1, 2, 3);

            var text = new TextField("Name") { maxLength = 4 };
            root.Add(text);
            text.RegisterValueChangedCallback(e => Notes.Note($"text:{e.previousValue}:{e.newValue}"));
            text.value = "Helix";
            Notes.Note($"text-state:{text.value}:{text.text}:{text.maxLength}");

            var popup = new PopupField<int>("Size", new List<int> { 10, 20, 30 }, 1);
            root.Add(popup);
            popup.RegisterValueChangedCallback(e => Notes.Note($"popup:{e.previousValue}:{e.newValue}:{popup.index}"));
            popup.value = 30;

            var fruit = new EnumField("Fruit", Fruit.Pear);
            root.Add(fruit);
            fruit.RegisterValueChangedCallback(e => Notes.Note($"enum:{e.previousValue}:{e.newValue}"));
            fruit.value = Fruit.Plum;

            var mask = new MaskField(new List<string> { "A", "B", "C" }, 0);
            root.Add(mask);
            mask.RegisterValueChangedCallback(e => Notes.Note($"mask:{e.newValue}"));
            mask.value = 5;

            var foldout = new Foldout { text = "More" };
            root.Add(foldout);
            foldout.RegisterValueChangedCallback(e => Notes.Note($"foldout:{e.previousValue}:{e.newValue}"));
            foldout.value = false;

            var hash = new Hash128Field();
            root.Add(hash);
            hash.value = Hash128.Compute("helix");
            Notes.Note($"hash:{hash.value == Hash128.Compute("helix")}:{hash.text == Hash128.Compute("helix").ToString()}");

            var group = new ToggleButtonGroup("Pick");
            root.Add(group);

            m_Button = new Button(() => Notes.Note("clicked")) { name = "cs-button", text = "Go" };
            m_Button.style.position = Position.Absolute;
            m_Button.style.left = 0;
            m_Button.style.top = 0;
            m_Button.style.width = 40;
            m_Button.style.height = 20;
            root.Add(m_Button);

            // The markup's controls are C# objects of their classes.
            var label = root.Find<Label>("label");
            var dropdown = root.Find<DropdownField>("dropdown");
            Notes.Note($"markup:{label?.text}:{dropdown?.value}:{dropdown?.index}:{string.Join("|", dropdown?.choices ?? new List<string>())}");
            dropdown.RegisterValueChangedCallback(e => Notes.Note($"dropdown:{e.previousValue}:{e.newValue}"));
            dropdown.index = 2;
        }

        void Update()
        {
            if (++m_Frame == 3)
                Notes.Note($"button:{m_Button.clickCount}");
        }
    }
}
