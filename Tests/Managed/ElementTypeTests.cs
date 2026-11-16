using HelixEngine;
using HelixEngine.UI;

namespace Tests
{
    public enum Mood { Calm, VeryHappy }

    // An element type of a C# class: its attributes from markup, its own children (one made by a field initializer,
    // before its base constructor runs), its content container for the markup's children.
    [HxmlElement]
    public class CsCard : Element
    {
        [HxmlAttribute] public string title { get; set; }
        [HxmlAttribute] public int count;
        [HxmlAttribute("accent-color")] Color m_Accent;
        [HxmlAttribute] public Mood mood { get; set; }

        readonly Element m_Content = new Element { name = "content" };

        public Element header { get; }
        public Color accent => m_Accent;
        public override Element contentContainer => m_Content;

        public CsCard()
        {
            header = new Element { name = "header" };
            hierarchy.Add(header);
            hierarchy.Add(m_Content);
        }
    }

    // Another markup name; the base's attributes too.
    [HxmlElement("fancy-card")]
    public class CsFancy : CsCard
    {
        [HxmlAttribute] public bool shiny;
    }

    public class CsElementTypeTests : GameBehaviour
    {
        int m_Frame;

        void Start()
        {
            var view = GetComponent<UIView>();
            Element root = view.rootElement;
            var card = root.Find("card") as CsCard;
            Notes.Note($"card:{card != null}:{card?.title}:{card?.count}:{card?.accent.r}:{card?.mood}:{card?.header.name}:{card?.childCount}:" +
                       $"{card?.hierarchy.childCount}");
            // The markup's child is in the card's content container.
            Notes.Note($"inner:{root.Find("inner")?.parent?.name}");
            var fancy = root.Find("fancy") as CsFancy;
            Notes.Note($"fancy:{fancy?.shiny}:{fancy?.title}:{fancy?.GetType().Name}");
            // The template from a script: a container of its elements, or its elements into an element.
            TemplateContainer instance = view.source.Instantiate();
            Notes.Note($"instance:{instance.templateSource == view.source}:{instance.childCount}:{instance.Find("card") is CsCard}");
            var target = new Element();
            view.source.CloneTree(target, out int first, out int added);
            Notes.Note($"clone:{first}:{added}");
            // The page's sheet, added to its root by the markup.
            ElementStyleSheetSet sheets = root.styleSheets;
            StyleSheet sheet = sheets.count > 0 ? sheets[0] : null;
            Notes.Note($"sheets:{sheets.count}:{sheet != null}:{sheets.Contains(sheet)}");
            sheets.Remove(sheet);
            Notes.Note($"removed:{sheets.count}:{sheets.Contains(sheet)}");
            card.styleSheets.Add(sheet);
            Notes.Note($"moved:{card.styleSheets.count}:{card.styleSheets[0] == sheet}");
            // The C++ class's elements are of its type.
            Notes.Note($"gauge:{root.Find("gauge").resolvedStyle.opacity}");
            // A new card from a script, into the card (its content container): its type is its class's name.
            var made = new CsCard { name = "made" };
            card.Add(made);
            Notes.Note($"made:{made.parent?.name}");
        }

        void Update()
        {
            if (++m_Frame != 2)
                return;
            Element root = GetComponent<UIView>().rootElement;
            // The sheet is the card's now: its type selector reaches the card and the new card in it, not the gauge.
            Notes.Note($"typed:{root.Find("card").resolvedStyle.opacity}:{root.Find("made").resolvedStyle.opacity}:{root.Find("gauge").resolvedStyle.opacity}");
        }
    }
}
