using System;
using System.Collections.Generic;
using System.Linq;

namespace HelixEngine.UI
{
    // ------------------------------------------------------------------------------------------- dropdown menus

    public class DropdownMenuEventInfo
    {
        public EventModifiers modifiers { get; }
        public Vector2 mousePosition { get; }
        public Vector2 localMousePosition { get; }
        public char character { get; }
        public KeyCode keyCode { get; }

        public DropdownMenuEventInfo(EventBase e)
        {
            if (e is IMouseEvent m)
            {
                modifiers = m.modifiers;
                mousePosition = m.mousePosition;
                localMousePosition = m.localMousePosition;
            }
            else if (e is IPointerEvent p)
            {
                modifiers = p.modifiers;
                mousePosition = new Vector2(p.position.x, p.position.y);
                localMousePosition = new Vector2(p.localPosition.x, p.localPosition.y);
            }
            else if (e is IKeyboardEvent k)
            {
                modifiers = k.modifiers;
                character = k.character;
                keyCode = k.keyCode;
            }
        }
    }

    public abstract class DropdownMenuItem { }

    public class DropdownMenuSeparator : DropdownMenuItem
    {
        public string subMenuPath { get; }
        public DropdownMenuSeparator(string subMenuPath) { this.subMenuPath = subMenuPath; }
    }

    public class DropdownMenuAction : DropdownMenuItem
    {
        [Flags]
        public enum Status
        {
            None = 0,
            Normal = 1,
            Disabled = 2,
            Checked = 4,
            Hidden = 8,
        }

        readonly Action<DropdownMenuAction> m_Action;
        readonly Func<DropdownMenuAction, Status> m_StatusCallback;

        public string name { get; }
        public string tooltip { get; set; }
        public Texture2D icon { get; }
        public Status status { get; private set; }
        public DropdownMenuEventInfo eventInfo { get; private set; }
        public object userData { get; }

        public DropdownMenuAction(string actionName, Action<DropdownMenuAction> actionCallback, Func<DropdownMenuAction, Status> actionStatusCallback,
                                  object userData = null)
            : this(actionName, actionCallback, actionStatusCallback, userData, null) { }

        public DropdownMenuAction(string actionName, Action<DropdownMenuAction> actionCallback, Func<DropdownMenuAction, Status> actionStatusCallback,
                                  object userData, Texture2D icon)
        {
            name = actionName;
            m_Action = actionCallback;
            m_StatusCallback = actionStatusCallback;
            this.userData = userData;
            this.icon = icon;
        }

        public static Status AlwaysEnabled(DropdownMenuAction a) => Status.Normal;
        public static Status AlwaysDisabled(DropdownMenuAction a) => Status.Disabled;

        public void UpdateActionStatus(DropdownMenuEventInfo eventInfo)
        {
            this.eventInfo = eventInfo;
            status = m_StatusCallback?.Invoke(this) ?? Status.Hidden;
        }

        public void Execute()
        {
            if (m_Action != null && (status & (Status.Disabled | Status.Hidden)) == 0)
                m_Action(this);
        }
    }

    // The items of a menu: actions (named with slashes for submenus) and separators.
    public class DropdownMenu
    {
        readonly List<DropdownMenuItem> m_Items = new List<DropdownMenuItem>();
        DropdownMenuEventInfo m_EventInfo;

        public bool allowDuplicateNames { get; set; }

        public List<DropdownMenuItem> MenuItems() => m_Items;

        public void AppendAction(string actionName, Action<DropdownMenuAction> action, Func<DropdownMenuAction, DropdownMenuAction.Status> actionStatusCallback,
                                 object userData = null) => InsertAction(m_Items.Count, actionName, action, actionStatusCallback, userData);

        public void AppendAction(string actionName, Action<DropdownMenuAction> action, DropdownMenuAction.Status status = DropdownMenuAction.Status.Normal) =>
            AppendAction(actionName, action, _ => status);

        public void InsertAction(int atIndex, string actionName, Action<DropdownMenuAction> action,
                                 Func<DropdownMenuAction, DropdownMenuAction.Status> actionStatusCallback, object userData = null)
        {
            if (!allowDuplicateNames && m_Items.OfType<DropdownMenuAction>().Any(a => a.name == actionName))
                return;
            var item = new DropdownMenuAction(actionName, action, actionStatusCallback, userData);
            if (m_EventInfo != null)
                item.UpdateActionStatus(m_EventInfo);
            m_Items.Insert(Math.Clamp(atIndex, 0, m_Items.Count), item);
        }

