using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Helix.Interop;

namespace HelixEngine.UI
{
    public enum SelectionType
    {
        None,
        Single,
        Multiple,
    }

    public enum CollectionVirtualizationMethod
    {
        FixedHeight,
        DynamicHeight,
    }

    public enum AlternatingRowBackground
    {
        None,
        ContentOnly,
        All,
    }

    public enum ListViewReorderMode
    {
        Simple,
        Animated,
    }

    public enum BindingSourceSelectionMode
    {
        Manual,
        AutoAssign,
    }

    // The items of a collection view: their count, ids, the elements made and bound for them, moves.
    public abstract class CollectionViewController
    {
        IList m_ItemsSource;
        public event Action itemsSourceChanged;
        public event Action<int, int> itemIndexChanged;

        protected BaseVerticalCollectionView view { get; private set; }

        public virtual IList itemsSource
        {
            get => m_ItemsSource;
            set
            {
                if (m_ItemsSource == value)
                    return;
                m_ItemsSource = value;
                RaiseItemsSourceChanged();
            }
        }

        public void SetView(BaseVerticalCollectionView collectionView)
        {
            view = collectionView;
            PrepareView();
        }

        protected virtual void PrepareView() { }
        public virtual void Dispose() { view = null; }

        public virtual int GetItemsCount() => m_ItemsSource?.Count ?? 0;
        public virtual int GetIndexForId(int id) => id;
        public virtual int GetIdForIndex(int index) => index;
        public virtual object GetItemForIndex(int index) => m_ItemsSource == null || index < 0 || index >= m_ItemsSource.Count ? null : m_ItemsSource[index];
        public virtual object GetItemForId(int id) => GetItemForIndex(GetIndexForId(id));

        protected internal abstract Element MakeItem();
        protected internal abstract void BindItem(Element element, int index);
        protected internal virtual void UnbindItem(Element element, int index) { }
        protected internal virtual void DestroyItem(Element element) { }

        // An item moved by dragging: the source's order follows.
        public virtual void Move(int index, int newIndex)
        {
            if (m_ItemsSource == null || index == newIndex || index < 0 || index >= m_ItemsSource.Count)
                return;
            object item = m_ItemsSource[index];
            m_ItemsSource.RemoveAt(index);
            if (newIndex > index)
                newIndex--;
            newIndex = Math.Clamp(newIndex, 0, m_ItemsSource.Count);
            m_ItemsSource.Insert(newIndex, item);
            RaiseItemIndexChanged(index, newIndex);
        }

        protected void RaiseItemsSourceChanged() => itemsSourceChanged?.Invoke();
        protected void RaiseItemIndexChanged(int srcIndex, int dstIndex) => itemIndexChanged?.Invoke(srcIndex, dstIndex);
    }

    // A vertical list of items of which only those in the viewport have elements: rows made once and bound again as
    // the list scrolls (all of the same height, or each of its own measured height), selected by the pointer and the
    // keyboard, moved by dragging when reorderable.
    public abstract class BaseVerticalCollectionView : BindableElement
    {
        public static readonly string hssClassName = "helix-collection-view";
        public static readonly string borderHssClassName = hssClassName + "--with-border";
        public static readonly string itemHssClassName = hssClassName + "__item";
        public static readonly string dragHoverBarHssClassName = hssClassName + "__drag-hover-bar";
        public static readonly string dragHoverMarkerHssClassName = hssClassName + "__drag-hover-marker";
        public static readonly string itemDragHoverHssClassName = itemHssClassName + "--drag-hover";
        public static readonly string itemSelectedVariantHssClassName = itemHssClassName + "--selected";
        public static readonly string itemAlternativeBackgroundHssClassName = itemHssClassName + "--alternative-background";
        public static readonly string listScrollViewHssClassName = hssClassName + "__scroll-view";
        internal const float k_DefaultItemHeight = 22;

        sealed class Row
        {
            public Element container;
            public Element item;
            public int index = -1;
            public int id = -1;
        }

        readonly ScrollView m_ScrollView;
        readonly Element m_DragBar;
        readonly List<Row> m_Active = new List<Row>(), m_Free = new List<Row>();
        readonly List<int> m_Selected = new List<int>();
        readonly Dictionary<int, float> m_Heights = new Dictionary<int, float>();
        CollectionViewController m_Controller;
        float m_FixedItemHeight = k_DefaultItemHeight;
        int m_Anchor = -1, m_Pointer = -1, m_DragFrom = -1, m_DropAt = -1;
        DragAndDropPosition m_DropPosition = DragAndDropPosition.BetweenItems;
        Element m_DropOver;
        Vector2 m_DownAt;
        bool m_Dragging, m_RefreshQueued;

        public event Action<IEnumerable<object>> itemsChosen;
        public event Action<IEnumerable<object>> selectionChanged;
        public event Action<IEnumerable<int>> selectedIndicesChanged;
        public event Action<int, int> itemIndexChanged;
        public event Action itemsSourceChanged;
        public event Func<CanStartDragArgs, bool> canStartDrag;
        public event Func<SetupDragAndDropArgs, StartDragArgs> setupDragAndDrop;
        public event Func<HandleDragAndDropArgs, DragVisualMode> dragAndDropUpdate;
        public event Func<HandleDragAndDropArgs, DragVisualMode> handleDrop;

        protected internal BaseVerticalCollectionView(ObjectId id) : base(id) { }

        public BaseVerticalCollectionView() : base()
        {
            AddToClassList(hssClassName);
            focusable = true;
            m_ScrollView = new ScrollView(ScrollViewMode.Vertical);
            m_ScrollView.AddToClassList(listScrollViewHssClassName);
            hierarchy.Add(m_ScrollView);
            m_DragBar = new Element { pickingMode = PickingMode.Ignore };
            m_DragBar.AddToClassList(dragHoverBarHssClassName);
            m_DragBar.style.display = DisplayStyle.None;
            m_ScrollView.contentViewport.Add(m_DragBar);
            m_ScrollView.verticalScroller.valueChanged += _ => QueueRefresh();
            m_ScrollView.contentViewport.RegisterCallback<GeometryChangedEvent>(_ => QueueRefresh());
            RegisterCallback<NavigationMoveEvent>(OnNavigationMove);
            RegisterCallback<NavigationSubmitEvent>(_ => ChooseSelection());
            RegisterCallback<KeyDownEvent>(OnKeyDown);
        }

