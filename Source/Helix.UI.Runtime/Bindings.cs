using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Helix.Scripting;
using HelixEngine.Properties;

namespace HelixEngine.UI
{
    public enum BindingMode
    {
        TwoWay,
        ToSource,
        ToTarget,
        ToTargetOnce,
    }

    public enum BindingUpdateTrigger
    {
        WhenDirty,
        OnSourceChanged,
        EveryUpdate,
    }

    public enum BindingStatus
    {
        Success,
        Failure,
        Pending,
    }

    public enum BindingLogLevel
    {
        None,
        Once,
        All,
    }

    public readonly struct BindingResult
    {
        public BindingStatus status { get; }
        public string message { get; }
        public BindingResult(BindingStatus status, string message = null)
        {
            this.status = status;
            this.message = message;
        }
    }

    // The property of an element a binding sets: a path from the element ("value", "style.width").
    public readonly struct BindingId : IEquatable<BindingId>
    {
        readonly PropertyPath m_Path;
        public static readonly BindingId Invalid = default;

        public BindingId(string path) { m_Path = new PropertyPath(path); }
        public BindingId(in PropertyPath path) { m_Path = path; }

        public static implicit operator PropertyPath(in BindingId id) => id.m_Path;
        public static implicit operator BindingId(in PropertyPath path) => new BindingId(path);
        public static implicit operator BindingId(string name) => new BindingId(name);
        public static implicit operator string(in BindingId id) => id.m_Path.ToString();
        public bool Equals(BindingId other) => m_Path.Equals(other.m_Path);
        public override bool Equals(object obj) => obj is BindingId b && Equals(b);
        public override int GetHashCode() => m_Path.GetHashCode();
        public static bool operator ==(in BindingId a, in BindingId b) => a.Equals(b);
        public static bool operator !=(in BindingId a, in BindingId b) => !a.Equals(b);
        public override string ToString() => m_Path.ToString();
    }

    public class BindablePropertyChangedEventArgs : EventArgs
    {
        public BindingId propertyName { get; }
        public BindablePropertyChangedEventArgs(in BindingId propertyName) { this.propertyName = propertyName; }
    }

    // A data source that says when its properties change (bindings WhenDirty update then).
    public interface INotifyBindablePropertyChanged
    {
        event EventHandler<BindablePropertyChangedEventArgs> propertyChanged;
    }

    // A data source that gives a number which changes when what bindings show of it changes.
    public interface IDataSourceViewHashProvider
    {
        long GetViewHashCode();
    }

    public interface IDataSourceProvider
    {
        object dataSource { get; }
        PropertyPath dataSourcePath { get; }
    }

    public readonly struct DataSourceContext
    {
        public object dataSource { get; }
        public PropertyPath dataSourcePath { get; }
        public DataSourceContext(object dataSource, in PropertyPath dataSourcePath)
        {
            this.dataSource = dataSource;
            this.dataSourcePath = dataSourcePath;
        }
    }

    public readonly struct BindingContext
    {
        public Element targetElement { get; }
        public BindingId bindingId { get; }
        public PropertyPath dataSourcePath { get; }
        public object dataSource { get; }
        internal BindingContext(Element target, in BindingId id, in PropertyPath path, object source)
        {
            targetElement = target;
            bindingId = id;
            dataSourcePath = path;
            dataSource = source;
        }
    }

    public readonly struct BindingInfo
    {
        public Element targetElement { get; }
        public BindingId bindingId { get; }
        public Binding binding { get; }
        internal BindingInfo(Element target, in BindingId id, Binding binding)
        {
            targetElement = target;
            bindingId = id;
            this.binding = binding;
        }
    }

    // A binding of an element's property: updated by the panels' binding pass as its trigger says.
    public abstract class Binding
    {
        static BindingLogLevel s_GlobalLogLevel = BindingLogLevel.All;

        public BindingUpdateTrigger updateTrigger { get; set; } = BindingUpdateTrigger.OnSourceChanged;
        public bool isDirty { get; private set; } = true;
        public void MarkDirty() => isDirty = true;
        internal void ClearDirty() => isDirty = false;

