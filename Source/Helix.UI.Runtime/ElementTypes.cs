using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Helix.Interop;
using Helix.Scripting;
using HelixEngine.Internal;

namespace HelixEngine.UI
{
    // An element class markup makes: <Name> (the class's name unless given) with the class's HxmlAttribute members.
    [AttributeUsage(AttributeTargets.Class, Inherited = false)]
    public sealed class HxmlElementAttribute : Attribute
    {
        public readonly string name;

        public HxmlElementAttribute() { }
        public HxmlElementAttribute(string name) { this.name = name; }
    }

    // A field or property markup sets: by its name in kebab case (myValue is my-value) unless given.
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public sealed class HxmlAttributeAttribute : Attribute
    {
        public string name { get; set; }
        public string[] obsoleteNames { get; set; }

        public HxmlAttributeAttribute() { }
        public HxmlAttributeAttribute(string name) { this.name = name; }

        public HxmlAttributeAttribute(string name, params string[] obsoleteNames)
        {
            this.name = name;
            this.obsoleteNames = obsoleteNames;
        }
    }

    // The element types of C# classes: found in every assembly the engine registers, given to the native markup,
    // which has the C# side make their objects (attributes set from the markup's text) and name their elements' type.
    internal static unsafe class ElementTypes
    {
        sealed class Entry
        {
            public Type Type;
            public readonly Dictionary<string, Action<object, string>> Setters = new Dictionary<string, Action<object, string>>();
        }

        static readonly List<Entry> s_Types = new List<Entry>();
        static readonly Dictionary<Type, Entry> s_Known = new Dictionary<Type, Entry>();
        static delegate* unmanaged[Cdecl]<byte*, int, byte*, int, int, byte*, int, byte*, int, void> s_Register;
        // The object made last: held until its element is in a tree (which keeps it).
        static Element s_Last;

        // The engine runs the module initializers of the assemblies it registers: the UI's types are set up before any
        // markup is built.
#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Start()
        {
            void* register = Host.FindExport("HelixUI.RegisterElementClass");
            void* factory = Host.FindExport("HelixUI.SetElementFactory");
            if (register == null || factory == null)
                return;
            s_Register = (delegate* unmanaged[Cdecl]<byte*, int, byte*, int, int, byte*, int, byte*, int, void>)register;
            delegate* unmanaged[Cdecl]<int, byte*, int, ulong> create = &Create;
            delegate* unmanaged[Cdecl]<ulong, byte*, int, int> name = &NameOf;
            ((delegate* unmanaged[Cdecl]<void*, void*, void>)factory)(create, name);
            ScriptBridge.OnAssembly(Scan);
        }

        static void Scan(Assembly assembly)
        {
            Type[] types;
            try
            {
                types = assembly.GetTypes();
            }
            catch (ReflectionTypeLoadException e)
            {
                types = e.Types;
            }
            var found = new List<Type>();
            foreach (Type t in types)
                if (t != null && t.IsClass && !t.IsAbstract && !t.IsGenericTypeDefinition && typeof(Element).IsAssignableFrom(t) &&
                    t.GetCustomAttribute<HxmlElementAttribute>(false) != null && !Types.IsGenerated(t))
                    found.Add(t);
            // Bases first (a class's base type is registered when it is).
            found.Sort((a, b) => Depth(a).CompareTo(Depth(b)));
            foreach (Type t in found)
                Register(t);
        }

        static int Depth(Type t)
        {
            int d = 0;
            for (Type c = t; c != null; c = c.BaseType)
                d++;
            return d;
        }

        static void Register(Type t)
        {
            if (s_Known.ContainsKey(t))
                return;
            if (t.GetConstructor(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic, null, Type.EmptyTypes, null) == null)
            {
                Debug.LogError($"{t.FullName}: an element class markup makes needs a constructor without parameters");
                return;
            }
            var entry = new Entry { Type = t };
            // Its attributes and those of its bases up to a registered one (whose own come from its registration) or a
            // generated class.
            Type baseType = t.BaseType;
            for (Type c = t; c != null && c != typeof(object); c = c.BaseType)
            {
                if (c != t && (Types.IsGenerated(c) || s_Known.ContainsKey(c)))
                {
                    baseType = c;
                    // A registered base's attributes are members of this class too.
                    if (s_Known.TryGetValue(c, out Entry registered))
                        foreach (var pair in registered.Setters)
                            if (!entry.Setters.ContainsKey(pair.Key))
                                entry.Setters[pair.Key] = pair.Value;
                    break;
                }
                const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;
                foreach (MemberInfo m in c.GetMembers(flags))
                {
                    var attribute = m.GetCustomAttribute<HxmlAttributeAttribute>();
                    if (attribute == null || !(m is FieldInfo || m is PropertyInfo))
                        continue;
                    Action<object, string> set = Setter(m);
                    if (set == null)
                    {
                        Debug.LogError($"{t.FullName}.{m.Name}: markup sets strings, bools, numbers, enums and colors");
                        continue;
                    }
                    entry.Setters[attribute.name ?? Kebab(m.Name)] = set;
                    if (attribute.obsoleteNames != null)
                        foreach (string old in attribute.obsoleteNames)
                            entry.Setters[old] = set;
                }
            }
            int index = s_Types.Count;
            s_Types.Add(entry);
            s_Known[t] = entry;
            var list = new StringBuilder();
            foreach (string a in entry.Setters.Keys)
                list.Append(a).Append('\0');
            string markupName = t.GetCustomAttribute<HxmlElementAttribute>(false)?.name ?? t.Name;
            byte[] name = Encoding.UTF8.GetBytes(markupName), className = Encoding.UTF8.GetBytes(t.Name);
            byte[] attributes = Encoding.UTF8.GetBytes(list.ToString()), basis = Encoding.UTF8.GetBytes(baseType?.Name ?? "Element");
            fixed (byte* n = name, c = className, a = attributes, b = basis)
                s_Register(n, name.Length, c, className.Length, index, a, attributes.Length, b, basis.Length);
        }