        public BaseVerticalCollectionView(IList itemsSource, float itemHeight = -1) : this()
        {
            if (itemHeight > 0)
                fixedItemHeight = itemHeight;
            this.itemsSource = itemsSource;
        }

        public override Element contentContainer => m_ScrollView?.contentContainer;
        public ScrollView scrollView => m_ScrollView;
        public CollectionViewController viewController => m_Controller;

        protected void SetViewController(CollectionViewController controller)
        {
            if (m_Controller != null)
            {
                m_Controller.itemsSourceChanged -= OnItemsSourceChanged;
                m_Controller.itemIndexChanged -= OnItemIndexChanged;
            }
            m_Controller = controller;
            if (m_Controller == null)
                return;
            m_Controller.SetView(this);
            m_Controller.itemsSourceChanged += OnItemsSourceChanged;
            m_Controller.itemIndexChanged += OnItemIndexChanged;
            Rebuild();
        }

        protected abstract CollectionViewController CreateViewController();

        internal CollectionViewController EnsureController()
        {
            if (m_Controller == null)
                SetViewController(CreateViewController());
            return m_Controller;
        }

        void OnItemsSourceChanged()
        {
            itemsSourceChanged?.Invoke();
            Rebuild();
        }

        void OnItemIndexChanged(int src, int dst) => itemIndexChanged?.Invoke(src, dst);

        public IList itemsSource
        {
            get => m_Controller?.itemsSource;
            set => EnsureController().itemsSource = value;
        }

        [HxmlAttribute]
        public float fixedItemHeight
        {
            get => m_FixedItemHeight;
            set
            {
                m_FixedItemHeight = value > 0 ? value : k_DefaultItemHeight;
                RefreshItems();
            }
        }

        [HxmlAttribute]
        public CollectionVirtualizationMethod virtualizationMethod { get; set; } = CollectionVirtualizationMethod.FixedHeight;

        [HxmlAttribute]
        public SelectionType selectionType { get; set; } = SelectionType.Single;

        [HxmlAttribute]
        public bool reorderable { get; set; }

        [HxmlAttribute]
        public bool horizontalScrollingEnabled
        {
            get => m_ScrollView.mode != ScrollViewMode.Vertical;
            set => m_ScrollView.mode = value ? ScrollViewMode.VerticalAndHorizontal : ScrollViewMode.Vertical;
        }

        bool m_ShowBorder;

        [HxmlAttribute]
        public bool showBorder
        {
            get => m_ShowBorder;
            set
            {
                m_ShowBorder = value;
                EnableInClassList(borderHssClassName, value);
            }
        }

        AlternatingRowBackground m_Alternating;

        [HxmlAttribute]
        public AlternatingRowBackground showAlternatingRowBackgrounds
        {
            get => m_Alternating;
            set
            {
                m_Alternating = value;
                RefreshItems();
            }
        }

        // ---------------------------------------------------------------------------------------------- selection

        public int selectedIndex
        {
            get => m_Selected.Count == 0 ? -1 : m_Selected[0];
            set => SetSelection(value);
        }

        public IEnumerable<int> selectedIndices => m_Selected;
        public object selectedItem => m_Selected.Count == 0 ? null : m_Controller?.GetItemForIndex(m_Selected[0]);
        public IEnumerable<object> selectedItems => m_Selected.Select(i => m_Controller?.GetItemForIndex(i));
        public IEnumerable<int> selectedIds => m_Selected.Select(i => m_Controller?.GetIdForIndex(i) ?? i);

        public void SetSelection(int index)
        {
            if (index < 0)
                ClearSelection();
            else
                SetSelection(new[] { index });
        }

        public void SetSelection(IEnumerable<int> indices) => SetSelectionInternal(indices, true);
        public void SetSelectionWithoutNotify(IEnumerable<int> indices) => SetSelectionInternal(indices, false);

        public void SetSelectionById(int id) => SetSelection(m_Controller?.GetIndexForId(id) ?? id);
        public void SetSelectionById(IEnumerable<int> ids) => SetSelection(ids.Select(i => m_Controller?.GetIndexForId(i) ?? i));
        public void SetSelectionByIdWithoutNotify(IEnumerable<int> ids) => SetSelectionWithoutNotify(ids.Select(i => m_Controller?.GetIndexForId(i) ?? i));

        public void AddToSelection(int index)
        {
            if (selectionType == SelectionType.None || m_Selected.Contains(index))
                return;
            if (selectionType == SelectionType.Single)
                m_Selected.Clear();
            m_Selected.Add(index);
            SelectionChanged(true);
        }

        public void RemoveFromSelection(int index)
        {
            if (m_Selected.Remove(index))
                SelectionChanged(true);
        }

        public void ClearSelection()
        {
            if (m_Selected.Count == 0)
                return;
            m_Selected.Clear();
            SelectionChanged(true);
        }

        public void SelectAll()
        {
            if (selectionType != SelectionType.Multiple)
                return;
            m_Selected.Clear();
            for (int i = 0; i < Count; i++)
                m_Selected.Add(i);
            SelectionChanged(true);
        }

        void SetSelectionInternal(IEnumerable<int> indices, bool notify)
        {
            if (selectionType == SelectionType.None)
                return;
            var next = indices.Where(i => i >= 0 && i < Count).Distinct().ToList();
            if (selectionType == SelectionType.Single && next.Count > 1)
                next.RemoveRange(1, next.Count - 1);
            if (next.SequenceEqual(m_Selected))
                return;
            m_Selected.Clear();
            m_Selected.AddRange(next);
            SelectionChanged(notify);
        }

        void SelectionChanged(bool notify)
        {
            foreach (Row r in m_Active)
                r.container.EnableInClassList(itemSelectedVariantHssClassName, m_Selected.Contains(r.index));
            if (!notify)
                return;
            selectedIndicesChanged?.Invoke(m_Selected.ToArray());
            selectionChanged?.Invoke(selectedItems.ToArray());
        }

        void ChooseSelection()
        {
            if (m_Selected.Count > 0)
                itemsChosen?.Invoke(selectedItems.ToArray());
        }

        // ------------------------------------------------------------------------------------------- the rows

        int Count => m_Controller?.GetItemsCount() ?? 0;

