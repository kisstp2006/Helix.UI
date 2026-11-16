using System;
using System.Text;
using Helix.Scripting;
using HelixEngine;
using HelixEngine.UI;

namespace Tests
{
    // The lines the C# tests write, read by the C++ test program (its export by name).
    static unsafe class Notes
    {
        static delegate* unmanaged[Cdecl]<byte*, int, void> s_Note;

        public static void Note(string message)
        {
            if (s_Note == null)
                s_Note = (delegate* unmanaged[Cdecl]<byte*, int, void>)Host.FindExport("UITests.Note");
            byte[] bytes = Encoding.UTF8.GetBytes(message);
            fixed (byte* p = bytes)
                s_Note(p, bytes.Length);
        }

        public static string Throws(Action a)
        {
            try
            {
                a();
                return "none";
            }
            catch (Exception e)
            {
                return e.GetType().Name;
            }
        }
    }

    // Elements made, nested, moved and queried from C#.
    public class CsElementTests : GameBehaviour
    {
        void Start()
        {
            var root = new Element { name = "root" };
            var a = new Element { name = "a" };
            var b = new Element { name = "b" };
            var c = new Element { name = "c" };
            root.Add(a);
            root.Add(b);
            root.Insert(1, c);
            Notes.Note($"children:{root.childCount}:{root[0].name}{root[1].name}{root[2].name}:{root.IndexOf(b)}:{(a.parent == root)}");
            b.SendToBack();
            a.BringToFront();
            Notes.Note($"order:{root[0].name}{root[1].name}{root[2].name}");
            b.PlaceInFront(c);
            Notes.Note($"placed:{root[0].name}{root[1].name}{root[2].name}");
            root.Remove(c);
            Notes.Note($"removed:{root.childCount}:{(c.parent == null)}:{Notes.Throws(() => root.Remove(c))}:{Notes.Throws(() => root.ElementAt(5))}:{Notes.Throws(() => root.Insert(9, c))}");

            a.AddToClassList("card");
            a.AddToClassList("warm");
            a.ToggleInClassList("warm");
            a.EnableInClassList("big", true);
            Notes.Note($"classes:{a.ClassListContains("card")}:{a.ClassListContains("warm")}:{string.Join(",", a.GetClasses())}");

            var inner = new Element { name = "inner" };
            inner.AddToClassList("card");
            b.Add(inner);
            Notes.Note($"find:{root.Find("inner")?.name}:{root.Find(null, "card")?.name}:{root.FindAll(null, "card").Count}:{(root.Find("none") == null)}");
            Notes.Note($"ancestors:{inner.GetFirstAncestorOfType<Element>().name}:{inner.FindCommonAncestor(a).name}:{root.Contains(inner)}:{a.Contains(inner)}");

            a.SetEnabled(false);
            a.focusable = true;
            a.tabIndex = 3;
            a.pickingMode = PickingMode.Ignore;
            Notes.Note($"state:{a.enabledSelf}:{a.enabledInHierarchy}:{a.focusable}:{a.tabIndex}:{a.pickingMode}:{a.canGrabFocus}");
            a.userData = this;
            Notes.Note($"userData:{(a.userData == (object)this)}");
        }
    }
}