        public void InsertAction(int atIndex, string actionName, Action<DropdownMenuAction> action,
                                 DropdownMenuAction.Status status = DropdownMenuAction.Status.Normal) =>
            InsertAction(atIndex, actionName, action, _ => status);

        public void AppendSeparator(string subMenuPath = null)
        {
            if (m_Items.Count > 0 && !(m_Items[m_Items.Count - 1] is DropdownMenuSeparator))
                m_Items.Add(new DropdownMenuSeparator(subMenuPath ?? string.Empty));
        }

        public void InsertSeparator(string subMenuPath, int atIndex)
        {
            if (atIndex > 0 && atIndex <= m_Items.Count && !(m_Items[atIndex - 1] is DropdownMenuSeparator))
                m_Items.Insert(atIndex, new DropdownMenuSeparator(subMenuPath ?? string.Empty));
        }

        public void RemoveItemAt(int index) => m_Items.RemoveAt(index);
        public void ClearItems() => m_Items.Clear();

        // The statuses as of the event; separators at the ends and doubled ones go.
        public void PrepareForDisplay(EventBase e)
        {
            m_EventInfo = e != null ? new DropdownMenuEventInfo(e) : null;
            foreach (DropdownMenuAction a in m_Items.OfType<DropdownMenuAction>())
                a.UpdateActionStatus(m_EventInfo);
            for (int i = m_Items.Count - 1; i >= 0; i--)
            {
                bool hidden = m_Items[i] is DropdownMenuAction a && (a.status & DropdownMenuAction.Status.Hidden) != 0;
                if (hidden)
                    m_Items.RemoveAt(i);
            }
            while (m_Items.Count > 0 && m_Items[0] is DropdownMenuSeparator)
                m_Items.RemoveAt(0);
            while (m_Items.Count > 0 && m_Items[m_Items.Count - 1] is DropdownMenuSeparator)
                m_Items.RemoveAt(m_Items.Count - 1);
        }
    }

    public enum DropdownMenuSizeMode
    {
        Auto,
        Fixed,
        Content,
    }

    // A menu over the target's panel: its items in a box under (or at) a rectangle, checked ones marked, a press
    // outside or Escape closes it; the arrows and Enter pick by the keyboard.
    public class GenericDropdownMenu
    {
        public static readonly string hssClassName = "helix-base-dropdown";
        public static readonly string itemHssClassName = hssClassName + "__item";
        public static readonly string itemContentHssClassName = hssClassName + "__item-content";
        public static readonly string labelHssClassName = hssClassName + "__label";
        public static readonly string containerInnerHssClassName = hssClassName + "__container-inner";
        public static readonly string containerOuterHssClassName = hssClassName + "__container-outer";
        public static readonly string checkmarkHssClassName = hssClassName + "__checkmark";
        public static readonly string separatorHssClassName = hssClassName + "__separator";
        public static readonly string contentWidthHssClassName = hssClassName + "--content-width-menu";

        sealed class Item
        {
            public Element element;
            public Action action;
            public bool enabled;
        }

        readonly Element m_Root, m_Outer, m_Inner;
        readonly List<Item> m_Items = new List<Item>();
        int m_Highlighted = -1;
        Element m_Panel;

        public event Action onOpen;
        public event Action onClose;

        public GenericDropdownMenu()
        {
            m_Root = new Element();
            m_Root.AddToClassList(hssClassName);
            m_Root.style.position = Position.Absolute;
            m_Root.style.left = 0;
            m_Root.style.top = 0;
            m_Root.style.right = 0;
            m_Root.style.bottom = 0;
            m_Root.focusable = true;
            m_Outer = new Element();
            m_Outer.AddToClassList(containerOuterHssClassName);
            m_Outer.style.position = Position.Absolute;
            m_Inner = new Element();
            m_Inner.AddToClassList(containerInnerHssClassName);
            m_Outer.Add(m_Inner);
            m_Root.Add(m_Outer);
            m_Root.RegisterCallback<PointerDownEvent>(e =>
            {
                if (ReferenceEquals(e.target, m_Root))
                    Hide();
            });
            m_Root.RegisterCallback<KeyDownEvent>(OnKey);
            m_Root.RegisterCallback<NavigationMoveEvent>(e =>
            {
                Move(e.direction == NavigationMoveDirection.Up ? -1 : e.direction == NavigationMoveDirection.Down ? 1 : 0);
                e.StopPropagation();
            });
            m_Root.RegisterCallback<NavigationSubmitEvent>(e =>
            {
                Pick(m_Highlighted);
                e.StopPropagation();
            });
            m_Root.RegisterCallback<NavigationCancelEvent>(e =>
            {
                Hide();
                e.StopPropagation();
            });
        }