        public void Rebuild()
        {
            foreach (Row r in m_Active.Concat(m_Free).ToArray())
            {
                if (r.index >= 0)
                    m_Controller?.UnbindItem(r.item, r.index);
                m_Controller?.DestroyItem(r.item);
                r.container.RemoveFromHierarchy();
            }
            m_Active.Clear();
            m_Free.Clear();
            m_Heights.Clear();
            m_Selected.RemoveAll(i => i >= Count);
            Refresh();
        }

        public void RefreshItems()
        {
            foreach (Row r in m_Active)
                r.index = -2;
            Refresh();
        }

        public void RefreshItem(int index)
        {
            foreach (Row r in m_Active)
                if (r.index == index)
                {
                    m_Controller?.UnbindItem(r.item, index);
                    m_Controller?.BindItem(r.item, index);
                }
        }

        public Element GetRootElementForIndex(int index) => m_Active.FirstOrDefault(r => r.index == index)?.item;
        public Element GetRootElementForId(int id) => GetRootElementForIndex(m_Controller?.GetIndexForId(id) ?? id);

        public void ScrollToItem(int index)
        {
            int count = Count;
            if (count == 0)
                return;
            if (index < 0 || index >= count)
                index = count - 1;
            float top = Top(index), height = HeightOf(index), view = m_ScrollView.contentViewport.layout.height;
            Vector2 offset = m_ScrollView.scrollOffset;
            if (top < offset.y)
                offset.y = top;
            else if (top + height > offset.y + view)
                offset.y = top + height - view;
            m_ScrollView.scrollOffset = offset;
            Refresh();
        }

        public void ScrollToItemById(int id) => ScrollToItem(m_Controller?.GetIndexForId(id) ?? id);

        float HeightOf(int index) => virtualizationMethod == CollectionVirtualizationMethod.DynamicHeight && m_Heights.TryGetValue(index, out float h) ? h : m_FixedItemHeight;

        float Top(int index)
        {
            if (virtualizationMethod == CollectionVirtualizationMethod.FixedHeight)
                return index * m_FixedItemHeight;
            float y = 0;
            for (int i = 0; i < index; i++)
                y += HeightOf(i);
            return y;
        }

        void QueueRefresh()
        {
            if (m_RefreshQueued)
                return;
            m_RefreshQueued = true;
            schedule.Execute(() =>
            {
                m_RefreshQueued = false;
                Refresh();
            });
        }

        // The rows of the items in the viewport (and one past each end), placed where their items are.
        void Refresh()
        {
            CollectionViewController c = EnsureController();
            int count = Count;
            float total = 0;
            var tops = new float[count + 1];
            for (int i = 0; i < count; i++)
            {
                tops[i] = total;
                total += HeightOf(i);
            }
            tops[count] = total;
            Element content = m_ScrollView.contentContainer;
            content.style.height = total;
            float from = m_ScrollView.scrollOffset.y, view = m_ScrollView.contentViewport.layout.height;
            if (view <= 0)
                view = 400;
            int first = 0;
            while (first < count && tops[first + 1] <= from)
                first++;
            int last = first;
            while (last < count && tops[last] < from + view)
                last++;
            first = Math.Max(0, first - 1);
            last = Math.Min(count - 1, last);
            // Rows of items no longer shown go back to the pool.
            for (int i = m_Active.Count - 1; i >= 0; i--)
            {
                Row r = m_Active[i];
                if (r.index >= first && r.index <= last)
                    continue;
                if (r.index >= 0)
                    c.UnbindItem(r.item, r.index);
                r.index = -1;
                r.container.style.display = DisplayStyle.None;
                m_Active.RemoveAt(i);
                m_Free.Add(r);
            }
            for (int index = first; index <= last && count > 0; index++)
            {
                Row r = m_Active.FirstOrDefault(x => x.index == index);
                if (r == null)
                {
                    r = m_Active.FirstOrDefault(x => x.index == -2) ?? TakeRow(c, content);
                    if (!m_Active.Contains(r))
                        m_Active.Add(r);
                    if (r.index >= 0)
                        c.UnbindItem(r.item, r.index);
                    r.index = index;
                    r.id = c.GetIdForIndex(index);
                    c.BindItem(r.item, index);
                }
                Place(r, tops[index]);
            }
            // Rows still marked for a refresh have no item now.
            for (int i = m_Active.Count - 1; i >= 0; i--)
                if (m_Active[i].index == -2)
                {
                    m_Active[i].index = -1;
                    m_Active[i].container.style.display = DisplayStyle.None;
                    m_Free.Add(m_Active[i]);
                    m_Active.RemoveAt(i);
                }
        }

        Row TakeRow(CollectionViewController c, Element content)
        {
            Row r;
            if (m_Free.Count > 0)
            {
                r = m_Free[m_Free.Count - 1];
                m_Free.RemoveAt(m_Free.Count - 1);
            }
            else
            {
                r = new Row { container = new Element() };
                r.container.AddToClassList(itemHssClassName);
                r.container.style.position = Position.Absolute;
                r.container.style.left = 0;
                r.container.style.right = 0;
                r.item = c.MakeItem() ?? new Element();
                r.container.Add(r.item);
                Row captured = r;
                r.container.RegisterCallback<PointerDownEvent>(e => OnRowDown(captured, e));
                r.container.RegisterCallback<PointerMoveEvent>(e => OnRowMove(captured, e));
                r.container.RegisterCallback<PointerUpEvent>(e => OnRowUp(captured, e));
                r.container.RegisterCallback<GeometryChangedEvent>(e => OnRowGeometry(captured, e));
                content.Add(r.container);
            }
            r.container.style.display = DisplayStyle.Flex;
            return r;
        }

        void Place(Row r, float top)
        {
            r.container.style.top = top;
            if (virtualizationMethod == CollectionVirtualizationMethod.FixedHeight)
                r.container.style.height = m_FixedItemHeight;
            else
                r.container.style.height = StyleKeyword.Auto;
            r.container.EnableInClassList(itemSelectedVariantHssClassName, m_Selected.Contains(r.index));
            bool alternative = m_Alternating != AlternatingRowBackground.None && r.index % 2 == 1;
            r.container.EnableInClassList(itemAlternativeBackgroundHssClassName, alternative);
        }

        void OnRowGeometry(Row r, GeometryChangedEvent e)
        {
            if (virtualizationMethod != CollectionVirtualizationMethod.DynamicHeight || r.index < 0)
                return;
            float h = e.newRect.height;
            if (h <= 0 || (m_Heights.TryGetValue(r.index, out float known) && Math.Abs(known - h) < 0.5f))
                return;
            m_Heights[r.index] = h;
            QueueRefresh();
        }

