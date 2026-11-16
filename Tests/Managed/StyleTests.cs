using HelixEngine;
using HelixEngine.UI;

namespace Tests
{
    // The looks of HSS from C#: transforms, backgrounds, text shadows and outlines, cursors in the inline style and
    // as resolved; custom properties and their event.
    public class CsStyleTests : GameBehaviour
    {
        static readonly CustomStyleProperty<Color> s_Accent = new CustomStyleProperty<Color>("--accent");
        static readonly CustomStyleProperty<float> s_Size = new CustomStyleProperty<float>("--size");
        static readonly CustomStyleProperty<string> s_Title = new CustomStyleProperty<string>("--title");
        static readonly CustomStyleProperty<bool> s_Flag = new CustomStyleProperty<bool>("--flag");
        static readonly CustomStyleProperty<int> s_Missing = new CustomStyleProperty<int>("--missing");

        Element m_Box;
        int m_Frame;

        void Start()
        {
            Element root = GetComponent<UIView>().rootElement;
            m_Box = root.Find("box");
            IStyle s = m_Box.style;
            s.translate = new Translate(new Length(10), Length.Percent(50));
            s.rotate = new Rotate(Angle.Turns(0.25f));
            s.scale = new Scale(new Vector2(2, 3));
            s.transformOrigin = new TransformOrigin(Length.Percent(0), new Length(4));
            s.backgroundPositionX = new BackgroundPosition(BackgroundPositionKeyword.Right, new Length(5));
            s.backgroundRepeat = new BackgroundRepeat(Repeat.Repeat, Repeat.NoRepeat);
            s.backgroundSize = new BackgroundSize(BackgroundSizeType.Cover);
            s.textShadow = new TextShadow { offset = new Vector2(1, 2), blurRadius = 3, color = new Color(0, 0, 1, 1) };
            s.helixTextOutlineWidth = 2;
            s.helixTextOverflowPosition = TextOverflowPosition.Middle;
            Notes.Note($"inline:{s.translate.value.x}:{s.translate.value.y}:{s.rotate.value.angle.ToDegrees()}:{s.scale.value.value.y}:" +
                       $"{s.transformOrigin.value.y}:{s.backgroundPositionX.value.keyword}:{s.backgroundPositionX.value.offset}:" +
                       $"{s.backgroundRepeat.value.x}:{s.backgroundSize.value.sizeType}:{s.textShadow.value.blurRadius}:{s.textShadow.value.color.b}:" +
                       $"{s.helixTextOutlineWidth.value}:{s.helixTextOverflowPosition.value}");
            // Null takes a property out of the inline style again.
            s.rotate = StyleKeyword.Null;
            Notes.Note($"unset:{s.rotate.keyword}");
            // A texture as the cursor, kept in the inline style as its object.
            var hand = new Texture2D(4, 4);
            s.cursor = new HelixEngine.UI.Cursor { texture = hand, hotspot = new Vector2(1, 2) };
            Notes.Note($"cursor:{s.cursor.value.texture == hand}:{s.cursor.value.hotspot.y}");
            m_Box.RegisterCallback<CustomStyleResolvedEvent>(e =>
            {
                bool accent = e.customStyle.TryGetValue(s_Accent, out Color c);
                e.customStyle.TryGetValue(s_Size, out float size);
                e.customStyle.TryGetValue(s_Title, out string title);
                e.customStyle.TryGetValue(s_Flag, out bool flag);
                bool missing = e.customStyle.TryGetValue(s_Missing, out int _);
                Notes.Note($"custom:{accent}:{c.g}:{size}:{title}:{flag}:{missing}");
            });
            try
            {
                new CustomStyleProperty<int>("size");
            }
            catch (System.ArgumentException)
            {
                Notes.Note("needs-dashes");
            }
        }

        void Update()
        {
            m_Frame++;
            if (m_Frame != 2)
                return;
            IResolvedStyle r = m_Box.resolvedStyle;
            // 50% of its 20 pixel height; the origin's 4 pixels.
            Notes.Note($"resolved:{r.translate.y}:{r.scale.value.x}:{r.transformOrigin.y}:{r.backgroundSize.sizeType}:{r.textShadow.offset.y}:" +
                       $"{r.helixTextOutlineWidth}:{r.helixTextOverflowPosition}");
            // Its bounds as shown: turned a quarter... the rotation was taken out, so scaled 2 x 3 about (0, 4).
            Rect b = m_Box.worldBound;
            Notes.Note($"bound:{b.width}:{b.height}");
        }
    }
}
