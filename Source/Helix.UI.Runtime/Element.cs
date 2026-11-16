using System;
using System.Collections.Generic;

namespace HelixEngine.UI
{
    // What C# has of Element beside its bindings: the reference's argument checks, the indexer, the children as an
    // enumeration, the hierarchy, userData.
    public partial class Element
    {
        // The children of the element itself (contentContainer aside).
        public struct Hierarchy
        {
            readonly Element m_Owner;

            internal Hierarchy(Element owner) { m_Owner = owner; }

            public Element parent => m_Owner.parent;
            public int childCount => m_Owner.HierarchyChildCount();

            public Element this[int index]
            {
                get
                {
                    if (index < 0 || index >= childCount)
                        throw new ArgumentOutOfRangeException(nameof(index));
                    return m_Owner.HierarchyElementAt(index);
                }
            }

            public void Add(Element child)
            {
                if (child == null)
                    throw new ArgumentException("Cannot add null child");
                m_Owner.HierarchyInsert(-1, child);
            }

            public void Insert(int index, Element child)
            {
                if (child == null)
                    throw new ArgumentException("Cannot insert null child");
                if (index < 0 || index > childCount)
                    throw new ArgumentOutOfRangeException(nameof(index));
                m_Owner.HierarchyInsert(index, child);
            }

            public void Remove(Element child)
            {
                if (child == null)
                    throw new ArgumentException("Cannot remove null child");
                if (child.hierarchy.parent != m_Owner)
                    throw new ArgumentException("This visualElement is not my child");
                m_Owner.HierarchyRemove(child);
            }

            public void RemoveAt(int index) => Remove(this[index]);
            public int IndexOf(Element child) => child != null && child.hierarchy.parent == m_Owner ? IndexIn(m_Owner, child) : -1;
            public Element ElementAt(int index) => this[index];

            public IEnumerable<Element> Children()
            {
                int n = childCount;
                for (int i = 0; i < n; i++)
                    yield return m_Owner.HierarchyElementAt(i);
            }

            static int IndexIn(Element owner, Element child)
            {
                int n = owner.HierarchyChildCount();
                for (int i = 0; i < n; i++)
                    if (owner.HierarchyElementAt(i) == child)
                        return i;
                return -1;
            }
        }

        public Hierarchy hierarchy => new Hierarchy(this);

        object m_UserData;

        // Anything a script keeps with the element (its C# object is then kept while the element is in a tree).
        public object userData
        {
            get => m_UserData;
            set
            {
                m_UserData = value;
                Events.Keep(this);
            }
        }

        public Element this[int key] => ElementAt(key);

        public Element ElementAt(int index)
        {
            if (index < 0 || index >= childCount)
                throw new ArgumentOutOfRangeException(nameof(index));
            return ElementAtChecked(index);
        }

        public void Insert(int index, Element element)
        {
            if (element == null)
                throw new ArgumentException("Cannot insert null element");
            if (index < 0 || index > childCount)
                throw new ArgumentOutOfRangeException(nameof(index));
            InsertChecked(index, element);
        }

        public void Remove(Element element)
        {
            if (element == null)
                throw new ArgumentException("Cannot remove null element");
            if (!RemoveChecked(element))
                throw new ArgumentException("This visualElement is not my child");
        }

        public void RemoveAt(int index)
        {
            if (index < 0 || index >= childCount)
                throw new ArgumentOutOfRangeException(nameof(index));
            RemoveAtChecked(index);
        }

        public IEnumerable<Element> Children()
        {
            int n = childCount;
            for (int i = 0; i < n; i++)
                yield return ElementAtChecked(i);
        }

        public IEnumerable<string> GetClassNames() => GetClasses();

        // The first ancestor of a type (not the element itself).
        public T GetFirstAncestorOfType<T>() where T : Element
        {
            for (Element e = parent; e != null; e = e.parent)
                if (e is T t)
                    return t;
            return null;
        }

        // The element itself or its first ancestor of a type.
        public T GetFirstOfType<T>() where T : Element => this is T t ? t : GetFirstAncestorOfType<T>();

        // Sorts the children (of the content container) by a comparison.
        public void Sort(Comparison<Element> comp)
        {
            var list = new List<Element>(Children());
            list.Sort(comp);
            Element container = contentContainer;
            for (int i = 0; i < list.Count; i++)
                container.HierarchyInsert(i, list[i]);
        }
    }
}