        // ---------------------------------------------------------------------------------------- the pointer

        void OnRowDown(Row r, PointerDownEvent e)
        {
            if (e.button != 0 || r.index < 0)
                return;
            Focus();
            int index = r.index;
            if (e.clickCount == 2)
            {
                if (!m_Selected.Contains(index))
                    SetSelection(index);
                ChooseSelection();
                return;
            }
            if (selectionType == SelectionType.Multiple && e.actionKey)
            {
                if (m_Selected.Contains(index))
                    RemoveFromSelection(index);
                else
                    AddToSelection(index);
                m_Anchor = index;
            }
            else if (selectionType == SelectionType.Multiple && e.shiftKey && m_Anchor >= 0)
            {
                int a = Math.Min(m_Anchor, index), b = Math.Max(m_Anchor, index);
                SetSelection(Enumerable.Range(a, b - a + 1));
            }
            else
            {
                m_Anchor = index;
                SetSelection(index);
            }
            if (reorderable)
            {
                m_Pointer = e.pointerId;
                m_DownAt = new Vector2(e.position.x, e.position.y);
                m_DragFrom = index;
                r.container.CapturePointer(e.pointerId);
            }
        }

        void OnRowMove(Row r, PointerMoveEvent e)
        {
            if (e.pointerId != m_Pointer || m_DragFrom < 0)
                return;
            var at = new Vector2(e.position.x, e.position.y);
            if (!m_Dragging && (at - m_DownAt).magnitude < 5)
                return;
            if (!m_Dragging)
            {
                bool allowed = canStartDrag?.Invoke(new CanStartDragArgs(r.container, r.id, m_Selected.Select(i => m_Controller.GetIdForIndex(i)))) ?? true;
                if (!allowed)
                {
                    m_DragFrom = -1;
                    return;
                }
                m_Dragging = true;
                setupDragAndDrop?.Invoke(new SetupDragAndDropArgs(r.container, m_Selected.Select(i => m_Controller.GetIdForIndex(i)), new StartDragArgs("", DragVisualMode.Move)));
            }
            // The gap the pointer is over: before the row whose middle is below it; a view that drops onto items
            // takes a row's middle half as the row itself.
            Vector2 local = m_ScrollView.contentContainer.WorldToLocal(at);
            int count = Count, drop = count;
            DragAndDropPosition position = count == 0 ? DragAndDropPosition.OutsideItems : DragAndDropPosition.BetweenItems;
            for (int i = 0; i < count; i++)
            {
                float top = Top(i), h = HeightOf(i);
                if (dropsOnItems && local.y >= top + h * 0.25f && local.y < top + h * 0.75f)
                {
                    drop = i;
                    position = DragAndDropPosition.OverItem;
                    break;
                }
                if (local.y < top + h * 0.5f)
                {
                    drop = i;
                    break;
                }
            }
            m_DropAt = drop;
            m_DropPosition = position;
            DropTarget(drop, position, out int parentId, out int childIndex);
            var args = new HandleDragAndDropArgs(local, drop, position, parentId, childIndex);
            DragVisualMode mode = dragAndDropUpdate?.Invoke(args) ?? DragVisualMode.Move;
            bool shown = mode != DragVisualMode.Rejected && mode != DragVisualMode.None;
            MarkDropOver(shown && position == DragAndDropPosition.OverItem ? GetRootElementForIndex(drop) : null);
            m_DragBar.style.display = shown && position != DragAndDropPosition.OverItem ? DisplayStyle.Flex : DisplayStyle.None;
            m_DragBar.style.top = Top(drop) - m_ScrollView.scrollOffset.y;
        }

        void MarkDropOver(Element row)
        {
            Element container = row?.parent;
            if (m_DropOver == container)
                return;
            m_DropOver?.RemoveFromClassList(itemDragHoverHssClassName);
            m_DropOver = container;
            m_DropOver?.AddToClassList(itemDragHoverHssClassName);
        }

        // Whether a drag may end on a row (a tree's: into that item).
        internal virtual bool dropsOnItems => false;

        // Where a drop goes in a tree (a list: no parent, the index).
        internal virtual void DropTarget(int insertAtIndex, DragAndDropPosition position, out int parentId, out int childIndex)
        {
            parentId = -1;
            childIndex = insertAtIndex;
        }

        void OnRowUp(Row r, PointerUpEvent e)
        {
            if (e.pointerId != m_Pointer)
                return;
            r.container.ReleasePointer(e.pointerId);
            m_Pointer = -1;
            m_DragBar.style.display = DisplayStyle.None;
            MarkDropOver(null);
            if (m_Dragging && m_DropAt >= 0 && m_DragFrom >= 0 && handleDrop != null)
            {
                // The view's owner moves what it shows.
                DropTarget(m_DropAt, m_DropPosition, out int parentId, out int childIndex);
                handleDrop(new HandleDragAndDropArgs(Vector2.zero, m_DropAt, m_DropPosition, parentId, childIndex));
            }
            else if (m_Dragging && m_DropAt >= 0 && m_DragFrom >= 0 && m_DropPosition != DragAndDropPosition.OverItem)
            {
                DragVisualMode mode = DragVisualMode.Move;
                if (mode != DragVisualMode.Rejected && mode != DragVisualMode.None)
                {
                    int to = m_DropAt > m_DragFrom ? m_DropAt - 1 : m_DropAt;
                    m_Controller.Move(m_DragFrom, m_DropAt);
                    SetSelection(Math.Clamp(to, 0, Count - 1));
                    RefreshItems();
                }
            }
            m_Dragging = false;
            m_DragFrom = -1;
            m_DropAt = -1;
        }

        // ----------------------------------------------------------------------------------------- the keyboard

        void OnNavigationMove(NavigationMoveEvent e)
        {
            int count = Count;
            if (count == 0 || selectionType == SelectionType.None)
                return;
            int current = m_Selected.Count > 0 ? m_Selected[m_Selected.Count - 1] : -1;
            int next = e.direction == NavigationMoveDirection.Up ? current - 1 : e.direction == NavigationMoveDirection.Down ? current + 1 : current;
            if (next == current)
                return;
            next = Math.Clamp(next, 0, count - 1);
            if (e.shiftKey && selectionType == SelectionType.Multiple && m_Anchor >= 0)
            {
                int a = Math.Min(m_Anchor, next), b = Math.Max(m_Anchor, next);
                SetSelection(Enumerable.Range(a, b - a + 1));
            }
            else
            {
                m_Anchor = next;
                SetSelection(next);
            }
            ScrollToItem(next);
            e.StopPropagation();
        }

