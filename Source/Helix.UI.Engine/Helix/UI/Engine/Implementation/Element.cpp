#include <Helix/UI/Engine/Element.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/UI/Styles/Properties.h>

#include <bit>
#include <cstddef>
#include <cstdint>

namespace helix::ui
{
    namespace
    {
        // The objects of a world's elements by slot, and the elements that count as being in a tree (panel roots).
        struct Registry
        {
            Array<Element*> bySlot;
            // The panel roots' event systems (null for the other slots).
            Array<EventSystem*> anchored;
            bool hooked = false;
        };

        struct Objects
        {
            HashMap<u64, CallbackEventHandler*> byId;
            u64 next = 0;
            HashMap<const ElementWorld*, Registry*> worlds;
        };

        Objects& O()
        {
            static Objects* o = new Objects();
            return *o;
        }

        Registry& RegistryOf(const ElementWorld& world)
        {
            Registry*& r = O().worlds[&world];
            if (!r)
                r = new Registry();
            return *r;
        }

        Element* SlotObject(Registry& r, ElementHandle e)
        {
            if (e.index >= r.bySlot.Size())
                return nullptr;
            Element* x = r.bySlot[e.index];
            return x && x->handle() == e ? x : nullptr;
        }

        bool Anchored(Registry& r, ElementHandle e) { return e.index < r.anchored.Size() && r.anchored[e.index]; }

        // ----------------------------------------------------------------------------- the C# side's hooks

        struct Managed
        {
            // An event reaching a C# callback of an element (by the element's id), and the scheduler's tick.
            void (*dispatch)(u64 elementId, UIEvent* e) = nullptr;
            void (*tick)(f64 time) = nullptr;
        };

        Managed& M()
        {
            static Managed m;
            return m;
        }

        void ManagedCallback(void* user, EventSystem&, UIEvent& e)
        {
            if (M().dispatch)
                M().dispatch(u64(std::uintptr_t(user)), &e);
        }

        Element* ElementById(u64 id)
        {
            CallbackEventHandler* h = CallbackEventHandler::Find(id);
            return h ? h->AsElement() : nullptr;
        }

        void SetDispatcher(void* function) { M().dispatch = std::bit_cast<void (*)(u64, UIEvent*)>(function); }
        void SetTick(void* function) { M().tick = std::bit_cast<void (*)(f64)>(function); }

        // The event's size and the places of its fields, which the C# side's copy of it checks.
        void EventLayout(i32* out)
        {
            const i32 values[] = {i32(sizeof(UIEvent)),
                                  i32(offsetof(UIEvent, target)),
                                  i32(offsetof(UIEvent, relatedTarget)),
                                  i32(offsetof(UIEvent, timestamp)),
                                  i32(offsetof(UIEvent, pointerId)),
                                  i32(offsetof(UIEvent, position)),
                                  i32(offsetof(UIEvent, keyCode)),
                                  i32(offsetof(UIEvent, character)),
                                  i32(offsetof(UIEvent, direction)),
                                  i32(offsetof(UIEvent, propagationStopped)),
                                  i32(offsetof(UIEvent, oldRect)),
                                  i32(offsetof(UIEvent, newRect))};
            for (usize i = 0; i < sizeof(values) / sizeof(values[0]); i++)
                out[i] = values[i];
        }

        // The id of an event type by its C# class's name (registered when the panel does not know it yet).
        i32 EventTypeIdOf(const char* name, i32 length, i32 tricklesDown, i32 bubbles)
        {
            StringView n(name, usize(length));
            EventTypeId id = FindEventType(n);
            if (id == kNoEventType)
                id = RegisterEventType(n, tricklesDown != 0, bubbles != 0, EventFamily::Other);
            return i32(id);
        }

        // A C# callback of an element for a type and phase (one native callback per element, type and phase).
        void RegisterManaged(u64 elementId, i32 type, i32 trickleDown, i32 add)
        {
            Element* e = ElementById(elementId);
            if (!e || !e->alive())
                return;
            void* user = reinterpret_cast<void*>(std::uintptr_t(elementId));
            TrickleDown phase = trickleDown ? TrickleDown::TrickleDown : TrickleDown::NoTrickleDown;
            if (add)
            {
                e->world().callbacks().Register(e->handle(), EventTypeId(type), &ManagedCallback, user, phase);
                // Its C# callbacks live in its C# object: kept while the element is in a tree.
                e->KeepScriptState();
            }
            else
                e->world().callbacks().Unregister(e->handle(), EventTypeId(type), &ManagedCallback, user, phase);
        }

