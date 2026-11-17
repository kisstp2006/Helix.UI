using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using System.Text;

namespace HelixEngine.Properties
{
    // A member of a type the property paths reach (fields and properties are reached when public or marked).
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public sealed class CreatePropertyAttribute : Attribute
    {
        public bool ReadOnly { get; set; }
    }

    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public sealed class DontCreatePropertyAttribute : Attribute { }

    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
    public sealed class GeneratePropertyBagAttribute : Attribute { }

    public enum PropertyPathPartKind
    {
        Name,
        Index,
        Key,
    }

    public readonly struct PropertyPathPart : IEquatable<PropertyPathPart>
    {
        readonly string m_Name;
        readonly int m_Index;
        readonly object m_Key;

        public PropertyPathPartKind Kind { get; }
        public string Name => Kind == PropertyPathPartKind.Name ? m_Name : throw new InvalidOperationException("The part is not a name.");
        public int Index => Kind == PropertyPathPartKind.Index ? m_Index : throw new InvalidOperationException("The part is not an index.");
        public object Key => Kind == PropertyPathPartKind.Key ? m_Key : throw new InvalidOperationException("The part is not a key.");
        public bool IsName => Kind == PropertyPathPartKind.Name;
        public bool IsIndex => Kind == PropertyPathPartKind.Index;
        public bool IsKey => Kind == PropertyPathPartKind.Key;

        public PropertyPathPart(string name)
        {
            Kind = PropertyPathPartKind.Name;
            m_Name = name;
            m_Index = -1;
            m_Key = null;
        }

        public PropertyPathPart(int index)
        {
            Kind = PropertyPathPartKind.Index;
            m_Name = null;
            m_Index = index;
            m_Key = null;
        }

        public PropertyPathPart(object key)
        {
            Kind = PropertyPathPartKind.Key;
            m_Name = null;
            m_Index = -1;
            m_Key = key;
        }

        public bool Equals(PropertyPathPart other) => Kind == other.Kind && m_Name == other.m_Name && m_Index == other.m_Index && Equals(m_Key, other.m_Key);
        public override bool Equals(object obj) => obj is PropertyPathPart p && Equals(p);
        public override int GetHashCode() => HashCode.Combine(Kind, m_Name, m_Index, m_Key);
        public override string ToString() => Kind switch
        {
            PropertyPathPartKind.Name => m_Name,
            PropertyPathPartKind.Index => $"[{m_Index}]",
            _ => $"[\"{m_Key}\"]",
        };
    }

