using System;

namespace HelixEngine.UI
{
    public partial class UITemplate
    {
        // Its elements built into target (its content container): where the first of them is among the container's
        // children, and how many it added there.
        public void CloneTree(Element target, out int firstElementIndex, out int elementAddedCount)
        {
            if (target == null)
                throw new ArgumentNullException(nameof(target));
            Element into = target.contentContainer ?? target;
            int before = into.childCount;
            CloneTree(target);
            firstElementIndex = before;
            elementAddedCount = into.childCount - before;
        }
    }

    // The style sheets added to an element (not those it inherits from its ancestors): they apply to it and its
    // descendants, a later one winning at the same specificity.
    public struct ElementStyleSheetSet : IEquatable<ElementStyleSheetSet>
    {
        readonly Element m_Element;

        internal ElementStyleSheetSet(Element element) { m_Element = element; }

        public int count => m_Element.StyleSheetCount();

        public StyleSheet this[int index]
        {
            get
            {
                if (index < 0 || index >= count)
                    throw new ArgumentOutOfRangeException(nameof(index));
                return m_Element.StyleSheetAt(index);
            }
        }

        public void Add(StyleSheet styleSheet)
        {
            if (styleSheet == null)
                throw new ArgumentNullException(nameof(styleSheet));
            m_Element.AddStyleSheet(styleSheet);
        }

        public void Insert(int index, StyleSheet styleSheet)
        {
            if (styleSheet == null)
                throw new ArgumentNullException(nameof(styleSheet));
            if (index < 0 || index > count)
                throw new ArgumentOutOfRangeException(nameof(index));
            m_Element.InsertStyleSheet(index, styleSheet);
        }

        public bool Remove(StyleSheet styleSheet)
        {
            if (styleSheet == null)
                throw new ArgumentNullException(nameof(styleSheet));
            return m_Element.RemoveStyleSheet(styleSheet);
        }

        public bool Contains(StyleSheet styleSheet) => styleSheet != null && m_Element.ContainsStyleSheet(styleSheet);

        public void Clear() => m_Element.ClearStyleSheets();

        public static bool operator ==(ElementStyleSheetSet left, ElementStyleSheetSet right) => left.Equals(right);
        public static bool operator !=(ElementStyleSheetSet left, ElementStyleSheetSet right) => !left.Equals(right);
        public bool Equals(ElementStyleSheetSet other) => ReferenceEquals(m_Element, other.m_Element);
        public override bool Equals(object obj) => obj is ElementStyleSheetSet s && Equals(s);
        public override int GetHashCode() => m_Element != null ? m_Element.GetHashCode() : 0;
    }

    public partial class Element
    {
        public ElementStyleSheetSet styleSheets => new ElementStyleSheetSet(this);
    }
}