        // An event a script sends to an element: through the element's panel (none outside of one).
        i32 SendEventTo(u64 elementId, UIEvent* event)
        {
            Element* e = ElementById(elementId);
            EventSystem* events = e ? e->events() : nullptr;
            if (!events || !event)
                return 0;
            UIEvent copy = *event;
            copy.target = e->handle();
            copy.currentTarget = {};
            copy.propagationStopped = copy.immediatePropagationStopped = false;
            events->SendEvent(copy);
            return 1;
        }

        void KeepWrapper(u64 elementId)
        {
            if (Element* e = ElementById(elementId))
                e->KeepScriptState();
        }

        struct ExportRegistrar
        {
            ExportRegistrar()
            {
                scripting::RegisterNativeExport("HelixUI.KeepWrapper", std::bit_cast<const void*>(&KeepWrapper));
                scripting::RegisterNativeExport("HelixUI.SetDispatcher", std::bit_cast<const void*>(&SetDispatcher));
                scripting::RegisterNativeExport("HelixUI.SetTick", std::bit_cast<const void*>(&SetTick));
                scripting::RegisterNativeExport("HelixUI.EventLayout", std::bit_cast<const void*>(&EventLayout));
                scripting::RegisterNativeExport("HelixUI.EventTypeId", std::bit_cast<const void*>(&EventTypeIdOf));
                scripting::RegisterNativeExport("HelixUI.RegisterCallback", std::bit_cast<const void*>(&RegisterManaged));
                scripting::RegisterNativeExport("HelixUI.SendEvent", std::bit_cast<const void*>(&SendEventTo));
            }
        };
        const ExportRegistrar registrar;

        // The event system of the panel whose root holds an element.
        EventSystem* EventsOf(ElementWorld& world, ElementHandle e)
        {
            ElementHandle top = e;
            for (ElementHandle p = world.tree().Parent(e); p.IsValid(); p = world.tree().Parent(p))
                top = p;
            Registry& r = RegistryOf(world);
            return world.tree().IsAlive(top) && top.index < r.anchored.Size() ? r.anchored[top.index] : nullptr;
        }
    }

    // ------------------------------------------------------------------------------------------ CallbackEventHandler

    CallbackEventHandler::CallbackEventHandler() : id_(++O().next) { O().byId.Set(id_, this); }

    CallbackEventHandler::~CallbackEventHandler()
    {
        O().byId.Remove(id_);
        scripting::ReleaseWrapper(wrapper_);
    }

    CallbackEventHandler* CallbackEventHandler::Find(u64 id)
    {
        CallbackEventHandler** found = O().byId.Find(id);
        return found ? *found : nullptr;
    }

    // ----------------------------------------------------------------------------------------------------- Element

    Element::Element() : world_(&ElementWorld::Main())
    {
        InstallHooks(*world_);
        handle_ = tree().Create(Intern("Element"));
        Registry& r = RegistryOf(*world_);
        if (r.bySlot.Size() <= handle_.index)
            r.bySlot.Resize(handle_.index + 1);
        r.bySlot[handle_.index] = this;
    }

    Element::Element(ElementWorld& world, ElementHandle e) : world_(&world), handle_(e)
    {
        InstallHooks(world);
        Registry& r = RegistryOf(world);
        if (r.bySlot.Size() <= e.index)
            r.bySlot.Resize(e.index + 1);
        r.bySlot[e.index] = this;
    }

    Element::~Element()
    {
        Registry& r = RegistryOf(*world_);
        if (handle_.index < r.bySlot.Size() && r.bySlot[handle_.index] == this)
            r.bySlot[handle_.index] = nullptr;
        scripting::ReleaseWrapper(retained_);
        // An element no tree holds goes with its object (the elements under it a script holds stay).
        ElementHandle e = handle_;
        handle_ = {};
        if (tree().IsAlive(e) && !tree().Parent(e).IsValid() && !Anchored(r, e))
        {
            world_->callbacks().Clear(e);
            tree().Destroy(e);
        }
    }

    scripting::TypeId Element::HelixType() const { return scripting::TypeOf<Element>(); }

    bool Element::alive() const { return world_ && tree().IsAlive(handle_); }

    Element* Element::Of(ElementWorld& world, ElementHandle e)
    {
        if (!world.tree().IsAlive(e))
            return nullptr;
        if (Element* x = SlotObject(RegistryOf(world), e))
            return x;
        return new Element(world, e);
    }