    // A path through members, list indices and dictionary keys: "player.items[2].name".
    public readonly struct PropertyPath : IEquatable<PropertyPath>
    {
        readonly PropertyPathPart[] m_Parts;

        public PropertyPath(string path) { m_Parts = Parse(path); }
        PropertyPath(PropertyPathPart[] parts) { m_Parts = parts ?? Array.Empty<PropertyPathPart>(); }

        public int Length => m_Parts?.Length ?? 0;
        public bool IsEmpty => Length == 0;
        public PropertyPathPart this[int index] => m_Parts[index];

        public static PropertyPath FromName(string name) => new PropertyPath(new[] { new PropertyPathPart(name) });
        public static PropertyPath FromIndex(int index) => new PropertyPath(new[] { new PropertyPathPart(index) });
        public static PropertyPath FromKey(object key) => new PropertyPath(new[] { new PropertyPathPart(key) });
        public static PropertyPath FromPart(PropertyPathPart part) => new PropertyPath(new[] { part });

        public static PropertyPath Combine(in PropertyPath path, in PropertyPath pathToAppend)
        {
            var parts = new PropertyPathPart[path.Length + pathToAppend.Length];
            for (int i = 0; i < path.Length; i++)
                parts[i] = path[i];
            for (int i = 0; i < pathToAppend.Length; i++)
                parts[path.Length + i] = pathToAppend[i];
            return new PropertyPath(parts);
        }

        public static PropertyPath Combine(in PropertyPath path, string pathToAppend) => Combine(path, new PropertyPath(pathToAppend));
        public static PropertyPath AppendName(in PropertyPath path, string name) => Combine(path, FromName(name));
        public static PropertyPath AppendIndex(in PropertyPath path, int index) => Combine(path, FromIndex(index));
        public static PropertyPath AppendKey(in PropertyPath path, object key) => Combine(path, FromKey(key));

        public static PropertyPath Pop(in PropertyPath path)
        {
            if (path.Length == 0)
                return path;
            var parts = new PropertyPathPart[path.Length - 1];
            for (int i = 0; i < parts.Length; i++)
                parts[i] = path[i];
            return new PropertyPath(parts);
        }

        public static PropertyPath SubPath(in PropertyPath path, int startIndex)
        {
            var parts = new PropertyPathPart[Math.Max(0, path.Length - startIndex)];
            for (int i = 0; i < parts.Length; i++)
                parts[i] = path[startIndex + i];
            return new PropertyPath(parts);
        }

        static PropertyPathPart[] Parse(string path)
        {
            var parts = new List<PropertyPathPart>();
            if (string.IsNullOrEmpty(path))
                return parts.ToArray();
            int i = 0;
            while (i < path.Length)
            {
                if (path[i] == '.')
                {
                    i++;
                    continue;
                }
                if (path[i] == '[')
                {
                    int end = path.IndexOf(']', i);
                    if (end < 0)
                        throw new ArgumentException($"'{path}': a [ without its ]");
                    string inside = path.Substring(i + 1, end - i - 1).Trim();
                    if (inside.Length >= 2 && inside[0] == '"' && inside[inside.Length - 1] == '"')
                        parts.Add(new PropertyPathPart((object)inside.Substring(1, inside.Length - 2)));
                    else
                        parts.Add(new PropertyPathPart(int.Parse(inside, CultureInfo.InvariantCulture)));
                    i = end + 1;
                    continue;
                }
                int start = i;
                while (i < path.Length && path[i] != '.' && path[i] != '[')
                    i++;
                parts.Add(new PropertyPathPart(path.Substring(start, i - start)));
            }
            return parts.ToArray();
        }

        public static implicit operator PropertyPath(string path) => new PropertyPath(path);
        public bool Equals(PropertyPath other)
        {
            if (Length != other.Length)
                return false;
            for (int i = 0; i < Length; i++)
                if (!m_Parts[i].Equals(other.m_Parts[i]))
                    return false;
            return true;
        }
        public override bool Equals(object obj) => obj is PropertyPath p && Equals(p);
        public override int GetHashCode()
        {
            int h = 17;
            for (int i = 0; i < Length; i++)
                h = h * 31 + m_Parts[i].GetHashCode();
            return h;
        }
        public static bool operator ==(PropertyPath a, PropertyPath b) => a.Equals(b);
        public static bool operator !=(PropertyPath a, PropertyPath b) => !a.Equals(b);

        public override string ToString()
        {
            var sb = new StringBuilder();
            for (int i = 0; i < Length; i++)
            {
                if (i > 0 && m_Parts[i].IsName)
                    sb.Append('.');
                sb.Append(m_Parts[i].ToString());
            }
            return sb.ToString();
        }
    }

    public delegate TDestination TypeConverter<TSource, TDestination>(ref TSource value);

    // Values along property paths, through the members of C# objects.
    public static class PropertyContainer
    {
        public static TValue GetValue<TContainer, TValue>(TContainer container, in PropertyPath path) =>
            TryGetValue(ref container, path, out TValue value) ? value : throw new InvalidOperationException($"No property at '{path}'.");

        public static TValue GetValue<TContainer, TValue>(ref TContainer container, in PropertyPath path) => GetValue<TContainer, TValue>(container, path);

        public static bool TryGetValue<TContainer, TValue>(ref TContainer container, in PropertyPath path, out TValue value)
        {
            value = default;
            if (!Values.TryGet(container, path, out object o))
                return false;
            if (o is TValue t)
            {
                value = t;
                return true;
            }
            if (o == null)
                return !typeof(TValue).IsValueType;
            if (Values.TryConvert(o, typeof(TValue), out object converted) && converted is TValue c)
            {
                value = c;
                return true;
            }
            return false;
        }

        public static bool TryGetValue<TContainer, TValue>(TContainer container, in PropertyPath path, out TValue value) => TryGetValue(ref container, path, out value);

        public static void SetValue<TContainer, TValue>(ref TContainer container, in PropertyPath path, TValue value)
        {
            if (!TrySetValue(ref container, path, value))
                throw new InvalidOperationException($"No property at '{path}' takes the value.");
        }

        public static void SetValue<TContainer, TValue>(TContainer container, in PropertyPath path, TValue value) => SetValue(ref container, path, value);

        public static bool TrySetValue<TContainer, TValue>(ref TContainer container, in PropertyPath path, TValue value)
        {
            object boxed = container;
            if (!Values.TrySet(ref boxed, path, value))
                return false;
            container = (TContainer)boxed;
            return true;
        }

        public static bool IsPathValid<TContainer>(ref TContainer container, in PropertyPath path) => Values.TryGet(container, path, out _);
    }

    // The reflection the paths go through.
    internal static class Values
    {
        static readonly Dictionary<(Type, string), MemberInfo> s_Members = new Dictionary<(Type, string), MemberInfo>();