        public static void SetGlobalLogLevel(BindingLogLevel logLevel) => s_GlobalLogLevel = logLevel;
        public static BindingLogLevel GetGlobalLogLevel() => s_GlobalLogLevel;
        public static void SetPanelLogLevel(IPanel panel, BindingLogLevel logLevel) => s_GlobalLogLevel = logLevel;
        public static BindingLogLevel GetPanelLogLevel(IPanel panel) => s_GlobalLogLevel;
        public static void ResetPanelLogLevel(IPanel panel) { }
        public static void ResetGlobalLogLevel() => s_GlobalLogLevel = BindingLogLevel.All;

        protected internal virtual void OnActivated(in BindingActivationContext context) { }
        protected internal virtual void OnDeactivated(in BindingActivationContext context) { }
        protected internal virtual void OnDataSourceChanged(in DataSourceContextChanged context) { }
    }

    public readonly struct BindingActivationContext
    {
        public Element targetElement { get; }
        public BindingId bindingId { get; }
        internal BindingActivationContext(Element target, in BindingId id)
        {
            targetElement = target;
            bindingId = id;
        }
    }

    public readonly struct DataSourceContextChanged
    {
        public Element targetElement { get; }
        public BindingId bindingId { get; }
        public DataSourceContext previousContext { get; }
        public DataSourceContext newContext { get; }
        internal DataSourceContextChanged(Element target, in BindingId id, in DataSourceContext previous, in DataSourceContext next)
        {
            targetElement = target;
            bindingId = id;
            previousContext = previous;
            newContext = next;
        }
    }

    // A binding updated by its own code.
    public abstract class CustomBinding : Binding
    {
        protected CustomBinding() { updateTrigger = BindingUpdateTrigger.EveryUpdate; }
        protected internal virtual BindingResult Update(in BindingContext context) => new BindingResult(BindingStatus.Success);
    }

    // A binding between a property of the element and a value of its data source (along dataSourcePath), both ways
    // or one, through the converters of its groups.
    public class DataBinding : Binding, IDataSourceProvider
    {
        readonly ConverterGroup m_ToUi = new ConverterGroup("ui"), m_ToSource = new ConverterGroup("source");
        internal object lastSource, lastTarget;
        internal bool synced, once;
        internal long lastHash = long.MinValue;

        public object dataSource { get; set; }
        public Type dataSourceType { get; set; }
        public PropertyPath dataSourcePath { get; set; }
        public string dataSourcePathString
        {
            get => dataSourcePath.ToString();
            set => dataSourcePath = new PropertyPath(value);
        }
        public BindingMode bindingMode { get; set; } = BindingMode.TwoWay;
        public ConverterGroup sourceToUiConverters => m_ToUi;
        public ConverterGroup uiToSourceConverters => m_ToSource;

        public void ApplyConverterGroupToSource(string groupId)
        {
            if (ConverterGroups.TryGetConverterGroup(groupId, out ConverterGroup g))
                m_ToSource.AddGroup(g);
        }

        public void ApplyConverterGroupToUI(string groupId)
        {
            if (ConverterGroups.TryGetConverterGroup(groupId, out ConverterGroup g))
                m_ToUi.AddGroup(g);
        }

        protected internal virtual BindingResult UpdateUI<TValue>(in BindingContext context, ref TValue value) => new BindingResult(BindingStatus.Success);
        protected internal virtual BindingResult UpdateSource<TValue>(in BindingContext context, ref TValue value) => new BindingResult(BindingStatus.Success);
    }

    // Converters of values from one type to another, by group.
    public class ConverterGroup
    {
        readonly Dictionary<(Type, Type), Delegate> m_Converters = new Dictionary<(Type, Type), Delegate>();
        readonly List<ConverterGroup> m_Groups = new List<ConverterGroup>();

        public string id { get; }
        public string displayName { get; set; }
        public string description { get; set; }

        public ConverterGroup(string id, string displayName = null, string description = null)
        {
            this.id = id;
            this.displayName = displayName;
            this.description = description;
        }

        public void AddConverter<TSource, TDestination>(TypeConverter<TSource, TDestination> converter) => m_Converters[(typeof(TSource), typeof(TDestination))] = converter;
        internal void AddGroup(ConverterGroup group) => m_Groups.Add(group);

        public bool TryConvert<TSource, TDestination>(ref TSource source, out TDestination destination)
        {
            if (TryConvert(source, typeof(TDestination), out object result) && result is TDestination d)
            {
                destination = d;
                return true;
            }
            destination = default;
            return false;
        }