    Element* Element::FromHandle(u32 index, u32 generation) { return Of(ElementWorld::Main(), ElementHandle{index, generation}); }

    void Element::InstallHooks(ElementWorld& world)
    {
        Registry& r = RegistryOf(world);
        if (r.hooked)
            return;
        r.hooked = true;
        ElementTree::Hooks hooks;
        hooks.keep = &Element::Keep;
        hooks.destroyed = &Element::Destroyed;
        hooks.moved = &Element::Moved;
        hooks.user = &world;
        world.tree().SetHooks(hooks);
    }

    void Element::SetAnchored(ElementWorld& world, ElementHandle e, EventSystem* events)
    {
        Registry& r = RegistryOf(world);
        if (r.anchored.Size() <= e.index)
            r.anchored.Resize(e.index + 1);
        r.anchored[e.index] = events;
        if (Element* x = SlotObject(r, e))
            x->UpdateRetain();
    }

    bool Element::Keep(void* user, ElementHandle e)
    {
        Element* x = SlotObject(RegistryOf(*static_cast<ElementWorld*>(user)), e);
        return x && x->Wrapper() != 0;
    }

    void Element::Destroyed(void* user, ElementHandle e)
    {
        auto& world = *static_cast<ElementWorld*>(user);
        Registry& r = RegistryOf(world);
        if (e.index < r.anchored.Size())
            r.anchored[e.index] = nullptr;
        world.callbacks().Clear(e);
        // Only objects no script holds go with their element (Keep took the others out).
        if (Element* x = SlotObject(r, e))
        {
            r.bySlot[e.index] = nullptr;
            x->handle_ = {};
            delete x;
        }
    }

    void Element::Moved(void* user, ElementHandle e)
    {
        if (Element* x = SlotObject(RegistryOf(*static_cast<ElementWorld*>(user)), e))
            x->UpdateRetain();
    }

    void Element::UpdateRetain()
    {
        bool inTree = alive() && (tree().Parent(handle_).IsValid() || Anchored(RegistryOf(*world_), handle_));
        if (inTree && !retained_ && Wrapper() != 0)
            retained_ = scripting::RetainWrapper(Wrapper());
        else if (!inTree && retained_)
            scripting::ReleaseWrapper(retained_);
    }

    // --------------------------------------------------------------------------------------------- hierarchy

    String Element::name() const { return alive() ? String(AtomName(tree().Name(handle_))) : String(); }

    void Element::SetName(StringView name)
    {
        if (alive())
            tree().SetName(handle_, name.Empty() ? kNoAtom : Intern(name));
    }

    Element* Element::parent() const { return alive() ? Of(*world_, tree().Parent(handle_)) : nullptr; }

    Element* Element::contentContainer() { return this; }

    i32 Element::HierarchyChildCount() const { return alive() ? i32(tree().ChildCount(handle_)) : 0; }

    Element* Element::HierarchyElementAt(i32 index) const
    {
        if (!alive() || index < 0)
            return nullptr;
        return Of(*world_, tree().ChildAt(handle_, u32(index)));
    }

    void Element::HierarchyInsert(i32 index, Element* child)
    {
        if (!alive() || !child || !child->alive() || child->world_ != world_)
            return;
        tree().Insert(handle_, index < 0 ? ~0u : u32(index), child->handle_);
    }

    void Element::HierarchyRemove(Element* child)
    {
        if (alive() && child && child->alive() && tree().Parent(child->handle_) == handle_)
            tree().Detach(child->handle_);
    }

    i32 Element::childCount() const { return const_cast<Element*>(this)->contentContainer()->HierarchyChildCount(); }

    Element* Element::ElementAtChecked(i32 index) const
    {
        Element* c = const_cast<Element*>(this)->contentContainer();
        return index >= 0 && index < c->HierarchyChildCount() ? c->HierarchyElementAt(index) : nullptr;
    }

    i32 Element::IndexOf(Element* child) const
    {
        Element* c = const_cast<Element*>(this)->contentContainer();
        if (!child || !child->alive() || !c->alive() || tree().Parent(child->handle_) != c->handle_)
            return -1;
        return i32(tree().IndexInParent(child->handle_));
    }

    void Element::Add(Element* child)
    {
        if (child && child != this)
            contentContainer()->HierarchyInsert(-1, child);
    }