        void OnKeyDown(KeyDownEvent e)
        {
            int count = Count;
            if (count == 0)
                return;
            int page = Math.Max(1, (int)(m_ScrollView.contentViewport.layout.height / m_FixedItemHeight));
            int current = selectedIndex;
            int next;
            switch (e.keyCode)
            {
            case KeyCode.Home: next = 0; break;
            case KeyCode.End: next = count - 1; break;
            case KeyCode.PageUp: next = Math.Max(0, current - page); break;
            case KeyCode.PageDown: next = Math.Min(count - 1, current + page); break;
            case KeyCode.A when e.actionKey:
                SelectAll();
                e.StopPropagation();
                return;
            default: return;
            }
            m_Anchor = next;
            SetSelection(next);
            ScrollToItem(next);
            e.StopPropagation();
        }
    }

    // ---------------------------------------------------------------------------------------- drag and drop args

    public enum DragAndDropPosition
    {
        OverItem,
        BetweenItems,
        OutsideItems,
    }

    public readonly struct CanStartDragArgs
    {
        public readonly Element draggedElement;
        public readonly int id;
        public readonly IEnumerable<int> selectedIds;
        internal CanStartDragArgs(Element draggedElement, int id, IEnumerable<int> selectedIds)
        {
            this.draggedElement = draggedElement;
            this.id = id;
            this.selectedIds = selectedIds;
        }
    }

    public readonly struct StartDragArgs
    {
        public string title { get; }
        public DragVisualMode visualMode { get; }
        public StartDragArgs(string title, DragVisualMode visualMode)
        {
            this.title = title;
            this.visualMode = visualMode;
        }
    }

    public readonly struct SetupDragAndDropArgs
    {
        public readonly Element draggedElement;
        public readonly IEnumerable<int> selectedIds;
        public readonly StartDragArgs startDragArgs;
        internal SetupDragAndDropArgs(Element draggedElement, IEnumerable<int> selectedIds, StartDragArgs startDragArgs)
        {
            this.draggedElement = draggedElement;
            this.selectedIds = selectedIds;
            this.startDragArgs = startDragArgs;
        }
    }

    public readonly struct HandleDragAndDropArgs
    {
        public Vector2 position { get; }
        public int insertAtIndex { get; }
        public DragAndDropPosition dropPosition { get; }
        // A tree's drop: the item it goes into (-1: the roots) and its place among that item's children (-1: last).
        public int parentId { get; }
        public int childIndex { get; }
        internal HandleDragAndDropArgs(Vector2 position, int insertAtIndex, DragAndDropPosition dropPosition, int parentId = -1, int childIndex = -1)
        {
            this.position = position;
            this.insertAtIndex = insertAtIndex;
            this.dropPosition = dropPosition;
            this.parentId = parentId;
            this.childIndex = childIndex;
        }
    }

    public enum DragVisualMode
    {
        None,
        Copy,
        Move,
        Rejected,
    }

    // ------------------------------------------------------------------------------------------------- lists

    public class BaseListViewController : CollectionViewController
    {
        public event Action<IEnumerable<int>> itemsAdded;
        public event Action<IEnumerable<int>> itemsRemoved;
        protected BaseListView baseListView => view as BaseListView;

        protected internal override Element MakeItem() => new Label();
        protected internal override void BindItem(Element element, int index)
        {
            if (element is Label label)
                label.text = GetItemForIndex(index)?.ToString() ?? "";
        }

        public virtual void AddItems(int itemCount)
        {
            if (itemsSource == null)
                return;
            Type elementType = itemsSource.GetType().IsGenericType ? itemsSource.GetType().GetGenericArguments()[0] : null;
            var added = new List<int>();
            for (int i = 0; i < itemCount; i++)
            {
                object value = elementType != null && elementType.IsValueType ? Activator.CreateInstance(elementType) : null;
                if (elementType == typeof(string))
                    value = "";
                itemsSource.Add(value);
                added.Add(itemsSource.Count - 1);
            }
            itemsAdded?.Invoke(added);
            RaiseItemsSourceChanged();
        }

        public virtual void RemoveItems(List<int> indices)
        {
            if (itemsSource == null)
                return;
            indices.Sort();
            for (int i = indices.Count - 1; i >= 0; i--)
                if (indices[i] >= 0 && indices[i] < itemsSource.Count)
                    itemsSource.RemoveAt(indices[i]);
            itemsRemoved?.Invoke(indices);
            RaiseItemsSourceChanged();
        }

        public virtual void RemoveItem(int index) => RemoveItems(new List<int> { index });
        public virtual void ClearItems() => RemoveItems(Enumerable.Range(0, itemsSource?.Count ?? 0).ToList());
    }

    public class ListViewController : BaseListViewController
    {
        protected ListView listView => view as ListView;

        protected internal override Element MakeItem()
        {
            ListView v = listView;
            if (v?.makeItem != null)
                return v.makeItem();
            if (v?.itemTemplate != null)
                return v.itemTemplate.Instantiate();
            return base.MakeItem();
        }

        protected internal override void BindItem(Element element, int index)
        {
            ListView v = listView;
            if (v?.bindItem != null)
                v.bindItem(element, index);
            else
                base.BindItem(element, index);
        }

        protected internal override void UnbindItem(Element element, int index) => listView?.unbindItem?.Invoke(element, index);
        protected internal override void DestroyItem(Element element) => listView?.destroyItem?.Invoke(element);
    }