        public Element contentContainer => m_Inner;
        public Element targetElement { get; private set; }

        public void AddItem(string itemName, bool isChecked, Action action) => Add(itemName, isChecked, true, action);
        public void AddItem(string itemName, bool isChecked, Action<object> action, object data) => Add(itemName, isChecked, true, () => action?.Invoke(data));
        public void AddDisabledItem(string itemName, bool isChecked) => Add(itemName, isChecked, false, null);

        public void AddSeparator(string path)
        {
            var separator = new Element();
            separator.AddToClassList(separatorHssClassName);
            m_Inner.Add(separator);
        }

        void Add(string name, bool isChecked, bool enabled, Action action)
        {
            var element = new Element();
            element.AddToClassList(itemHssClassName);
            var check = new Element { pickingMode = PickingMode.Ignore };
            check.AddToClassList(checkmarkHssClassName);
            var label = new Label(name) { pickingMode = PickingMode.Ignore };
            label.AddToClassList(labelHssClassName);
            element.Add(check);
            element.Add(label);
            element.SetEnabled(enabled);
            if (isChecked)
                element.AddToClassList(itemHssClassName + "--checked");
            var item = new Item { element = element, action = action, enabled = enabled };
            int index = m_Items.Count;
            element.RegisterCallback<PointerUpEvent>(e =>
            {
                Pick(index);
                e.StopPropagation();
            });
            element.RegisterCallback<PointerEnterEvent>(_ => Highlight(index));
            m_Items.Add(item);
            m_Inner.Add(element);
        }

        // Shown under the rectangle (in the target's panel), or at its position when anchored.
        public void DropDown(Rect position, Element targetElement, bool anchored = false) =>
            DropDown(position, targetElement, anchored ? DropdownMenuSizeMode.Fixed : DropdownMenuSizeMode.Auto);

        public void DropDown(Rect position, Element targetElement, DropdownMenuSizeMode dropdownMenuSizeMode)
        {
            this.targetElement = targetElement;
            m_Panel = targetElement?.panel?.visualTree;
            if (m_Panel == null)
                return;
            m_Outer.style.left = position.x;
            m_Outer.style.top = position.y + position.height;
            if (dropdownMenuSizeMode == DropdownMenuSizeMode.Fixed)
                m_Outer.style.width = position.width;
            else if (dropdownMenuSizeMode == DropdownMenuSizeMode.Auto)
                m_Outer.style.minWidth = position.width;
            else
                m_Root.AddToClassList(contentWidthHssClassName);
            m_Panel.Add(m_Root);
            m_Root.Focus();
            onOpen?.Invoke();
        }

        void Hide()
        {
            if (m_Root.parent == null)
                return;
            m_Root.RemoveFromHierarchy();
            targetElement?.Focus();
            onClose?.Invoke();
        }

        void Pick(int index)
        {
            if (index < 0 || index >= m_Items.Count || !m_Items[index].enabled)
                return;
            Hide();
            m_Items[index].action?.Invoke();
        }

        void Highlight(int index)
        {
            for (int i = 0; i < m_Items.Count; i++)
                m_Items[i].element.EnableInClassList(itemHssClassName + "--highlighted", i == index);
            m_Highlighted = index;
        }

        void Move(int step)
        {
            if (step == 0 || m_Items.Count == 0)
                return;
            int i = m_Highlighted;
            for (int n = 0; n < m_Items.Count; n++)
            {
                i = (i + step + m_Items.Count) % m_Items.Count;
                if (m_Items[i].enabled)
                    break;
            }
            Highlight(i);
        }

        void OnKey(KeyDownEvent e)
        {
            if (e.keyCode == KeyCode.Escape)
                Hide();
            else if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter)
                Pick(m_Highlighted);
            else
                return;
            e.StopPropagation();
        }