    bool Element::InsertChecked(i32 index, Element* child)
    {
        Element* c = contentContainer();
        if (!child || index < 0 || index > c->HierarchyChildCount())
            return false;
        c->HierarchyInsert(index, child);
        return true;
    }

    bool Element::RemoveChecked(Element* child)
    {
        Element* c = contentContainer();
        if (!child || !child->alive() || tree().Parent(child->handle_) != c->handle_)
            return false;
        c->HierarchyRemove(child);
        return true;
    }

    bool Element::RemoveAtChecked(i32 index)
    {
        Element* c = contentContainer();
        if (index < 0 || index >= c->HierarchyChildCount())
            return false;
        c->HierarchyRemove(c->HierarchyElementAt(index));
        return true;
    }

    void Element::Clear()
    {
        Element* c = contentContainer();
        if (!c->alive())
            return;
        while (c->tree().FirstChild(c->handle_).IsValid())
            c->tree().Detach(c->tree().FirstChild(c->handle_));
    }

    void Element::RemoveFromHierarchy()
    {
        if (alive())
            tree().Detach(handle_);
    }

    void Element::BringToFront()
    {
        ElementHandle p = alive() ? tree().Parent(handle_) : ElementHandle{};
        if (p.IsValid())
            tree().Append(p, handle_);
    }

    void Element::SendToBack()
    {
        ElementHandle p = alive() ? tree().Parent(handle_) : ElementHandle{};
        if (p.IsValid())
            tree().Insert(p, 0, handle_);
    }

    void Element::PlaceBehind(Element* sibling)
    {
        if (!alive() || !sibling || !sibling->alive() || sibling == this)
            return;
        ElementHandle p = tree().Parent(sibling->handle_);
        if (!p.IsValid())
            return;
        // Taken out first, so the sibling's place is counted without it.
        tree().Detach(handle_);
        tree().Insert(p, tree().IndexInParent(sibling->handle_), handle_);
    }

    void Element::PlaceInFront(Element* sibling)
    {
        if (!alive() || !sibling || !sibling->alive() || sibling == this)
            return;
        ElementHandle p = tree().Parent(sibling->handle_);
        if (!p.IsValid())
            return;
        tree().Detach(handle_);
        tree().Insert(p, tree().IndexInParent(sibling->handle_) + 1, handle_);
    }

    bool Element::Contains(Element* child) const
    {
        return alive() && child && child->alive() && (child == this || tree().IsAncestor(handle_, child->handle_));
    }