        // myValue is my-value.
        internal static string Kebab(string name)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < name.Length; i++)
            {
                char ch = name[i];
                if (char.IsUpper(ch) && i > 0 && (char.IsLower(name[i - 1]) || char.IsDigit(name[i - 1])))
                    sb.Append('-');
                sb.Append(char.ToLowerInvariant(ch));
            }
            return sb.ToString();
        }

        static Action<object, string> Setter(MemberInfo m)
        {
            Type type = m is FieldInfo f ? f.FieldType : ((PropertyInfo)m).PropertyType;
            Func<string, object> convert = Converter(type);
            if (convert == null)
                return null;
            if (m is FieldInfo field)
                return (o, text) => field.SetValue(o, convert(text));
            var property = (PropertyInfo)m;
            if (!property.CanWrite)
                return null;
            return (o, text) => property.SetValue(o, convert(text));
        }

        static Func<string, object> Converter(Type type)
        {
            CultureInfo c = CultureInfo.InvariantCulture;
            if (type == typeof(string))
                return s => s;
            if (type == typeof(bool))
                return s => bool.Parse(s.Trim());
            if (type == typeof(int))
                return s => int.Parse(s.Trim(), NumberStyles.Integer, c);
            if (type == typeof(long))
                return s => long.Parse(s.Trim(), NumberStyles.Integer, c);
            if (type == typeof(float))
                return s => float.Parse(s.Trim(), NumberStyles.Float, c);
            if (type == typeof(double))
                return s => double.Parse(s.Trim(), NumberStyles.Float, c);
            if (type == typeof(Color))
                return s => StyleText.TryColor(s, out Color color) ? color : throw new FormatException($"'{s}' is not a color");
            if (type.IsEnum)
                return s => Enum.Parse(type, s.Trim().Replace("-", ""), true);
            return null;
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static ulong Create(int index, byte* attributes, int length)
        {
            try
            {
                if (index < 0 || index >= s_Types.Count)
                    return 0;
                Entry entry = s_Types[index];
                var element = (Element)Activator.CreateInstance(entry.Type, nonPublic: true);
                s_Last = element;
                // Names and values, each ended by a zero.
                string packed = Encoding.UTF8.GetString(attributes, length);
                string[] parts = packed.Split('\0');
                for (int i = 0; i + 1 < parts.Length; i += 2)
                {
                    if (!entry.Setters.TryGetValue(parts[i], out Action<object, string> set))
                        continue;
                    try
                    {
                        set(element, parts[i + 1]);
                    }
                    catch (Exception e)
                    {
                        Exception inner = e is TargetInvocationException t && t.InnerException != null ? t.InnerException : e;
                        Debug.LogError($"<{entry.Type.Name}> {parts[i]}=\"{parts[i + 1]}\": {inner.Message}");
                    }
                }
                return (ulong)element.handleIndex() << 32 | element.handleGeneration();
            }
            catch (Exception e)
            {
                Debug.LogException(e is TargetInvocationException t && t.InnerException != null ? t.InnerException : e);
                return 0;
            }
        }

        // The C# class of an element object (by its wrapper's handle) when it is no generated class: its elements' type.
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static int NameOf(ulong wrapper, byte* buffer, int capacity)
        {
            try
            {
                object target = wrapper != 0 ? GCHandle.FromIntPtr((IntPtr)wrapper).Target : null;
                if (target == null || Types.IsGenerated(target.GetType()))
                    return 0;
                if (ReferenceEquals(target, s_Last))
                    s_Last = null;
                byte[] name = Encoding.UTF8.GetBytes(target.GetType().Name);
                if (name.Length > capacity)
                    return 0;
                for (int i = 0; i < name.Length; i++)
                    buffer[i] = name[i];
                return name.Length;
            }
            catch (Exception)
            {
                return 0;
            }
        }
    }
}