        public bool TrySetValue<TContainer, TValue>(ref TContainer container, in PropertyPath path, TValue value, out VisitReturnCode returnCode)
        {
            object boxed = container;
            object converted = value;
            if (Values.Member(container.GetType(), path[path.Length - 1].IsName ? path[path.Length - 1].Name : "") is MemberInfo m)
                TryConvert(value, Values.TypeOf(m), out converted);
            bool ok = Values.TrySet(ref boxed, path, converted);
            if (ok)
                container = (TContainer)boxed;
            returnCode = ok ? VisitReturnCode.Ok : VisitReturnCode.InvalidPath;
            return ok;
        }

        internal bool TryConvert(object value, Type to, out object result)
        {
            result = null;
            if (value == null)
                return false;
            for (Type from = value.GetType(); from != null; from = from.BaseType)
                if (m_Converters.TryGetValue((from, to), out Delegate d))
                {
                    result = Invoke(d, value);
                    return true;
                }
            foreach (ConverterGroup g in m_Groups)
                if (g.TryConvert(value, to, out result))
                    return true;
            return false;
        }

        internal static object Invoke(Delegate converter, object value)
        {
            // TypeConverter<TSource, TDestination>(ref TSource): the argument by reference.
            object[] args = { value };
            return converter.DynamicInvoke(args);
        }
    }

    public enum VisitReturnCode
    {
        Ok,
        NullContainer,
        InvalidContainerType,
        MissingPropertyBag,
        InvalidPath,
        InvalidCast,
        AccessViolation,
    }

    // The global converters and the named groups.
    public static class ConverterGroups
    {
        static readonly ConverterGroup s_Global = new ConverterGroup("__global");
        static readonly Dictionary<string, ConverterGroup> s_Groups = new Dictionary<string, ConverterGroup>();

        public static void RegisterGlobalConverter<TSource, TDestination>(TypeConverter<TSource, TDestination> converter) => s_Global.AddConverter(converter);
        public static void RegisterConverterGroup(ConverterGroup converterGroup) => s_Groups[converterGroup.id] = converterGroup;
        public static bool TryGetConverterGroup(string groupId, out ConverterGroup converterGroup) => s_Groups.TryGetValue(groupId ?? "", out converterGroup);

        public static bool TryConvert<TSource, TDestination>(ref TSource source, out TDestination destination) => s_Global.TryConvert(ref source, out destination);

        public static bool TrySetValueGlobal<TContainer, TValue>(ref TContainer container, in PropertyPath path, TValue value, out VisitReturnCode returnCode) =>
            s_Global.TrySetValue(ref container, path, value, out returnCode);

        internal static bool TryConvertGlobal(object value, Type to, out object result) => s_Global.TryConvert(value, to, out result);
    }

    public partial class Element : IDataSourceProvider
    {
        object m_DataSource;
        PropertyPath m_DataSourcePath;
        Type m_DataSourceType;
        Dictionary<BindingId, Binding> m_Bindings;

        // The object the element's bindings (and its descendants') read; null: its parent's.
        public object dataSource
        {
            get => m_DataSource;
            set
            {
                m_DataSource = value;
                DataBindings.SourcesChanged(this);
            }
        }

        public PropertyPath dataSourcePath
        {
            get => m_DataSourcePath;
            set
            {
                m_DataSourcePath = value;
                DataBindings.SourcesChanged(this);
            }
        }

        public string dataSourcePathString
        {
            get => m_DataSourcePath.ToString();
            set => dataSourcePath = new PropertyPath(value);
        }

        public Type dataSourceType
        {
            get => m_DataSourceType;
            set => m_DataSourceType = value;
        }

        public void SetBinding(BindingId bindingId, Binding binding)
        {
            if (binding == null)
            {
                ClearBinding(bindingId);
                return;
            }
            m_Bindings ??= new Dictionary<BindingId, Binding>();
            if (m_Bindings.TryGetValue(bindingId, out Binding old))
                old.OnDeactivated(new BindingActivationContext(this, bindingId));
            m_Bindings[bindingId] = binding;
            binding.MarkDirty();
            binding.OnActivated(new BindingActivationContext(this, bindingId));
            KeepForCallbacks();
            DataBindings.Register(this);
        }