    // A list with a header (a foldout with the collection's size) and a footer adding and removing items.
    public abstract class BaseListView : BaseVerticalCollectionView
    {
        public new static readonly string hssClassName = "helix-list-view";
        public new static readonly string itemHssClassName = hssClassName + "__item";
        public static readonly string emptyLabelHssClassName = hssClassName + "__empty-label";
        public static readonly string foldoutHeaderHssClassName = hssClassName + "__foldout-header";
        public static readonly string arraySizeFieldHssClassName = hssClassName + "__size-field";
        public static readonly string footerHssClassName = hssClassName + "__footer";
        public static readonly string footerAddButtonName = hssClassName + "__add-button";
        public static readonly string footerRemoveButtonName = hssClassName + "__remove-button";
        public static readonly string reorderableHssClassName = hssClassName + "__reorderable";
        public static readonly string reorderableItemHssClassName = reorderableHssClassName + "-item";
        public static readonly string reorderableItemHandleHssClassName = reorderableHssClassName + "-handle";
        public static readonly string listViewWithHeaderHssClassName = hssClassName + "--with-header";
        public static readonly string listViewWithFooterHssClassName = hssClassName + "--with-footer";
        public static readonly string scrollViewWithFooterHssClassName = hssClassName + "__scroll-view--with-footer";

        Foldout m_Header;
        Element m_Footer;
        Label m_Empty;
        IntegerField m_Size;
        bool m_ShowHeader, m_ShowFooter, m_ShowSize = true;
        string m_HeaderTitle = "";

        public event Action<IEnumerable<int>> itemsAdded;
        public event Action<IEnumerable<int>> itemsRemoved;
        public Action<BaseListView> onAdd { get; set; }
        public Action<BaseListView> onRemove { get; set; }
        public Func<BaseListView, Button, bool> overridingAddButtonBehavior { get; set; }

        protected internal BaseListView(ObjectId id) : base(id) { }

        public BaseListView() : base()
        {
            AddToClassList(hssClassName);
            m_Empty = new Label("List is empty");
            m_Empty.AddToClassList(emptyLabelHssClassName);
            itemsSourceChanged += UpdateParts;
        }

        public BaseListView(IList itemsSource, float itemHeight = -1) : this()
        {
            if (itemHeight > 0)
                fixedItemHeight = itemHeight;
            this.itemsSource = itemsSource;
        }

        public new BaseListViewController viewController => base.viewController as BaseListViewController;

        [HxmlAttribute]
        public bool showFoldoutHeader
        {
            get => m_ShowHeader;
            set
            {
                m_ShowHeader = value;
                UpdateParts();
            }
        }

        [HxmlAttribute]
        public string headerTitle
        {
            get => m_HeaderTitle;
            set
            {
                m_HeaderTitle = value ?? "";
                UpdateParts();
            }
        }

        [HxmlAttribute]
        public bool showAddRemoveFooter
        {
            get => m_ShowFooter;
            set
            {
                m_ShowFooter = value;
                UpdateParts();
            }
        }

        [HxmlAttribute]
        public bool showBoundCollectionSize
        {
            get => m_ShowSize;
            set
            {
                m_ShowSize = value;
                UpdateParts();
            }
        }

        [HxmlAttribute]
        public bool allowAdd { get; set; } = true;

        [HxmlAttribute]
        public bool allowRemove { get; set; } = true;

        [HxmlAttribute]
        public ListViewReorderMode reorderMode { get; set; } = ListViewReorderMode.Simple;

        public BindingSourceSelectionMode bindingSourceSelectionMode { get; set; } = BindingSourceSelectionMode.Manual;

        void UpdateParts()
        {
            if (m_ShowHeader && m_Header == null)
            {
                m_Header = new Foldout();
                m_Header.AddToClassList(foldoutHeaderHssClassName);
                hierarchy.Insert(0, m_Header);
                m_Header.RegisterValueChangedCallback(e => scrollView.style.display = e.newValue ? DisplayStyle.Flex : DisplayStyle.None);
                m_Size = new IntegerField();
                m_Size.AddToClassList(arraySizeFieldHssClassName);
                m_Size.isDelayed = true;
                m_Size.RegisterValueChangedCallback(e => Resize(e.newValue));
                m_Header.toggle.hierarchy.Add(m_Size);
            }
            if (m_Header != null)
            {
                m_Header.style.display = m_ShowHeader ? DisplayStyle.Flex : DisplayStyle.None;
                m_Header.text = m_HeaderTitle;
                m_Size.style.display = m_ShowSize ? DisplayStyle.Flex : DisplayStyle.None;
                m_Size.SetValueWithoutNotify(itemsSource?.Count ?? 0);
            }
            EnableInClassList(listViewWithHeaderHssClassName, m_ShowHeader);
            if (m_ShowFooter && m_Footer == null)
            {
                m_Footer = new Element();
                m_Footer.AddToClassList(footerHssClassName);
                var add = new Button(OnAddClicked) { text = "+", name = footerAddButtonName };
                var remove = new Button(OnRemoveClicked) { text = "-", name = footerRemoveButtonName };
                m_Footer.Add(add);
                m_Footer.Add(remove);
                hierarchy.Add(m_Footer);
            }
            if (m_Footer != null)
                m_Footer.style.display = m_ShowFooter ? DisplayStyle.Flex : DisplayStyle.None;
            EnableInClassList(listViewWithFooterHssClassName, m_ShowFooter);
            bool empty = (itemsSource?.Count ?? 0) == 0;
            if (empty && m_Empty.parent == null)
                scrollView.contentViewport.Add(m_Empty);
            else if (!empty && m_Empty.parent != null)
                m_Empty.RemoveFromHierarchy();
        }

        void Resize(int size)
        {
            int count = itemsSource?.Count ?? 0;
            if (size > count)
                viewController.AddItems(size - count);
            else if (size < count)
                viewController.RemoveItems(Enumerable.Range(size, count - size).ToList());
        }

        void OnAddClicked()
        {
            if (!allowAdd)
                return;
            int before = itemsSource?.Count ?? 0;
            if (onAdd != null)
                onAdd(this);
            else
                viewController.AddItems(1);
            int after = itemsSource?.Count ?? 0;
            if (after > before)
            {
                itemsAdded?.Invoke(Enumerable.Range(before, after - before));
                RefreshItems();
                SetSelection(after - 1);
                ScrollToItem(after - 1);
            }
        }

        void OnRemoveClicked()
        {
            if (!allowRemove || itemsSource == null || itemsSource.Count == 0)
                return;
            var removed = selectedIndices.Any() ? selectedIndices.ToList() : new List<int> { itemsSource.Count - 1 };
            if (onRemove != null)
                onRemove(this);
            else
                viewController.RemoveItems(removed);
            itemsRemoved?.Invoke(removed);
            ClearSelection();
            RefreshItems();
        }
    }

    // A list of items made by makeItem (or the item template) and bound by bindItem.
    [HxmlElement]
    public class ListView : BaseListView
    {
        Func<Element> m_MakeItem;
        Action<Element, int> m_BindItem;