        internal static MemberInfo Member(Type type, string name)
        {
            if (s_Members.TryGetValue((type, name), out MemberInfo m))
                return m;
            const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.FlattenHierarchy;
            m = null;
            for (Type t = type; t != null && m == null; t = t.BaseType)
            {
                PropertyInfo p = t.GetProperty(name, flags | BindingFlags.DeclaredOnly);
                if (p != null && p.GetIndexParameters().Length == 0 && (p.GetMethod?.IsPublic == true || p.GetCustomAttribute<CreatePropertyAttribute>() != null) &&
                    p.GetCustomAttribute<DontCreatePropertyAttribute>() == null)
                    m = p;
                else
                {
                    FieldInfo f = t.GetField(name, flags | BindingFlags.DeclaredOnly);
                    if (f != null && (f.IsPublic || f.GetCustomAttribute<CreatePropertyAttribute>() != null || f.GetCustomAttribute<HelixEngine.SerializeField>() != null) &&
                        f.GetCustomAttribute<DontCreatePropertyAttribute>() == null)
                        m = f;
                }
            }
            s_Members[(type, name)] = m;
            return m;
        }

        internal static Type TypeOf(MemberInfo m) => m is PropertyInfo p ? p.PropertyType : ((FieldInfo)m).FieldType;

        static bool Step(object container, PropertyPathPart part, out object value)
        {
            value = null;
            if (container == null)
                return false;
            switch (part.Kind)
            {
            case PropertyPathPartKind.Name:
                MemberInfo m = Member(container.GetType(), part.Name);
                if (m == null)
                    return false;
                value = m is PropertyInfo p ? p.GetValue(container) : ((FieldInfo)m).GetValue(container);
                return true;
            case PropertyPathPartKind.Index:
                if (container is IList list && part.Index >= 0 && part.Index < list.Count)
                {
                    value = list[part.Index];
                    return true;
                }
                return false;
            default:
                if (container is IDictionary dict && dict.Contains(part.Key))
                {
                    value = dict[part.Key];
                    return true;
                }
                return false;
            }
        }

        internal static bool TryGet(object root, in PropertyPath path, out object value)
        {
            value = root;
            for (int i = 0; i < path.Length; i++)
                if (!Step(value, path[i], out value))
                    return false;
            return true;
        }

        static bool SetPart(ref object container, PropertyPathPart part, object value)
        {
            switch (part.Kind)
            {
            case PropertyPathPartKind.Name:
                MemberInfo m = Member(container.GetType(), part.Name);
                if (m == null)
                    return false;
                if (m is PropertyInfo p && p.CanWrite && p.GetCustomAttribute<CreatePropertyAttribute>()?.ReadOnly != true)
                {
                    if (!TryConvert(value, p.PropertyType, out object v))
                        return false;
                    p.SetValue(container, v);
                    return true;
                }
                if (m is FieldInfo f && !f.IsInitOnly)
                {
                    if (!TryConvert(value, f.FieldType, out object v))
                        return false;
                    f.SetValue(container, v);
                    return true;
                }
                return false;
            case PropertyPathPartKind.Index:
                if (container is IList list && part.Index >= 0 && part.Index < list.Count)
                {
                    list[part.Index] = value;
                    return true;
                }
                return false;
            default:
                if (container is IDictionary dict)
                {
                    dict[part.Key] = value;
                    return true;
                }
                return false;
            }
        }

        // Sets along the path; a struct on the way is set back into its owner.
        internal static bool TrySet(ref object root, in PropertyPath path, object value)
        {
            if (path.Length == 0 || root == null)
                return false;
            if (path.Length == 1)
                return SetPart(ref root, path[0], value);
            if (!Step(root, path[0], out object child) || child == null)
                return false;
            if (!TrySet(ref child, PropertyPath.SubPath(path, 1), value))
                return false;
            return child.GetType().IsValueType ? SetPart(ref root, path[0], child) : true;
        }

        internal static bool TryConvert(object value, Type type, out object result)
        {
            result = value;
            if (value == null)
                return !type.IsValueType;
            if (type.IsInstanceOfType(value))
                return true;
            if (HelixEngine.UI.ConverterGroups.TryConvertGlobal(value, type, out result))
                return true;
            try
            {
                if (type == typeof(string))
                {
                    result = Convert.ToString(value, CultureInfo.InvariantCulture);
                    return true;
                }
                if (type.IsEnum && value is string s)
                {
                    result = Enum.Parse(type, s, true);
                    return true;
                }
                if (type.IsEnum)
                {
                    result = Enum.ToObject(type, value);
                    return true;
                }
                if (value is IConvertible && typeof(IConvertible).IsAssignableFrom(type))
                {
                    result = Convert.ChangeType(value, type, CultureInfo.InvariantCulture);
                    return true;
                }
            }
            catch (Exception)
            {
            }
            result = null;
            return false;
        }
    }
}