        public Binding GetBinding(BindingId bindingId) => m_Bindings != null && m_Bindings.TryGetValue(bindingId, out Binding b) ? b : null;

        public bool TryGetBinding(BindingId bindingId, out Binding binding)
        {
            binding = GetBinding(bindingId);
            return binding != null;
        }

        public IEnumerable<BindingInfo> GetBindingInfos() =>
            m_Bindings == null ? Enumerable.Empty<BindingInfo>() : m_Bindings.Select(p => new BindingInfo(this, p.Key, p.Value)).ToArray();

        public bool HasBinding(BindingId bindingId) => m_Bindings != null && m_Bindings.ContainsKey(bindingId);

        public void ClearBinding(BindingId bindingId)
        {
            if (m_Bindings != null && m_Bindings.TryGetValue(bindingId, out Binding b))
            {
                b.OnDeactivated(new BindingActivationContext(this, bindingId));
                m_Bindings.Remove(bindingId);
            }
        }

        public void ClearBindings()
        {
            if (m_Bindings == null)
                return;
            foreach (BindingId id in m_Bindings.Keys.ToArray())
                ClearBinding(id);
        }

        internal IEnumerable<KeyValuePair<BindingId, Binding>> bindings => m_Bindings ?? (IEnumerable<KeyValuePair<BindingId, Binding>>)Array.Empty<KeyValuePair<BindingId, Binding>>();

        // The data source and path of the element: its own, else its ancestors' (their paths joined).
        public DataSourceContext GetHierarchicalDataSourceContext()
        {
            object source = null;
            var paths = new List<PropertyPath>();
            for (Element e = this; e != null; e = e.parent)
            {
                if (!e.m_DataSourcePath.IsEmpty)
                    paths.Add(e.m_DataSourcePath);
                if (e.m_DataSource != null)
                {
                    source = e.m_DataSource;
                    break;
                }
            }
            var path = new PropertyPath();
            for (int i = paths.Count - 1; i >= 0; i--)
                path = PropertyPath.Combine(path, paths[i]);
            return new DataSourceContext(source, path);
        }

        public DataSourceContext GetDataSourceContext(BindingId bindingId)
        {
            DataSourceContext h = GetHierarchicalDataSourceContext();
            if (GetBinding(bindingId) is DataBinding d)
            {
                object source = d.dataSource ?? h.dataSource;
                PropertyPath path = d.dataSource != null ? d.dataSourcePath : PropertyPath.Combine(h.dataSourcePath, d.dataSourcePath);
                return new DataSourceContext(source, path);
            }
            return h;
        }

        public bool TryGetDataSourceContext(BindingId bindingId, out DataSourceContext context)
        {
            context = GetDataSourceContext(bindingId);
            return context.dataSource != null;
        }
    }

    // The bindings' pass, once a frame: each bound element in a panel updates its bindings as their triggers and
    // modes say (a changed value of the source to the element, a changed value of the element to the source).
    internal static unsafe class DataBindings
    {
        static readonly List<WeakReference<Element>> s_Bound = new List<WeakReference<Element>>();
        static readonly HashSet<Element> s_Known = new HashSet<Element>();
        static readonly HashSet<object> s_DirtySources = new HashSet<object>(ReferenceEqualityComparer.Instance);
        static delegate* unmanaged[Cdecl]<void*, void> s_TakeMarkup;
        static bool s_Hooked;

        internal static void Register(Element e)
        {
            if (s_Known.Add(e))
                s_Bound.Add(new WeakReference<Element>(e));
        }

        internal static void SourcesChanged(Element e)
        {
            foreach (KeyValuePair<BindingId, Binding> b in e.bindings)
                b.Value.MarkDirty();
        }

        static void OnSourceChanged(object sender, BindablePropertyChangedEventArgs args)
        {
            if (sender != null)
                s_DirtySources.Add(sender);
        }

        internal static void Tick()
        {
            TakeMarkupBindings();
            for (int i = s_Bound.Count - 1; i >= 0; i--)
            {
                if (!s_Bound[i].TryGetTarget(out Element e))
                {
                    if (s_Bound[i].TryGetTarget(out Element gone))
                        s_Known.Remove(gone);
                    s_Bound.RemoveAt(i);
                    continue;
                }
                if (e.panel == null)
                    continue;
                foreach (KeyValuePair<BindingId, Binding> pair in e.bindings.ToArray())
                    Update(e, pair.Key, pair.Value);
            }
            s_DirtySources.Clear();
        }