    Element* Element::FindCommonAncestor(Element* other) const
    {
        if (!alive() || !other || !other->alive())
            return nullptr;
        for (ElementHandle a = handle_; a.IsValid(); a = tree().Parent(a))
            if (a == other->handle_ || tree().IsAncestor(a, other->handle_))
                return Of(*world_, a);
        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------- classes

    void Element::AddToClassList(StringView className)
    {
        if (alive() && !className.Empty())
            tree().AddClass(handle_, Intern(className));
    }

    void Element::RemoveFromClassList(StringView className)
    {
        if (alive())
            tree().RemoveClass(handle_, Intern(className));
    }

    bool Element::ClassListContains(StringView className) const { return alive() && tree().HasClass(handle_, Intern(className)); }

    void Element::EnableInClassList(StringView className, bool enable)
    {
        if (enable)
            AddToClassList(className);
        else
            RemoveFromClassList(className);
    }

    void Element::ToggleInClassList(StringView className) { EnableInClassList(className, !ClassListContains(className)); }

    void Element::ClearClassList()
    {
        if (alive())
            tree().ClearClasses(handle_);
    }

    Array<String> Element::GetClasses() const
    {
        Array<String> out;
        if (alive())
            for (Atom a : tree().Classes(handle_))
                out.Push(String(AtomName(a)));
        return out;
    }

    // ------------------------------------------------------------------------------------- state and focus

    bool Element::enabledSelf() const { return alive() && tree().EnabledSelf(handle_); }
    bool Element::enabledInHierarchy() const { return alive() && tree().EnabledInHierarchy(handle_); }

    void Element::SetEnabled(bool value)
    {
        if (alive())
            tree().SetEnabled(handle_, value);
    }

    bool Element::visible() const { return alive() && world_->styles().Computed(handle_).visibility == Visibility::Visible; }

    void Element::SetVisible(bool value)
    {
        // As the reference: the inline style's visibility.
        if (alive())
            world_->styles().SetInlineProperty(handle_, FindProperty("visibility"), value ? "visible" : "hidden");
    }

    PickingMode Element::pickingMode() const { return alive() ? tree().Picking(handle_) : PickingMode::Position; }

    void Element::SetPickingMode(PickingMode value)
    {
        if (alive())
            tree().SetPicking(handle_, value);
    }

    bool Element::focusable() const { return alive() && tree().Focusable(handle_); }

    void Element::SetFocusable(bool value)
    {
        if (alive())
            tree().SetFocusable(handle_, value);
    }

    i32 Element::tabIndex() const { return alive() ? tree().TabIndex(handle_) : 0; }

    void Element::SetTabIndex(i32 value)
    {
        if (alive())
            tree().SetTabIndex(handle_, value);
    }

    bool Element::delegatesFocus() const { return alive() && tree().DelegatesFocus(handle_); }

    void Element::SetDelegatesFocus(bool value)
    {
        if (alive())
            tree().SetDelegatesFocus(handle_, value);
    }

    EventSystem* Element::events() const { return alive() ? EventsOf(*world_, handle_) : nullptr; }

    // The focus is a panel's: outside of one an element cannot take it.
    bool Element::canGrabFocus() const
    {
        EventSystem* e = events();
        return e && e->CanGrabFocus(handle_);
    }

    void Element::Focus()
    {
        if (EventSystem* e = events())
            e->Focus(handle_);
    }

    void Element::Blur()
    {
        if (EventSystem* e = events())
            e->Blur(handle_);
    }

    void Element::MarkDirtyRepaint()
    {
        // Every panel draws each frame; a style change marks what must be computed again.
        if (alive())
            tree().MarkStyleDirty(handle_);
    }

    void Element::TickScripts(f64 time)
    {
        if (M().tick)
            M().tick(time);
    }

    // ------------------------------------------------------------------------------- the panel, for C#

    bool Element::CapturePointerInternal(i32 pointerId)
    {
        EventSystem* e = events();
        if (!e)
            return false;
        e->CapturePointer(handle_, pointerId);
        return true;
    }

    void Element::ReleasePointerInternal(i32 pointerId)
    {
        if (EventSystem* e = events())
            e->ReleasePointer(handle_, pointerId);
    }

    bool Element::HasPointerCaptureInternal(i32 pointerId) const
    {
        EventSystem* e = events();
        return e && e->HasPointerCapture(handle_, pointerId);
    }

    Element* Element::CapturingElementInternal(i32 pointerId) const
    {
        EventSystem* e = events();
        return e ? Of(*world_, e->CapturingElement(pointerId)) : nullptr;
    }

    Element* Element::FocusedElementInternal() const
    {
        EventSystem* e = events();
        return e ? Of(*world_, e->Focused()) : nullptr;
    }

    Element* Element::PanelRootInternal() const
    {
        if (!events())
            return nullptr;
        ElementHandle top = handle_;
        for (ElementHandle p = tree().Parent(handle_); p.IsValid(); p = tree().Parent(p))
            top = p;
        return Of(*world_, top);
    }

    Element* Element::PickInternal(Vector2 point) const
    {
        EventSystem* e = events();
        return e ? Of(*world_, e->Pick(point)) : nullptr;
    }

    Array<Element*> Element::PickAllInternal(Vector2 point) const
    {
        Array<Element*> out;
        if (EventSystem* e = events())
        {
            Array<ElementHandle> all;
            e->PickAll(point, all);
            for (ElementHandle h : all)
                out.Push(Of(*world_, h));
        }
        return out;
    }

    // ------------------------------------------------------------------------------------ styles, for C#

    namespace
    {
        bool InlineOf(const ElementWorld& world, ElementHandle e, StringView property, Array<StyleValue>& out, bool& initial)
        {
            initial = false;
            return const_cast<ElementWorld&>(world).styles().InlineProperty(e, FindProperty(property), out, &initial);
        }

        String WordOf(PropertyId id, const StyleValue& v)
        {
            if (id == kNoProperty || v.kind != StyleValue::Kind::Keyword)
                return String();
            Span<const StringView> words = Property(id).keywords;
            return v.keyword < words.Size() ? String(words[v.keyword]) : String();
        }
    }

    bool Element::SetInlineStyleText(StringView property, StringView value)
    {
        return alive() && world_->styles().SetInlineProperty(handle_, FindProperty(property), value);
    }

    void Element::SetInlineStyleKeyword(StringView property, i32 keyword)
    {
        if (!alive())
            return;
        PropertyId id = FindProperty(property);
        if (keyword == 1)
            world_->styles().SetInlineInitial(handle_, id);
        else
            world_->styles().ClearInlineProperty(handle_, id);
    }

    i32 Element::InlineStyleKind(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial))
            return -2;
        if (initial || v.Empty())
            return -1;
        return i32(v[0].kind);
    }

    f32 Element::InlineStyleNumber(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || v.Empty())
            return 0;
        return v[0].kind == StyleValue::Kind::Length ? v[0].length.value : v[0].number;
    }

    i32 Element::InlineStyleUnit(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || v.Empty() || v[0].kind != StyleValue::Kind::Length)
            return 0;
        return i32(v[0].length.unit);
    }

    Color Element::InlineStyleColor(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || v.Empty())
            return Color(0, 0, 0, 0);
        return v[0].color;
    }

    String Element::InlineStyleWord(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || v.Empty())
            return String();
        return WordOf(FindProperty(property), v[0]);
    }

    String Element::InlineStyleResource(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || v.Empty() || v[0].kind != StyleValue::Kind::Resource)
            return String();
        return String(AtomName(v[0].resource));
    }

    f32 Element::ResolvedStyleNumber(StringView property) const
    {
        PropertyId id = FindProperty(property);
        if (!alive() || id == kNoProperty || !Property(id).get)
            return 0;
        StyleValue v = Property(id).get(world_->styles().Computed(handle_));
        if (v.kind != StyleValue::Kind::Length)
            return v.number;
        // Lengths as laid out: pixels (percentages and auto through the layout's result where it has one).
        return v.length.unit == Length::Unit::Pixel ? v.length.value : 0;
    }

    Color Element::ResolvedStyleColor(StringView property) const
    {
        PropertyId id = FindProperty(property);
        if (!alive() || id == kNoProperty || !Property(id).get)
            return Color(0, 0, 0, 0);
        return Property(id).get(world_->styles().Computed(handle_)).color;
    }

    String Element::ResolvedStyleWord(StringView property) const
    {
        PropertyId id = FindProperty(property);
        if (!alive() || id == kNoProperty || !Property(id).get)
            return String();
        return WordOf(id, Property(id).get(world_->styles().Computed(handle_)));
    }

    // -------------------------------------------------------------------------------------------- the boxes

    Rect Element::layout() const
    {
        if (!alive())
            return Rect();
        const LayoutBox& b = world_->layout().Box(handle_);
        return Rect(b.x, b.y, b.width, b.height);
    }

    Rect Element::worldBound() const { return alive() ? world_->layout().WorldRect(handle_) : Rect(); }

    Rect Element::localBound() const { return layout(); }

    Rect Element::paddingRect() const
    {
        if (!alive())
            return Rect();
        const LayoutBox& b = world_->layout().Box(handle_);
        return Rect(b.border[kLeft], b.border[kTop], b.width - b.border[kLeft] - b.border[kRight], b.height - b.border[kTop] - b.border[kBottom]);
    }

    Rect Element::contentRect() const
    {
        if (!alive())
            return Rect();
        const LayoutBox& b = world_->layout().Box(handle_);
        f32 x = b.border[kLeft] + b.padding[kLeft], y = b.border[kTop] + b.padding[kTop];
        return Rect(x, y, b.width - x - b.border[kRight] - b.padding[kRight], b.height - y - b.border[kBottom] - b.padding[kBottom]);
    }

    bool Element::ContainsPoint(Vector2 p) const
    {
        Rect r = layout();
        return p.x >= 0 && p.y >= 0 && p.x < r.width && p.y < r.height;
    }

    Vector2 Element::WorldToLocal(Vector2 p) const
    {
        Rect w = worldBound();
        return Vector2(p.x - w.x, p.y - w.y);
    }

    Vector2 Element::LocalToWorld(Vector2 p) const
    {
        Rect w = worldBound();
        return Vector2(p.x + w.x, p.y + w.y);
    }

    Vector2 Element::ChangeCoordinatesTo(Element* dest, Vector2 point) const
    {
        Vector2 world = LocalToWorld(point);
        return dest ? dest->WorldToLocal(world) : world;
    }
}

helix::ui::CallbackEventHandler* helix::scripting::ObjectHandles<helix::ui::CallbackEventHandler>::FromId(u64 id)
{
    return ui::CallbackEventHandler::Find(id);
}