        public ListView() : base()
        {
            SetViewController(CreateViewController());
        }

        public ListView(IList itemsSource, float itemHeight = -1, Func<Element> makeItem = null, Action<Element, int> bindItem = null) : this()
        {
            if (itemHeight > 0)
                fixedItemHeight = itemHeight;
            m_MakeItem = makeItem;
            m_BindItem = bindItem;
            this.itemsSource = itemsSource;
        }

        protected override CollectionViewController CreateViewController() => new ListViewController();
        public new ListViewController viewController => base.viewController as ListViewController;

        public Func<Element> makeItem
        {
            get => m_MakeItem;
            set
            {
                m_MakeItem = value;
                Rebuild();
            }
        }

        public Action<Element, int> bindItem
        {
            get => m_BindItem;
            set
            {
                m_BindItem = value;
                RefreshItems();
            }
        }

        public Action<Element, int> unbindItem { get; set; }
        public Action<Element> destroyItem { get; set; }
        public UITemplate itemTemplate { get; set; }
    }

    // --------------------------------------------------------------------------------------------------- trees

    public readonly struct TreeViewItemData<T>
    {
        readonly List<TreeViewItemData<T>> m_Children;
        public int id { get; }
        public T data { get; }
        public IEnumerable<TreeViewItemData<T>> children => m_Children ?? (IEnumerable<TreeViewItemData<T>>)Array.Empty<TreeViewItemData<T>>();
        public bool hasChildren => m_Children != null && m_Children.Count > 0;

        public TreeViewItemData(int id, T data, List<TreeViewItemData<T>> children = null)
        {
            this.id = id;
            this.data = data;
            m_Children = children;
        }

        internal List<TreeViewItemData<T>> childList => m_Children;
    }

    public class TreeViewExpansionChangedArgs
    {
        public int id { get; internal set; }
        public bool isExpanded { get; internal set; }
        public bool isAppliedToAllChildren { get; internal set; }
    }

    // A tree's items flattened into the rows of the expanded ones: each row its indent, its toggle and its item.
    public abstract class BaseTreeViewController : CollectionViewController
    {
        protected internal sealed class Node
        {
            public int id, parent = -1, depth;
            public object data;
            public List<int> children = new List<int>();
        }

        protected internal readonly Dictionary<int, Node> nodes = new Dictionary<int, Node>();
        protected internal readonly List<int> roots = new List<int>();
        readonly HashSet<int> m_Expanded = new HashSet<int>();
        readonly List<int> m_Rows = new List<int>();
        protected BaseTreeView treeView => view as BaseTreeView;

        internal void Flatten()
        {
            m_Rows.Clear();
            void Add(int id)
            {
                m_Rows.Add(id);
                if (m_Expanded.Contains(id))
                    foreach (int c in nodes[id].children)
                        Add(c);
            }
            foreach (int r in roots)
                Add(r);
            itemsSource = m_Rows.ToList();
            RaiseItemsSourceChanged();
        }

        public override int GetItemsCount() => m_Rows.Count;
        public override int GetIdForIndex(int index) => index >= 0 && index < m_Rows.Count ? m_Rows[index] : -1;
        public override int GetIndexForId(int id) => m_Rows.IndexOf(id);
        public override object GetItemForIndex(int index) => nodes.TryGetValue(GetIdForIndex(index), out Node n) ? n.data : null;
        public int GetParentId(int id) => nodes.TryGetValue(id, out Node n) ? n.parent : -1;
        public IEnumerable<int> GetChildrenIds(int id) => nodes.TryGetValue(id, out Node n) ? n.children : Enumerable.Empty<int>();
        public IEnumerable<int> GetRootItemIds() => roots;
        public bool HasChildren(int id) => nodes.TryGetValue(id, out Node n) && n.children.Count > 0;
        public bool IsExpanded(int id) => m_Expanded.Contains(id);
        public int GetTreeItemsCount() => nodes.Count;

        public void ExpandItem(int id, bool expandAllChildren, bool refresh = true)
        {
            if (!nodes.ContainsKey(id))
                return;
            m_Expanded.Add(id);
            if (expandAllChildren)
                foreach (int c in nodes[id].children)
                    ExpandItem(c, true, false);
            if (refresh)
                Flatten();
        }

        public void CollapseItem(int id, bool collapseAllChildren, bool refresh = true)
        {
            m_Expanded.Remove(id);
            if (collapseAllChildren && nodes.ContainsKey(id))
                foreach (int c in nodes[id].children)
                    CollapseItem(c, true, false);
            if (refresh)
                Flatten();
        }

        public void ExpandAll()
        {
            foreach (int id in nodes.Keys)
                m_Expanded.Add(id);
            Flatten();
        }

        public void CollapseAll()
        {
            m_Expanded.Clear();
            Flatten();
        }

        protected internal void AddNode(int id, object data, int parent, int depth)
        {
            nodes[id] = new Node { id = id, data = data, parent = parent, depth = depth };
            if (parent < 0)
                roots.Add(id);
            else
                nodes[parent].children.Add(id);
        }

        protected internal void ClearNodes()
        {
            nodes.Clear();
            roots.Clear();
        }

        protected internal abstract Element MakeContent();
        protected internal abstract void BindContent(Element element, int index);

        protected internal override Element MakeItem()
        {
            var row = new Element();
            row.AddToClassList(BaseTreeView.itemHssClassName);
            row.style.flexDirection = FlexDirection.Row;
            var indents = new Element();
            indents.AddToClassList(BaseTreeView.itemIndentsContainerHssClassName);
            var toggle = new Toggle();
            toggle.AddToClassList(BaseTreeView.itemToggleHssClassName);
            var content = new Element();
            content.AddToClassList(BaseTreeView.itemContentContainerHssClassName);
            content.Add(MakeContent());
            row.Add(indents);
            row.Add(toggle);
            row.Add(content);
            toggle.RegisterValueChangedCallback(e =>
            {
                int id = (int)(row.userData ?? -1);
                if (id < 0)
                    return;
                treeView?.SetExpanded(id, e.newValue, false);
            });
            return row;
        }