        static readonly HashSet<object> s_Listened = new HashSet<object>(ReferenceEqualityComparer.Instance);

        static void Update(Element e, BindingId id, Binding binding)
        {
            try
            {
                if (binding is CustomBinding custom)
                {
                    if (custom.updateTrigger == BindingUpdateTrigger.WhenDirty && !custom.isDirty)
                        return;
                    DataSourceContext ctx = e.GetDataSourceContext(id);
                    custom.Update(new BindingContext(e, id, ctx.dataSourcePath, ctx.dataSource));
                    custom.ClearDirty();
                    return;
                }
                if (binding is not DataBinding data)
                    return;
                DataSourceContext context = e.GetDataSourceContext(id);
                object source = context.dataSource;
                if (source == null)
                    return;
                if (source is INotifyBindablePropertyChanged notify && s_Listened.Add(source))
                    notify.propertyChanged += OnSourceChanged;
                var bindingContext = new BindingContext(e, id, context.dataSourcePath, source);
                // The source changed (by its notification, its view hash, or a new value).
                long hash = source is IDataSourceViewHashProvider h ? h.GetViewHashCode() : long.MinValue;
                bool sourceDirty = data.isDirty || s_DirtySources.Contains(source) || (hash != long.MinValue && hash != data.lastHash);
                data.lastHash = hash;
                Values.TryGet(source, context.dataSourcePath, out object sourceValue);
                Values.TryGet(e, id, out object targetValue);
                bool sourceMoved = !Equals(sourceValue, data.lastSource);
                bool targetMoved = data.synced && !Equals(targetValue, data.lastTarget);
                BindingMode mode = data.bindingMode;
                bool toTarget = mode == BindingMode.ToTarget || mode == BindingMode.TwoWay || (mode == BindingMode.ToTargetOnce && !data.once);
                bool toSource = mode == BindingMode.ToSource || mode == BindingMode.TwoWay;
                bool pushTarget = toTarget && (data.updateTrigger == BindingUpdateTrigger.EveryUpdate || sourceDirty || sourceMoved || !data.synced);
                if (data.updateTrigger == BindingUpdateTrigger.WhenDirty && !sourceDirty && data.synced)
                    pushTarget = false;
                if (toSource && targetMoved)
                {
                    // The element changed: its value to the source.
                    object v = targetValue;
                    data.UpdateSource(bindingContext, ref v);
                    if (data.uiToSourceConverters.TryConvert(v, SourceType(source, context.dataSourcePath) ?? typeof(object), out object converted))
                        v = converted;
                    object boxed = source;
                    if (Values.TrySet(ref boxed, context.dataSourcePath, v))
                    {
                        Values.TryGet(source, context.dataSourcePath, out sourceValue);
                        data.lastSource = sourceValue;
                    }
                    data.lastTarget = targetValue;
                }
                else if (pushTarget)
                {
                    object v = sourceValue;
                    data.UpdateUI(bindingContext, ref v);
                    Type targetType = TargetType(e, id);
                    if (targetType != null && v != null && !targetType.IsInstanceOfType(v) && data.sourceToUiConverters.TryConvert(v, targetType, out object converted))
                        v = converted;
                    object boxed = e;
                    if (!Equals(v, targetValue))
                        Values.TrySet(ref boxed, id, v);
                    Values.TryGet(e, id, out targetValue);
                    data.lastSource = sourceValue;
                    data.lastTarget = targetValue;
                    data.once = true;
                }
                data.synced = true;
                data.ClearDirty();
            }
            catch (Exception ex)
            {
                if (Binding.GetGlobalLogLevel() != BindingLogLevel.None)
                    Debug.LogException(ex);
            }
        }

        static Type SourceType(object source, in PropertyPath path)
        {
            if (path.IsEmpty)
                return null;
            PropertyPath owner = PropertyPath.Pop(path);
            if (!Values.TryGet(source, owner, out object o) || o == null || !path[path.Length - 1].IsName)
                return null;
            MemberInfo m = Values.Member(o.GetType(), path[path.Length - 1].Name);
            return m != null ? Values.TypeOf(m) : null;
        }

        static Type TargetType(Element e, in BindingId id) => SourceType(e, id);

