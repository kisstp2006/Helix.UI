using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

namespace HelixEngine.UI
{
    public enum SortDirection
    {
        Ascending,
        Descending,
    }

    public enum ColumnSortingMode
    {
        None,
        Default,
        Custom,
    }

    public enum ColumnDataType
    {
        Name,
        Title,
        Icon,
        Visibility,
        Width,
        MinWidth,
        MaxWidth,
        Sortable,
        Stretchable,
        Optional,
        Resizable,
        HeaderTemplate,
        CellTemplate,
    }

    // A column of a multi-column view: its header, its cells, its width and how it sorts.
    public class Column
    {
        string m_Name, m_Title;
        bool m_Visible = true, m_Stretchable, m_Sortable = true, m_Resizable = true, m_Optional = true;
        Length m_Width = new Length(120), m_MinWidth = new Length(35), m_MaxWidth = new Length(8000);
        Texture2D m_Icon;

        public event Action<Column, ColumnDataType> propertyChanged;

        public string name { get => m_Name; set => Set(ref m_Name, value, ColumnDataType.Name); }
        public string title { get => m_Title; set => Set(ref m_Title, value, ColumnDataType.Title); }
        public Texture2D icon { get => m_Icon; set => Set(ref m_Icon, value, ColumnDataType.Icon); }
        public bool visible { get => m_Visible; set => Set(ref m_Visible, value, ColumnDataType.Visibility); }
        public Length width { get => m_Width; set => Set(ref m_Width, value, ColumnDataType.Width); }
        public Length minWidth { get => m_MinWidth; set => Set(ref m_MinWidth, value, ColumnDataType.MinWidth); }
        public Length maxWidth { get => m_MaxWidth; set => Set(ref m_MaxWidth, value, ColumnDataType.MaxWidth); }
        public bool sortable { get => m_Sortable; set => Set(ref m_Sortable, value, ColumnDataType.Sortable); }
        public bool stretchable { get => m_Stretchable; set => Set(ref m_Stretchable, value, ColumnDataType.Stretchable); }
        public bool optional { get => m_Optional; set => Set(ref m_Optional, value, ColumnDataType.Optional); }
        public bool resizable { get => m_Resizable; set => Set(ref m_Resizable, value, ColumnDataType.Resizable); }
        public string bindingPath { get; set; }
        public UITemplate headerTemplate { get; set; }
        public UITemplate cellTemplate { get; set; }
        public Func<Element> makeHeader { get; set; }
        public Action<Element> bindHeader { get; set; }
        public Action<Element> unbindHeader { get; set; }
        public Action<Element> destroyHeader { get; set; }
        public Func<Element> makeCell { get; set; }
        public Action<Element, int> bindCell { get; set; }
        public Action<Element, int> unbindCell { get; set; }
        public Action<Element> destroyCell { get; set; }
        // Compares two items' indices for sorting (ColumnSortingMode.Default).
        public Comparison<int> comparison { get; set; }
        public Columns collection { get; internal set; }

        void Set<T>(ref T field, T value, ColumnDataType what)
        {
            if (EqualityComparer<T>.Default.Equals(field, value))
                return;
            field = value;
            propertyChanged?.Invoke(this, what);
        }

        internal float pixelWidth => m_Width.value > 0 ? m_Width.value : 120;
    }

    // The columns of a multi-column view.
    public class Columns : IList<Column>
    {
        public enum StretchMode
        {
            Grow,
            GrowAndFill,
        }

        readonly List<Column> m_Columns = new List<Column>();
        public event Action<ColumnsDataType> propertyChanged;
        internal event Action changed;

        public string primaryColumnName { get; set; }
        public bool reorderable { get; set; } = true;
        public bool resizable { get; set; } = true;
        public bool resizePreview { get; set; }
        public StretchMode stretchMode { get; set; } = StretchMode.GrowAndFill;

        public Column this[int index]
        {
            get => m_Columns[index];
            set
            {
                m_Columns[index] = value;
                Hook(value);
            }
        }

        public Column this[string name] => m_Columns.FirstOrDefault(c => c.name == name);
        public int Count => m_Columns.Count;
        public bool IsReadOnly => false;

        void Hook(Column c)
        {
            c.collection = this;
            c.propertyChanged += (_, __) => changed?.Invoke();
            changed?.Invoke();
        }

        public void Add(Column item)
        {
            m_Columns.Add(item);
            Hook(item);
        }

        public void Insert(int index, Column item)
        {
            m_Columns.Insert(index, item);
            Hook(item);
        }

