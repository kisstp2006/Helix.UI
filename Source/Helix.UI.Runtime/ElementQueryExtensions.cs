using System.Collections.Generic;

namespace HelixEngine.UI
{
    // Queries of a subtree (the reference's UQuery as Find and FindAll): by name and classes, of a type, in tree order;
    // the root itself counts.
    public static class ElementQueryExtensions
    {
        public static T Find<T>(this Element e, string name = null, params string[] classes) where T : Element
        {
            foreach (Element x in Walk(e))
                if (x is T t && Matches(x, name, classes))
                    return t;
            return null;
        }

        public static Element Find(this Element e, string name = null, params string[] classes) => Find<Element>(e, name, classes);

        public static List<T> FindAll<T>(this Element e, string name = null, params string[] classes) where T : Element
        {
            var found = new List<T>();
            foreach (Element x in Walk(e))
                if (x is T t && Matches(x, name, classes))
                    found.Add(t);
            return found;
        }

        public static List<Element> FindAll(this Element e, string name = null, params string[] classes) => FindAll<Element>(e, name, classes);

        static bool Matches(Element x, string name, string[] classes)
        {
            if (!string.IsNullOrEmpty(name) && x.name != name)
                return false;
            if (classes != null)
                foreach (string c in classes)
                    if (!string.IsNullOrEmpty(c) && !x.ClassListContains(c))
                        return false;
            return true;
        }

        // The subtree in tree order through the hierarchy (content containers aside).
        static IEnumerable<Element> Walk(Element root)
        {
            if (root == null)
                yield break;
            var stack = new Stack<Element>();
            stack.Push(root);
            while (stack.Count > 0)
            {
                Element x = stack.Pop();
                yield return x;
                Element.Hierarchy h = x.hierarchy;
                for (int i = h.childCount - 1; i >= 0; i--)
                    stack.Push(h[i]);
            }
        }
    }
}