        // ------------------------------------------------------------------------------------- the markup's

        static void TakeMarkupBindings()
        {
            if (!s_Hooked)
            {
                s_Hooked = true;
                s_TakeMarkup = (delegate* unmanaged[Cdecl]<void*, void>)Host.FindExport("HelixUI.TakeMarkupBindings");
            }
            if (s_TakeMarkup == null)
                return;
            delegate* unmanaged[Cdecl]<uint, uint, byte*, int, void> take = &TakeOne;
            s_TakeMarkup(take);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static void TakeOne(uint index, uint generation, byte* packed, int length)
        {
            try
            {
                Element e = Element.FromHandle(index, generation);
                if (e == null)
                    return;
                string[] parts = Encoding.UTF8.GetString(packed, length).Split('\0');
                string type = parts[0];
                var attributes = new Dictionary<string, string>();
                for (int i = 1; i + 1 < parts.Length; i += 2)
                    attributes[parts[i]] = parts[i + 1];
                if (type == "#data-source")
                {
                    if (attributes.TryGetValue("data-source-path", out string path))
                        e.dataSourcePathString = path;
                    if (attributes.TryGetValue("data-source-type", out string typeName))
                        e.dataSourceType = FindType(typeName);
                    return;
                }
                Binding binding = MakeBinding(type, attributes, out string property);
                if (binding != null && property != null)
                    e.SetBinding(property, binding);
            }
            catch (Exception ex)
            {
                Debug.LogException(ex);
            }
        }

        static Type FindType(string name)
        {
            Type t = Type.GetType(name);
            if (t != null)
                return t;
            foreach (Assembly a in AppDomain.CurrentDomain.GetAssemblies())
                if ((t = a.GetType(name.Split(',')[0].Trim())) != null)
                    return t;
            return null;
        }

        // A binding of the markup: its class (DataBinding or a class of the game's with a parameterless
        // constructor) and its attributes (property, data-source-path, binding-mode, update-trigger, the
        // converter groups source-to-ui-converters and ui-to-source-converters, or the class's own members).
        static Binding MakeBinding(string type, Dictionary<string, string> attributes, out string property)
        {
            attributes.TryGetValue("property", out property);
            Type t = type == "DataBinding" ? typeof(DataBinding) : FindType(type) ?? AppDomain.CurrentDomain.GetAssemblies()
                .SelectMany(a =>
                {
                    try
                    {
                        return a.GetTypes();
                    }
                    catch (ReflectionTypeLoadException ex)
                    {
                        return ex.Types.Where(x => x != null);
                    }
                })
                .FirstOrDefault(x => x.Name == type && typeof(Binding).IsAssignableFrom(x));
            if (t == null || !typeof(Binding).IsAssignableFrom(t) || t.IsAbstract)
            {
                Debug.LogError($"<{type}> is no binding class");
                return null;
            }
            var binding = (Binding)Activator.CreateInstance(t, true);
            foreach (KeyValuePair<string, string> a in attributes)
            {
                switch (a.Key)
                {
                case "property": break;
                case "data-source-path" when binding is DataBinding d: d.dataSourcePathString = a.Value; break;
                case "binding-mode" when binding is DataBinding d: d.bindingMode = (BindingMode)Enum.Parse(typeof(BindingMode), a.Value.Replace("-", ""), true); break;
                case "update-trigger": binding.updateTrigger = (BindingUpdateTrigger)Enum.Parse(typeof(BindingUpdateTrigger), a.Value.Replace("-", ""), true); break;
                case "source-to-ui-converters" when binding is DataBinding d:
                    foreach (string g in a.Value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                        d.ApplyConverterGroupToUI(g);
                    break;
                case "ui-to-source-converters" when binding is DataBinding d:
                    foreach (string g in a.Value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                        d.ApplyConverterGroupToSource(g);
                    break;
                default:
                    // A member of the binding's own class by its name (kebab or not).
                    string name = a.Key.Replace("-", "");
                    PropertyInfo p = t.GetProperties().FirstOrDefault(x => string.Equals(x.Name, name, StringComparison.OrdinalIgnoreCase) && x.CanWrite);
                    if (p != null && Values.TryConvert(a.Value, p.PropertyType, out object v))
                        p.SetValue(binding, v);
                    break;
                }
            }
            return binding;
        }
    }
}