        public bool Remove(Column item)
        {
            bool removed = m_Columns.Remove(item);
            if (removed)
                changed?.Invoke();
            return removed;
        }

        public void RemoveAt(int index)
        {
            m_Columns.RemoveAt(index);
            changed?.Invoke();
        }

        public void Clear()
        {
            m_Columns.Clear();
            changed?.Invoke();
        }

        public bool Contains(Column item) => m_Columns.Contains(item);
        public void CopyTo(Column[] array, int arrayIndex) => m_Columns.CopyTo(array, arrayIndex);
        public int IndexOf(Column column) => m_Columns.IndexOf(column);
        public bool IsPrimary(Column column) => primaryColumnName != null ? column.name == primaryColumnName : m_Columns.FirstOrDefault(c => c.visible) == column;
        public IEnumerator<Column> GetEnumerator() => m_Columns.GetEnumerator();
        IEnumerator IEnumerable.GetEnumerator() => m_Columns.GetEnumerator();

        // A column shown at another place (its index stays).
        public void ReorderDisplay(int from, int to)
        {
            if (from < 0 || from >= m_Columns.Count || to < 0 || to >= m_Columns.Count)
                return;
            Column c = m_Columns[from];
            m_Columns.RemoveAt(from);
            m_Columns.Insert(to, c);
            propertyChanged?.Invoke(ColumnsDataType.Reorderable);
            changed?.Invoke();
        }

        internal IEnumerable<Column> visibleList => m_Columns.Where(c => c.visible);
    }

    public enum ColumnsDataType
    {
        PrimaryColumn,
        StretchMode,
        Reorderable,
        Resizable,
        ResizePreview,
    }

    public class SortColumnDescription
    {
        public string columnName { get; set; }
        public int columnIndex { get; set; } = -1;
        public SortDirection direction { get; set; }
        public Column column { get; internal set; }

        public SortColumnDescription() { }

        public SortColumnDescription(string columnName, SortDirection direction)
        {
            this.columnName = columnName;
            this.direction = direction;
        }

        public SortColumnDescription(int columnIndex, SortDirection direction)
        {
            this.columnIndex = columnIndex;
            this.direction = direction;
        }
    }

    public class SortColumnDescriptions : IList<SortColumnDescription>
    {
        readonly List<SortColumnDescription> m_List = new List<SortColumnDescription>();
        internal event Action changed;

        public SortColumnDescription this[int index]
        {
            get => m_List[index];
            set
            {
                m_List[index] = value;
                changed?.Invoke();
            }
        }

        public int Count => m_List.Count;
        public bool IsReadOnly => false;
        public void Add(SortColumnDescription item)
        {
            m_List.Add(item);
            changed?.Invoke();
        }
        public void Insert(int index, SortColumnDescription item)
        {
            m_List.Insert(index, item);
            changed?.Invoke();
        }
        public bool Remove(SortColumnDescription item)
        {
            bool r = m_List.Remove(item);
            changed?.Invoke();
            return r;
        }
        public void RemoveAt(int index)
        {
            m_List.RemoveAt(index);
            changed?.Invoke();
        }
        public void Clear()
        {
            m_List.Clear();
            changed?.Invoke();
        }
        public bool Contains(SortColumnDescription item) => m_List.Contains(item);
        public void CopyTo(SortColumnDescription[] array, int arrayIndex) => m_List.CopyTo(array, arrayIndex);
        public int IndexOf(SortColumnDescription item) => m_List.IndexOf(item);
        public IEnumerator<SortColumnDescription> GetEnumerator() => m_List.GetEnumerator();
        IEnumerator IEnumerable.GetEnumerator() => m_List.GetEnumerator();
    }

    // The header row and the cells of a multi-column view: one header per visible column (its title and sort
    // indicator, a handle on its right edge resizing it), one cell per column in each row.
    public class MultiColumnController : IDisposable
    {
        public static readonly string headerContainerHssClassName = "helix-multi-column-header";
        public static readonly string headerColumnHssClassName = headerContainerHssClassName + "__column";
        public static readonly string headerTitleHssClassName = headerColumnHssClassName + "-title";
        public static readonly string headerSortIndicatorHssClassName = headerColumnHssClassName + "-sort-indicator";
        public static readonly string headerResizeHandleHssClassName = headerColumnHssClassName + "-resize-handle";
        public static readonly string cellHssClassName = "helix-multi-column-view__cell";
        public static readonly string rowContainerHssClassName = "helix-multi-column-view__row-container";

        readonly Columns m_Columns;
        readonly SortColumnDescriptions m_Sort;
        readonly BaseVerticalCollectionView m_View;
        readonly Element m_Header = new Element();
        List<int> m_Order;
        float m_ResizeStart, m_ResizeWidth;
        Column m_Resizing;

