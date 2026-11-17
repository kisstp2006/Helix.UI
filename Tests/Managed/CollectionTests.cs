using System;
using System.Collections.Generic;
using System.Linq;
using HelixEngine;
using HelixEngine.Properties;
using HelixEngine.UI;
using HelixEngine.UI.Experimental;

namespace Tests
{
    public class BoundModel : INotifyBindablePropertyChanged
    {
        string m_Title = "first";
        public event EventHandler<BindablePropertyChangedEventArgs> propertyChanged;

        [CreateProperty]
        public string title
        {
            get => m_Title;
            set
            {
                m_Title = value;
                propertyChanged?.Invoke(this, new BindablePropertyChangedEventArgs("title"));
            }
        }

        public int count = 3;
        public float level = 0.5f;
    }

    // Collections, menus, animations, painting and data binding from C#.
    public class CsCollectionTests : GameBehaviour
    {
        int m_Frame;
        ListView m_List;
        TreeView m_Tree;
        MultiColumnListView m_Columns;
        Element m_Painted, m_Root;
        BoundModel m_Model;
        Label m_Bound;
        IntegerField m_Count;
        ValueAnimation<float> m_Animation;
        float m_Animated;
        bool m_Completed;
        int m_Paints;
        List<string> m_Items;

        void Start()
        {
            m_Root = GetComponent<UIView>().rootElement;
            Element page = m_Root.Find("page");

            // A list of 100 items in 100 pixels: only the rows in view are made.
            m_Items = Enumerable.Range(0, 100).Select(i => $"item {i}").ToList();
            m_List = new ListView(m_Items, 20, () => new Label(), (e, i) => ((Label)e).text = m_Items[i]) { name = "list" };
            m_List.style.height = 100;
            m_List.selectionChanged += items => Notes.Note($"selected:{string.Join("|", items)}");
            m_List.itemsChosen += items => Notes.Note($"chosen:{items.Count()}");
            page.Add(m_List);
            m_List.SetSelection(2);

            // A tree: roots and children, expanded on request.
            m_Tree = new TreeView();
            m_Tree.style.height = 120;
            m_Tree.SetRootItems(new List<TreeViewItemData<string>>
            {
                new TreeViewItemData<string>(1, "fruit", new List<TreeViewItemData<string>> { new TreeViewItemData<string>(2, "apple"), new TreeViewItemData<string>(3, "pear") }),
                new TreeViewItemData<string>(4, "vegetable"),
            });
            page.Add(m_Tree);
            Notes.Note($"tree:{m_Tree.viewController.GetItemsCount()}");
            m_Tree.ExpandItem(1);
            Notes.Note($"expanded:{m_Tree.viewController.GetItemsCount()}:{m_Tree.GetItemDataForIndex<string>(1)}");

            // Columns, sorted by the second descending.
            var rows = new List<(string name, int size)> { ("a", 3), ("b", 1), ("c", 2) };
            m_Columns = new MultiColumnListView { name = "columns" };
            m_Columns.style.height = 100;
            m_Columns.columns.Add(new Column { name = "name", title = "Name", bindCell = (e, i) => ((Label)e).text = rows[i].name });
            m_Columns.columns.Add(new Column { name = "size", title = "Size", bindCell = (e, i) => ((Label)e).text = rows[i].size.ToString(), comparison = (x, y) => rows[x].size.CompareTo(rows[y].size) });
            m_Columns.itemsSource = rows;
            m_Columns.sortingMode = ColumnSortingMode.Default;
            m_Columns.sortColumnDescriptions.Add(new SortColumnDescription("size", SortDirection.Descending));
            page.Add(m_Columns);

            // Menus: a dropdown menu's statuses, a contextual menu built by the event, a generic menu shown.
            var menu = new DropdownMenu();
            menu.AppendAction("Open", _ => Notes.Note("open"));
            menu.AppendAction("Locked", _ => { }, DropdownMenuAction.Status.Disabled);
            menu.AppendSeparator();
            menu.AppendAction("Hidden", _ => { }, DropdownMenuAction.Status.Hidden);
            menu.PrepareForDisplay(null);
            Notes.Note($"menu:{menu.MenuItems().Count}:{((DropdownMenuAction)menu.MenuItems()[1]).status}");
            var target = new Element { name = "menu-target" };
            target.AddManipulator(new ContextualMenuManipulator(e => e.menu.AppendAction("Rename", _ => Notes.Note("renamed"))));
            page.Add(target);
            var generic = new GenericDropdownMenu();
            generic.AddItem("One", true, () => Notes.Note("one"));
            generic.AddSeparator("");
            generic.AddDisabledItem("Two", false);
            generic.DropDown(new Rect(10, 10, 80, 20), page);
            Notes.Note($"generic:{m_Root.panel.visualTree.FindAll(null, GenericDropdownMenu.itemHssClassName).Count}");

            // An animation over 50 ms, and the easings.
            m_Animation = page.experimental.animation.Start(0f, 10f, 50, (e, v) => m_Animated = v);
            m_Animation.OnCompleted(() => m_Completed = true);
            Notes.Note($"easing:{Easing.Linear(0.25f)}:{Easing.InQuad(0.5f)}:{Easing.OutQuad(0.5f)}");

            // Painting: a box filled by the element's painter.
            m_Painted = new Element { name = "painted" };
            m_Painted.style.width = 40;
            m_Painted.style.height = 40;
            m_Painted.generateVisualContent = ctx =>
            {
                m_Paints++;
                Painter2D p = ctx.painter2D;
                p.fillColor = Color.red;
                p.BeginPath();
                p.MoveTo(new Vector2(0, 0));
                p.LineTo(new Vector2(40, 0));
                p.LineTo(new Vector2(40, 40));
                p.ClosePath();
                p.Fill();
                MeshWriteData mesh = ctx.Allocate(3, 3);
                mesh.SetNextVertex(new Vertex { position = new Vector3(0, 0, Vertex.nearZ), tint = new Color32(0, 255, 0, 255) });
                mesh.SetNextVertex(new Vertex { position = new Vector3(10, 0, Vertex.nearZ), tint = new Color32(0, 255, 0, 255) });
                mesh.SetNextVertex(new Vertex { position = new Vector3(0, 10, Vertex.nearZ), tint = new Color32(0, 255, 0, 255) });
                mesh.SetNextIndex(0);
                mesh.SetNextIndex(1);
                mesh.SetNextIndex(2);
            };
            page.Add(m_Painted);

            // A vector image of a painter of its own.
            var image = ScriptableObject.CreateInstance<VectorImage>();
            using (var painter = new Painter2D())
            {
                painter.BeginPath();
                painter.Arc(new Vector2(16, 16), 16, 0, 360);
                painter.Fill();
                Notes.Note($"vector:{painter.SaveToVectorImage(image)}:{image.width > 30}:{image.height > 30}");
            }

            // Data binding: the root's data source, a label to its title, an integer field both ways to its count.
            m_Model = new BoundModel();
            m_Root.dataSource = m_Model;
            m_Bound = new Label { name = "bound" };
            m_Bound.SetBinding("text", new DataBinding { dataSourcePath = new PropertyPath("title"), bindingMode = BindingMode.ToTarget });
            page.Add(m_Bound);
            m_Count = new IntegerField("Count");
            m_Count.SetBinding("value", new DataBinding { dataSourcePath = new PropertyPath("count") });
            page.Add(m_Count);
            var path = new PropertyPath("a.b[2].c");
            Notes.Note($"path:{path.Length}:{path[2].Index}:{path}");
            ConverterGroups.RegisterGlobalConverter<float, string>((ref float v) => $"{v * 100:0}%");
        }

        void Update()
        {
            m_Frame++;
            if (m_Frame == 3)
            {
                int rows = 0;
                Element content = m_List.scrollView.contentContainer;
                for (int i = 0; i < content.childCount; i++)
                    if (content[i].resolvedStyle.display == DisplayStyle.Flex)
                        rows++;
                Notes.Note($"rows:{rows < 12}:{m_List.GetRootElementForIndex(0) != null}:{m_List.GetRootElementForIndex(50) == null}");
                m_List.ScrollToItem(50);
                Notes.Note($"scrolled:{m_List.GetRootElementForIndex(50) != null}");
                Element first = m_Columns.GetRootElementForIndex(0);
                Notes.Note($"sorted:{(first?[0]?[0] as Label)?.text}");
                Notes.Note($"bound:{m_Bound.text}:{m_Count.value}");
                Notes.Note($"marked:{m_Root.Find<Label>("marked")?.text}");
                m_Model.title = "second";
                m_Count.value = 9;
            }
            if (m_Frame == 5)
            {
                Notes.Note($"bound:{m_Bound.text}:{m_Model.count}");
                Notes.Note($"animated:{m_Completed}:{m_Animated}");
                Notes.Note($"painted:{m_Paints > 0}");
            }
        }
    }
}