        // A dropdown menu's actions in a GenericDropdownMenu (what the contextual menus show).
        internal static GenericDropdownMenu From(DropdownMenu menu)
        {
            var shown = new GenericDropdownMenu();
            foreach (DropdownMenuItem item in menu.MenuItems())
                if (item is DropdownMenuSeparator)
                    shown.AddSeparator("");
                else if (item is DropdownMenuAction a)
                {
                    bool on = (a.status & DropdownMenuAction.Status.Checked) != 0;
                    if ((a.status & DropdownMenuAction.Status.Disabled) != 0)
                        shown.AddDisabledItem(a.name, on);
                    else
                        shown.AddItem(a.name, on, a.Execute);
                }
            return shown;
        }
    }

    // ------------------------------------------------------------------------------------------- contextual menus

    // The event that builds a contextual menu: its target and the elements above it add their actions.
    public class ContextualMenuPopulateEvent : MouseEventBase<ContextualMenuPopulateEvent>
    {
        public DropdownMenu menu { get; private set; }
        public EventBase triggerEvent { get; private set; }
        ContextualMenuManager m_Manager;

        public static ContextualMenuPopulateEvent GetPooled(EventBase triggerEvent, DropdownMenu menu, IEventHandler target, ContextualMenuManager menuManager)
        {
            ContextualMenuPopulateEvent e = GetPooled();
            e.triggerEvent = triggerEvent;
            e.menu = menu;
            e.m_Manager = menuManager;
            e.target = target;
            if (triggerEvent is IPointerEvent p)
                e.Data.position = new Vector2(p.position.x, p.position.y);
            return e;
        }

        protected override void Init()
        {
            base.Init();
            menu = null;
            triggerEvent = null;
            m_Manager = null;
        }
    }

    // Shows the menu a contextual menu event built.
    public abstract class ContextualMenuManager
    {
        public abstract void DisplayMenuIfEventMatches(EventBase evt, IEventHandler eventHandler);

        public void DisplayMenu(EventBase triggerEvent, IEventHandler target)
        {
            var menu = new DropdownMenu();
            using (ContextualMenuPopulateEvent e = ContextualMenuPopulateEvent.GetPooled(triggerEvent, menu, target, this))
                (target as CallbackEventHandler)?.SendEvent(e);
            menu.PrepareForDisplay(triggerEvent);
            if (menu.MenuItems().Count > 0)
                DoDisplayMenu(menu, triggerEvent);
        }

        protected internal abstract void DoDisplayMenu(DropdownMenu menu, EventBase triggerEvent);

        internal Element target;
    }

    // The runtime panels' manager: the right button's release (or the menu key) shows a GenericDropdownMenu at the
    // pointer.
    internal sealed class RuntimeContextualMenuManager : ContextualMenuManager
    {
        internal static readonly RuntimeContextualMenuManager instance = new RuntimeContextualMenuManager();

        public override void DisplayMenuIfEventMatches(EventBase evt, IEventHandler eventHandler)
        {
            if (evt is PointerUpEvent up && up.button == 1)
                DisplayMenu(evt, eventHandler);
            else if (evt is KeyUpEvent key && (key.keyCode == KeyCode.Menu || (key.keyCode == KeyCode.F10 && key.shiftKey)))
                DisplayMenu(evt, eventHandler);
        }

        protected internal override void DoDisplayMenu(DropdownMenu menu, EventBase triggerEvent)
        {
            var element = triggerEvent?.target as Element ?? target;
            if (element == null)
                return;
            Vector2 at = triggerEvent is IPointerEvent p ? new Vector2(p.position.x, p.position.y) : element.worldBound.position;
            GenericDropdownMenu.From(menu).DropDown(new Rect(at.x, at.y, 0, 0), element, DropdownMenuSizeMode.Content);
        }
    }

    // Shows a contextual menu its builder fills when the target is right clicked.
    public class ContextualMenuManipulator : MouseManipulator
    {
        readonly Action<ContextualMenuPopulateEvent> m_MenuBuilder;

        public ContextualMenuManipulator(Action<ContextualMenuPopulateEvent> menuBuilder)
        {
            m_MenuBuilder = menuBuilder;
            activators.Add(new ManipulatorActivationFilter { button = MouseButton.RightMouse });
        }

        public static bool IsOSXContextualMenuPlatform() => false;

        protected override void RegisterCallbacksOnTarget()
        {
            target.RegisterCallback<PointerUpEvent>(OnUp);
            target.RegisterCallback<KeyUpEvent>(OnKeyUp);
            target.RegisterCallback<ContextualMenuPopulateEvent>(OnPopulate);
        }

        protected override void UnregisterCallbacksFromTarget()
        {
            target.UnregisterCallback<PointerUpEvent>(OnUp);
            target.UnregisterCallback<KeyUpEvent>(OnKeyUp);
            target.UnregisterCallback<ContextualMenuPopulateEvent>(OnPopulate);
        }

        void OnUp(PointerUpEvent e)
        {
            if (e.button != 1)
                return;
            RuntimeContextualMenuManager.instance.target = target;
            RuntimeContextualMenuManager.instance.DisplayMenuIfEventMatches(e, target);
            e.StopPropagation();
        }

        void OnKeyUp(KeyUpEvent e)
        {
            RuntimeContextualMenuManager.instance.target = target;
            RuntimeContextualMenuManager.instance.DisplayMenuIfEventMatches(e, target);
        }

        void OnPopulate(ContextualMenuPopulateEvent e) => m_MenuBuilder?.Invoke(e);
    }

    // ----------------------------------------------------------------------------------------- keyboard navigation

    public enum KeyboardNavigationOperation
    {
        None,
        SelectAll,
        Cancel,
        Submit,
        Previous,
        Next,
        MoveRight,
        MoveLeft,
        PageUp,
        PageDown,
        Begin,
        End,
    }

    // The keys of navigation as operations for its action.
    public class KeyboardNavigationManipulator : Manipulator
    {
        readonly Action<KeyboardNavigationOperation, EventBase> m_Action;

        public KeyboardNavigationManipulator(Action<KeyboardNavigationOperation, EventBase> action) { m_Action = action; }

        protected override void RegisterCallbacksOnTarget()
        {
            target.RegisterCallback<NavigationMoveEvent>(OnMove);
            target.RegisterCallback<NavigationSubmitEvent>(OnSubmit);
            target.RegisterCallback<NavigationCancelEvent>(OnCancel);
            target.RegisterCallback<KeyDownEvent>(OnKey);
        }

        protected override void UnregisterCallbacksFromTarget()
        {
            target.UnregisterCallback<NavigationMoveEvent>(OnMove);
            target.UnregisterCallback<NavigationSubmitEvent>(OnSubmit);
            target.UnregisterCallback<NavigationCancelEvent>(OnCancel);
            target.UnregisterCallback<KeyDownEvent>(OnKey);
        }

        void Invoke(KeyboardNavigationOperation op, EventBase e)
        {
            if (op == KeyboardNavigationOperation.None)
                return;
            m_Action?.Invoke(op, e);
        }

        void OnMove(NavigationMoveEvent e) =>
            Invoke(e.direction switch
            {
                NavigationMoveDirection.Up => KeyboardNavigationOperation.Previous,
                NavigationMoveDirection.Down => KeyboardNavigationOperation.Next,
                NavigationMoveDirection.Left => KeyboardNavigationOperation.MoveLeft,
                NavigationMoveDirection.Right => KeyboardNavigationOperation.MoveRight,
                _ => KeyboardNavigationOperation.None,
            }, e);

        void OnSubmit(NavigationSubmitEvent e) => Invoke(KeyboardNavigationOperation.Submit, e);
        void OnCancel(NavigationCancelEvent e) => Invoke(KeyboardNavigationOperation.Cancel, e);

        void OnKey(KeyDownEvent e) =>
            Invoke(e.keyCode switch
            {
                KeyCode.A when e.actionKey => KeyboardNavigationOperation.SelectAll,
                KeyCode.Home => KeyboardNavigationOperation.Begin,
                KeyCode.End => KeyboardNavigationOperation.End,
                KeyCode.PageUp => KeyboardNavigationOperation.PageUp,
                KeyCode.PageDown => KeyboardNavigationOperation.PageDown,
                _ => KeyboardNavigationOperation.None,
            }, e);
    }

    // ------------------------------------------------------------------------------------------------- tooltips

    // Sent to an element the pointer rests on when it has a tooltip, before the panel shows it (its rect where).
    public class TooltipEvent : EventBase<TooltipEvent>
    {
        string m_Tooltip;

        // The target's tooltip unless a callback sets another.
        public string tooltip
        {
            get => m_Tooltip ?? (target as Element)?.tooltip;
            set
            {
                m_Tooltip = value;
                if (target is Element e && value != null)
                    e.tooltip = value;
            }
        }

        public Rect rect
        {
            get => Data.newRect;
            set => Data.newRect = value;
        }

        protected override void Init()
        {
            base.Init();
            m_Tooltip = null;
        }
    }
}