        public event Action columnSortingChanged;
        public event Action<ContextualMenuPopulateEvent, Column> headerContextMenuPopulateEvent;
        public ColumnSortingMode sortingMode { get; set; } = ColumnSortingMode.None;
        public Element header => m_Header;

        public MultiColumnController(Columns columns, SortColumnDescriptions sortDescriptions, List<SortColumnDescription> sortedColumns)
        {
            m_Columns = columns;
            m_Sort = sortDescriptions;
            m_Header.AddToClassList(headerContainerHssClassName);
            m_Header.style.flexDirection = FlexDirection.Row;
            m_Columns.changed += () =>
            {
                BuildHeader();
                m_View?.Rebuild();
            };
            m_Sort.changed += () =>
            {
                Sort();
                columnSortingChanged?.Invoke();
                BuildHeader();
                m_View?.RefreshItems();
            };
        }

        internal MultiColumnController(Columns columns, SortColumnDescriptions sortDescriptions, BaseVerticalCollectionView view) : this(columns, sortDescriptions, (List<SortColumnDescription>)null)
        {
            m_View = view;
        }

        public void Dispose() { }

        internal IEnumerable<SortColumnDescription> sortedColumns => m_Sort;

        internal void BuildHeader()
        {
            m_Header.Clear();
            foreach (Column c in m_Columns.visibleList)
            {
                var cell = new Element();
                cell.AddToClassList(headerColumnHssClassName);
                cell.style.flexDirection = FlexDirection.Row;
                Size(cell, c);
                Element content = c.makeHeader != null ? c.makeHeader() : c.headerTemplate != null ? c.headerTemplate.Instantiate() : new Label(c.title ?? c.name ?? "");
                content.AddToClassList(headerTitleHssClassName);
                content.style.flexGrow = 1;
                cell.Add(content);
                c.bindHeader?.Invoke(content);
                SortColumnDescription sorted = m_Sort.FirstOrDefault(d => Matches(d, c));
                if (sorted != null)
                {
                    var indicator = new Label(sorted.direction == SortDirection.Ascending ? "^" : "v");
                    indicator.AddToClassList(headerSortIndicatorHssClassName);
                    cell.Add(indicator);
                }
                if (m_Columns.resizable && c.resizable)
                {
                    var handle = new Element();
                    handle.AddToClassList(headerResizeHandleHssClassName);
                    Column column = c;
                    handle.RegisterCallback<PointerDownEvent>(e =>
                    {
                        m_Resizing = column;
                        m_ResizeStart = e.position.x;
                        m_ResizeWidth = column.pixelWidth;
                        handle.CapturePointer(e.pointerId);
                        e.StopPropagation();
                    });
                    handle.RegisterCallback<PointerMoveEvent>(e =>
                    {
                        if (m_Resizing != column)
                            return;
                        float w = Math.Clamp(m_ResizeWidth + e.position.x - m_ResizeStart, column.minWidth.value, column.maxWidth.value);
                        column.width = new Length(w);
                    });
                    handle.RegisterCallback<PointerUpEvent>(e =>
                    {
                        m_Resizing = null;
                        handle.ReleasePointer(e.pointerId);
                    });
                    cell.Add(handle);
                }
                Column clicked = c;
                cell.RegisterCallback<ClickEvent>(e => OnHeaderClicked(clicked, e.shiftKey));
                // The header's contextual menu: the optional columns shown or hidden, then the script's items.
                cell.AddManipulator(new ContextualMenuManipulator(e =>
                {
                    foreach (Column each in m_Columns)
                    {
                        Column column = each;
                        e.menu.AppendAction(column.title ?? column.name ?? "", _ => column.visible = !column.visible,
                                            _ => !column.optional ? DropdownMenuAction.Status.Disabled | (column.visible ? DropdownMenuAction.Status.Checked : 0)
                                                 : column.visible ? DropdownMenuAction.Status.Checked : DropdownMenuAction.Status.Normal);
                    }
                    headerContextMenuPopulateEvent?.Invoke(e, clicked);
                }));
                m_Header.Add(cell);
            }
        }

        static void Size(Element e, Column c)
        {
            e.style.width = c.pixelWidth;
            e.style.minWidth = c.minWidth.value;
            e.style.maxWidth = c.maxWidth.value;
            e.style.flexGrow = c.stretchable ? 1 : 0;
            e.style.flexShrink = c.stretchable ? 1 : 0;
        }