        protected internal override void BindItem(Element element, int index)
        {
            int id = GetIdForIndex(index);
            element.userData = id;
            Element indents = element[0];
            var toggle = (Toggle)element[1];
            Element content = element[2];
            indents.style.width = nodes.TryGetValue(id, out Node n) ? n.depth * 15 : 0;
            toggle.SetValueWithoutNotify(m_Expanded.Contains(id));
            toggle.style.visibility = HasChildren(id) ? Visibility.Visible : Visibility.Hidden;
            BindContent(content[0], index);
        }
    }

    public abstract class BaseTreeView : BaseVerticalCollectionView
    {
        public new static readonly string hssClassName = "helix-tree-view";
        public new static readonly string itemHssClassName = hssClassName + "__item";
        public static readonly string itemToggleHssClassName = hssClassName + "__item-toggle";
        public static readonly string itemIndentsContainerHssClassName = hssClassName + "__item-indents";
        public static readonly string itemIndentHssClassName = hssClassName + "__item-indent";
        public static readonly string itemContentContainerHssClassName = hssClassName + "__item-content";

        public event Action<TreeViewExpansionChangedArgs> itemExpandedChanged;

        protected internal BaseTreeView(ObjectId id) : base(id) { }

        public BaseTreeView() : base() { AddToClassList(hssClassName); }

        public new BaseTreeViewController viewController => base.viewController as BaseTreeViewController;

        internal override bool dropsOnItems => true;

        // Into the row's item (last among its children); between rows: beside the row below (in its parent, at its
        // place), after the last row: the last root.
        internal override void DropTarget(int insertAtIndex, DragAndDropPosition position, out int parentId, out int childIndex)
        {
            int count = viewController?.GetItemsCount() ?? 0;
            if (position == DragAndDropPosition.OverItem && insertAtIndex < count)
            {
                parentId = GetIdForIndex(insertAtIndex);
                childIndex = -1;
                return;
            }
            if (insertAtIndex >= count)
            {
                parentId = -1;
                childIndex = -1;
                return;
            }
            int id = GetIdForIndex(insertAtIndex);
            parentId = viewController.GetParentId(id);
            IEnumerable<int> siblings = parentId < 0 ? viewController.GetRootItemIds() : viewController.GetChildrenIds(parentId);
            childIndex = 0;
            foreach (int s in siblings)
            {
                if (s == id)
                    break;
                childIndex++;
            }
        }

        [HxmlAttribute]
        public bool autoExpand { get; set; }

        public void SetRootItems<T>(IList<TreeViewItemData<T>> rootItems)
        {
            BaseTreeViewController c = (BaseTreeViewController)EnsureController();
            c.ClearNodes();
            void Add(TreeViewItemData<T> item, int parent, int depth)
            {
                c.AddNode(item.id, item.data, parent, depth);
                foreach (TreeViewItemData<T> child in item.children)
                    Add(child, item.id, depth + 1);
            }
            foreach (TreeViewItemData<T> item in rootItems)
                Add(item, -1, 0);
            if (autoExpand)
                c.ExpandAll();
            else
                c.Flatten();
        }

        public T GetItemDataForIndex<T>(int index) => (T)viewController.GetItemForIndex(index);
        public T GetItemDataForId<T>(int id) => viewController.nodes.TryGetValue(id, out var n) ? (T)n.data : default;
        public int GetIdForIndex(int index) => viewController.GetIdForIndex(index);
        public int GetParentIdForIndex(int index) => viewController.GetParentId(GetIdForIndex(index));
        public IEnumerable<int> GetChildrenIdsForIndex(int index) => viewController.GetChildrenIds(GetIdForIndex(index));
        public IEnumerable<int> GetRootIds() => viewController.GetRootItemIds();
        public int GetTreeCount() => viewController.GetTreeItemsCount();
        public bool IsExpanded(int id) => viewController.IsExpanded(id);
        public void ExpandItem(int id, bool expandAllChildren = false) => SetExpanded(id, true, expandAllChildren);
        public void CollapseItem(int id, bool collapseAllChildren = false) => SetExpanded(id, false, collapseAllChildren);
        public void ExpandRootItems()
        {
            foreach (int id in viewController.GetRootItemIds().ToArray())
                viewController.ExpandItem(id, false, false);
            viewController.Flatten();
        }
        public void ExpandAll() => viewController.ExpandAll();
        public void CollapseAll() => viewController.CollapseAll();

        internal void SetExpanded(int id, bool expanded, bool all)
        {
            if (expanded)
                viewController.ExpandItem(id, all);
            else
                viewController.CollapseItem(id, all);
            itemExpandedChanged?.Invoke(new TreeViewExpansionChangedArgs { id = id, isExpanded = expanded, isAppliedToAllChildren = all });
        }
    }

    public class TreeViewController : BaseTreeViewController
    {
        protected TreeView treeViewTyped => view as TreeView;

        protected internal override Element MakeContent()
        {
            TreeView v = treeViewTyped;
            if (v?.makeItem != null)
                return v.makeItem();
            if (v?.itemTemplate != null)
                return v.itemTemplate.Instantiate();
            return new Label();
        }

        protected internal override void BindContent(Element element, int index)
        {
            TreeView v = treeViewTyped;
            if (v?.bindItem != null)
                v.bindItem(element, index);
            else if (element is Label label)
                label.text = GetItemForIndex(index)?.ToString() ?? "";
        }

        protected internal override void UnbindItem(Element element, int index) => treeViewTyped?.unbindItem?.Invoke(element[2][0], index);
        protected internal override void DestroyItem(Element element) => treeViewTyped?.destroyItem?.Invoke(element[2][0]);
    }

    public class DefaultTreeViewController<T> : TreeViewController
    {
        public T GetDataForId(int id) => nodes.TryGetValue(id, out Node n) ? (T)n.data : default;
        public T GetDataForIndex(int index) => (T)GetItemForIndex(index);
    }

    [HxmlElement]
    public class TreeView : BaseTreeView
    {
        public TreeView() : base() { SetViewController(CreateViewController()); }

        public TreeView(Func<Element> makeItem, Action<Element, int> bindItem) : this()
        {
            this.makeItem = makeItem;
            this.bindItem = bindItem;
        }

        protected override CollectionViewController CreateViewController() => new TreeViewController();
        public new TreeViewController viewController => base.viewController as TreeViewController;
        public Func<Element> makeItem { get; set; }
        public Action<Element, int> bindItem { get; set; }
        public Action<Element, int> unbindItem { get; set; }
        public Action<Element> destroyItem { get; set; }
        public UITemplate itemTemplate { get; set; }
    }
}