        bool Matches(SortColumnDescription d, Column c) => d.columnName != null ? d.columnName == c.name : d.columnIndex == m_Columns.IndexOf(c);

        void OnHeaderClicked(Column c, bool add)
        {
            if (sortingMode == ColumnSortingMode.None || !c.sortable)
                return;
            SortColumnDescription existing = m_Sort.FirstOrDefault(d => Matches(d, c));
            var next = new SortColumnDescription(c.name ?? "", existing != null && existing.direction == SortDirection.Ascending ? SortDirection.Descending : SortDirection.Ascending)
            {
                columnIndex = m_Columns.IndexOf(c),
                column = c,
            };
            if (c.name == null)
                next.columnName = null;
            if (!add)
            {
                m_Sort.Clear();
                m_Sort.Add(next);
            }
            else if (existing != null)
                m_Sort[m_Sort.IndexOf(existing)] = next;
            else
                m_Sort.Add(next);
        }

        // The items' order under the sort descriptions (Default: by the columns' comparisons).
        internal void Sort()
        {
            m_Order = null;
            if (sortingMode != ColumnSortingMode.Default || m_Sort.Count == 0 || m_View == null)
                return;
            int count = m_View.itemsSource?.Count ?? 0;
            var order = Enumerable.Range(0, count).ToList();
            var keys = m_Sort.Select(d => (column: d.column ?? (d.columnName != null ? m_Columns[d.columnName] : d.columnIndex >= 0 && d.columnIndex < m_Columns.Count ? m_Columns[d.columnIndex] : null), d.direction))
                             .Where(k => k.column?.comparison != null).ToList();
            if (keys.Count == 0)
                return;
            order.Sort((a, b) =>
            {
                foreach (var (column, direction) in keys)
                {
                    int r = column.comparison(a, b);
                    if (r != 0)
                        return direction == SortDirection.Ascending ? r : -r;
                }
                return a.CompareTo(b);
            });
            m_Order = order;
        }

        // The item shown at a row (through the sort).
        internal int SourceIndex(int index) => m_Order != null && index >= 0 && index < m_Order.Count ? m_Order[index] : index;

        internal Element MakeRow()
        {
            var row = new Element();
            row.AddToClassList(rowContainerHssClassName);
            row.style.flexDirection = FlexDirection.Row;
            foreach (Column c in m_Columns.visibleList)
            {
                var cell = new Element();
                cell.AddToClassList(cellHssClassName);
                Size(cell, c);
                Element content = c.makeCell != null ? c.makeCell() : c.cellTemplate != null ? c.cellTemplate.Instantiate() : new Label();
                cell.Add(content);
                row.Add(cell);
            }
            return row;
        }

        internal void BindRow(Element row, int index, Func<int, object> item)
        {
            int source = SourceIndex(index);
            int i = 0;
            foreach (Column c in m_Columns.visibleList)
            {
                if (i >= row.childCount)
                    break;
                Element content = row[i][0];
                if (c.bindCell != null)
                    c.bindCell(content, source);
                else if (content is Label label)
                    label.text = item(source)?.ToString() ?? "";
                i++;
            }
        }

        internal void UnbindRow(Element row, int index)
        {
            int i = 0;
            foreach (Column c in m_Columns.visibleList)
            {
                if (i >= row.childCount)
                    break;
                c.unbindCell?.Invoke(row[i][0], SourceIndex(index));
                i++;
            }
        }
    }

    public class MultiColumnListViewController : BaseListViewController
    {
        readonly MultiColumnController m_Columns;

        public MultiColumnListViewController(Columns columns, SortColumnDescriptions sortDescriptions, List<SortColumnDescription> sortedColumns)
        {
            m_Columns = new MultiColumnController(columns, sortDescriptions, sortedColumns);
        }

        internal MultiColumnListViewController(MultiColumnController columns) { m_Columns = columns; }

        public MultiColumnController columnController => m_Columns;
        public override object GetItemForIndex(int index) => base.GetItemForIndex(m_Columns.SourceIndex(index));
        protected internal override Element MakeItem() => m_Columns.MakeRow();
        protected internal override void BindItem(Element element, int index) => m_Columns.BindRow(element, index, i => base.GetItemForIndex(i));
        protected internal override void UnbindItem(Element element, int index) => m_Columns.UnbindRow(element, index);
    }

    // A list whose rows have a cell per column, under a header that resizes and sorts the columns.
    [HxmlElement]
    public class MultiColumnListView : BaseListView
    {
        readonly Columns m_Columns = new Columns();
        readonly SortColumnDescriptions m_Sort = new SortColumnDescriptions();
        readonly MultiColumnController m_Controller;

        public event Action columnSortingChanged
        {
            add => m_Controller.columnSortingChanged += value;
            remove => m_Controller.columnSortingChanged -= value;
        }

        public event Action<ContextualMenuPopulateEvent, Column> headerContextMenuPopulateEvent
        {
            add => m_Controller.headerContextMenuPopulateEvent += value;
            remove => m_Controller.headerContextMenuPopulateEvent -= value;
        }

        public MultiColumnListView() : this(new Columns()) { }

        public MultiColumnListView(Columns columns) : base()
        {
            AddToClassList("helix-multi-column-list-view");
            m_Columns = columns ?? new Columns();
            m_Controller = new MultiColumnController(m_Columns, m_Sort, this);
            hierarchy.Insert(0, m_Controller.header);
            SetViewController(new MultiColumnListViewController(m_Controller));
            m_Controller.BuildHeader();
        }

        protected override CollectionViewController CreateViewController() => new MultiColumnListViewController(m_Controller);
        public new MultiColumnListViewController viewController => base.viewController as MultiColumnListViewController;
        public Columns columns => m_Columns;
        public SortColumnDescriptions sortColumnDescriptions => m_Sort;
        public IEnumerable<SortColumnDescription> sortedColumns => m_Controller.sortedColumns;

        [HxmlAttribute]
        public ColumnSortingMode sortingMode
        {
            get => m_Controller.sortingMode;
            set
            {
                m_Controller.sortingMode = value;
                m_Controller.Sort();
                RefreshItems();
            }
        }

        public void SetViewController(MultiColumnListViewController controller) => base.SetViewController(controller);
    }

    public class MultiColumnTreeViewController : BaseTreeViewController
    {
        readonly MultiColumnController m_Columns;

        public MultiColumnTreeViewController(Columns columns, SortColumnDescriptions sortDescriptions, List<SortColumnDescription> sortedColumns)
        {
            m_Columns = new MultiColumnController(columns, sortDescriptions, sortedColumns);
        }

        internal MultiColumnTreeViewController(MultiColumnController columns) { m_Columns = columns; }

        public MultiColumnController columnController => m_Columns;
        protected internal override Element MakeContent() => m_Columns.MakeRow();
        protected internal override void BindContent(Element element, int index) => m_Columns.BindRow(element, index, i => GetItemForIndex(i));
    }

    public class DefaultMultiColumnTreeViewController<T> : MultiColumnTreeViewController
    {
        public DefaultMultiColumnTreeViewController(Columns columns, SortColumnDescriptions sortDescriptions, List<SortColumnDescription> sortedColumns)
            : base(columns, sortDescriptions, sortedColumns) { }

        internal DefaultMultiColumnTreeViewController(MultiColumnController columns) : base(columns) { }

        public T GetDataForId(int id) => nodes.TryGetValue(id, out Node n) ? (T)n.data : default;
        public T GetDataForIndex(int index) => (T)GetItemForIndex(index);
    }

    // A tree whose rows have a cell per column.
    [HxmlElement]
    public class MultiColumnTreeView : BaseTreeView
    {
        readonly Columns m_Columns;
        readonly SortColumnDescriptions m_Sort = new SortColumnDescriptions();
        readonly MultiColumnController m_Controller;

        public event Action columnSortingChanged
        {
            add => m_Controller.columnSortingChanged += value;
            remove => m_Controller.columnSortingChanged -= value;
        }

        public MultiColumnTreeView() : this(new Columns()) { }

        public MultiColumnTreeView(Columns columns) : base()
        {
            AddToClassList("helix-multi-column-tree-view");
            m_Columns = columns ?? new Columns();
            m_Controller = new MultiColumnController(m_Columns, m_Sort, this);
            hierarchy.Insert(0, m_Controller.header);
            SetViewController(new MultiColumnTreeViewController(m_Controller));
            m_Controller.BuildHeader();
        }

        protected override CollectionViewController CreateViewController() => new MultiColumnTreeViewController(m_Controller);
        public new MultiColumnTreeViewController viewController => base.viewController as MultiColumnTreeViewController;
        public Columns columns => m_Columns;
        public SortColumnDescriptions sortColumnDescriptions => m_Sort;
        public IEnumerable<SortColumnDescription> sortedColumns => m_Controller.sortedColumns;

        [HxmlAttribute]
        public ColumnSortingMode sortingMode
        {
            get => m_Controller.sortingMode;
            set => m_Controller.sortingMode = value;
        }

        public void SetViewController(MultiColumnTreeViewController controller) => base.SetViewController(controller);
    }
}
