#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/TemplateContainer.h>
#include <Helix/UI/Engine/UIAssets.h>
#include <Helix/UI/Styles/Transforms.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Scripting/Scripts/NativeHandleClass.h>
#include <Helix/UI/Styles/Properties.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>

namespace helix::ui
{
    namespace
    {
        // The objects of a world's elements by slot, and the elements that count as being in a tree (panel roots).
        struct Registry
        {
            Array<Element*> bySlot;
            // The panels of the panel roots (null for the other slots).
            Array<Panel*> anchored;
            // The generation of each slot whose element took its type's behavior.
            Array<u32> attached;
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

        HashMap<Atom, Element::ObjectMaker>& Makers()
        {
            static auto* m = new HashMap<Atom, Element::ObjectMaker>();
            return *m;
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

        // ------------------------------------------------------------------------- element types of script classes

        // A type of a C# class (managed: its index on the C# side) or of a C++ class of a game module (native): its
        // markup name and its class's name (the type of its elements, what type selectors match), the markup
        // attributes it reads and the type it derives from (whose attributes, painting and behavior it keeps).
        struct ScriptType
        {
            Atom name = kNoAtom;
            Atom className = kNoAtom;
            i32 managed = -1;
            const scripting::NativeHandleClass* native = nullptr;
            Array<Atom> attributes;
            // C++: the field of each attribute.
            Array<const scripting::NativeHandleField*> fields;
            Atom base = kNoAtom;
        };

        // The C++ part of an element: the object of its type's class, made over the element's object.
        struct Part
        {
            u32 generation = ~0u;
            void* object = nullptr;
            const scripting::NativeHandleClass* c = nullptr;
        };

        struct Types
        {
            HashMap<Atom, ScriptType*> byName;
            // The C# side makes a C# class's object (its index), given its attributes (names and values, each ended by
            // a zero), and answers its element (index << 32 | generation; 0 when it could not); and it names the C#
            // class of an object (by its wrapper) into a buffer, answering the length.
            u64 (*factory)(i32 managed, const char* attributes, i32 length) = nullptr;
            i32 (*namer)(u64 wrapper, char* buffer, i32 capacity) = nullptr;
            Array<Part> parts;
        };

        Types& T()
        {
            static Types* t = new Types();
            return *t;
        }

        // A member's name as an attribute: myValue is my-value.
        String Kebab(StringView name)
        {
            String out;
            for (usize i = 0; i < name.Size(); i++)
            {
                char c = name[i];
                bool upper = c >= 'A' && c <= 'Z';
                if (upper && i > 0 && ((name[i - 1] >= 'a' && name[i - 1] <= 'z') || (name[i - 1] >= '0' && name[i - 1] <= '9')))
                    out.Append('-');
                out.Append(upper ? char(c - 'A' + 'a') : c);
            }
            return out;
        }

        ScriptType* ScriptTypeOf(Atom name)
        {
            ScriptType** found = T().byName.Find(name);
            return found ? *found : nullptr;
        }

        // The index of an attribute in the type or its script bases (-1: none of them reads it); the type that does.
        i32 FindAttribute(ScriptType* t, Atom name, ScriptType*& owner)
        {
            for (u32 depth = 0; t && depth < 32; depth++, t = ScriptTypeOf(t->base))
                for (u32 i = 0; i < t->attributes.Size(); i++)
                    if (t->attributes[i] == name)
                    {
                        owner = t;
                        return i32(i);
                    }
            return -1;
        }

        // The type below the script types of a chain (Element, Label, ...): its handlers read what the classes do not.
        const ElementType* NativeBase(ScriptType* t)
        {
            for (u32 depth = 0; t && depth < 32; depth++)
            {
                ScriptType* next = ScriptTypeOf(t->base);
                if (!next)
                    return FindElementType(t->base);
                t = next;
            }
            return nullptr;
        }

        bool NativeAttribute(ScriptType* t, ElementTree& tree, ElementHandle e, const HxmlAttribute& a)
        {
            const ElementType* base = NativeBase(t);
            return base && base->attributes && base->attributes(tree, e, AtomName(a.name), a.value.View());
        }

        // Where the markup's children go: the content container of the element's object (its class may name another).
        ElementHandle ObjectContent(ElementTree& tree, ElementHandle e)
        {
            ElementWorld& world = ElementWorld::Main();
            if (&tree != &world.tree())
                return e;
            Element* x = Element::Of(world, e);
            Element* c = x ? x->contentContainer() : nullptr;
            return c && c->alive() ? c->handle() : e;
        }

        ElementHandle CreateManaged(ElementTree& tree, Atom type, Span<const HxmlAttribute> attributes, Array<Atom>& unknown)
        {
            ScriptType* t = ScriptTypeOf(type);
            if (!t || !T().factory || &tree != &ElementWorld::Main().tree())
                return {};
            // What the classes read goes to the C# object; the rest to the type below them, once the element is there.
            String packed;
            Array<const HxmlAttribute*> rest;
            for (const HxmlAttribute& a : attributes)
            {
                ScriptType* owner = nullptr;
                if (FindAttribute(t, a.name, owner) >= 0)
                {
                    packed.Append(AtomName(a.name));
                    packed.Append('\0');
                    packed.Append(a.value.View());
                    packed.Append('\0');
                }
                else
                    rest.Push(&a);
            }
            u64 made = T().factory(t->managed, packed.Data(), i32(packed.Size()));
            ElementHandle e{u32(made >> 32), u32(made)};
            if (!made || !tree.IsAlive(e))
                return {};
            for (const HxmlAttribute* a : rest)
                if (!NativeAttribute(t, tree, e, *a))
                    unknown.Push(a->name);
            return e;
        }

        // A field of a C++ class from the markup's text, by its kind.
        bool SetField(void* object, const scripting::NativeHandleField& f, StringView text)
        {
            using K = scripting::NativeValueKind;
            StringView t = text.Trimmed();
            switch (f.kind)
            {
            case K::Bool:
            {
                if (t != StringView("true") && t != StringView("false"))
                    return false;
                bool v = t == StringView("true");
                f.set(object, &v);
                return true;
            }
            case K::I32:
            case K::I64:
            {
                i64 v = 0;
                bool negative = !t.Empty() && t[0] == '-';
                usize i = negative || (!t.Empty() && t[0] == '+') ? 1 : 0;
                if (i >= t.Size())
                    return false;
                for (; i < t.Size(); i++)
                {
                    if (t[i] < '0' || t[i] > '9')
                        return false;
                    v = v * 10 + (t[i] - '0');
                }
                v = negative ? -v : v;
                if (f.kind == K::I32)
                {
                    i32 n = i32(v);
                    f.set(object, &n);
                }
                else
                    f.set(object, &v);
                return true;
            }
            case K::F32:
            case K::F64:
            {
                // Numbers as HSS reads them (one reader for the markup of both languages).
                Array<StyleValue> values;
                String error;
                if (!ParseValue(FindProperty("opacity"), t, values, error) || values.Empty())
                    return false;
                if (f.kind == K::F32)
                    f.set(object, &values[0].number);
                else
                {
                    f64 d = values[0].number;
                    f.set(object, &d);
                }
                return true;
            }
            case K::String:
            {
                String v(text);
                f.set(object, &v);
                return true;
            }
            case K::Color:
            {
                Array<StyleValue> values;
                String error;
                if (!ParseValue(FindProperty("color"), t, values, error) || values.Empty())
                    return false;
                f.set(object, &values[0].color);
                return true;
            }
            }
            return false;
        }

        void DestroyPart(ElementHandle e)
        {
            Types& s = T();
            if (e.index >= s.parts.Size() || s.parts[e.index].generation != e.generation || !s.parts[e.index].object)
                return;
            Part p = s.parts[e.index];
            s.parts[e.index] = Part();
            p.c->destroy(p.object);
            ::operator delete(p.object, std::align_val_t(p.c->alignment));
        }

        ElementHandle CreateNative(ElementTree& tree, Atom type, Span<const HxmlAttribute> attributes, Array<Atom>& unknown)
        {
            ScriptType* t = ScriptTypeOf(type);
            ElementWorld& world = ElementWorld::Main();
            if (!t || !t->native || &tree != &world.tree())
                return {};
            // An element of the class's type; its C++ object made over the element's object (its handle refers to it in
            // its constructor).
            ElementHandle e = tree.Create(t->className);
            Element* x = Element::Of(world, e);
            const scripting::NativeHandleClass& c = *t->native;
            void* memory = ::operator new(c.size, std::align_val_t(c.alignment));
            void* object = c.construct(memory, x->Id());
            Types& s = T();
            if (s.parts.Size() <= e.index)
                s.parts.Resize(e.index + 1);
            s.parts[e.index] = Part{e.generation, object, &c};
            for (const HxmlAttribute& a : attributes)
            {
                ScriptType* owner = nullptr;
                i32 i = FindAttribute(t, a.name, owner);
                if (i >= 0 && owner->native)
                {
                    if (!SetField(object, *owner->fields[u32(i)], a.value.View()))
                        unknown.Push(a.name);
                }
                else if (!NativeAttribute(t, tree, e, a))
                    unknown.Push(a.name);
            }
            return e;
        }

        void UnregisterScriptType(ScriptType* t)
        {
            Types& s = T();
            for (Atom name : {t->name, t->className})
            {
                s.byName.Remove(name);
                UnregisterElementType(name);
            }
            delete t;
        }

        // Registers a type of a script class below a base type (its painting and behavior kept), by its markup name and
        // its class's name.
        void RegisterScriptType(ScriptType* t, CreateHandler create)
        {
            for (Atom name : {t->name, t->className})
                if (ScriptType* old = ScriptTypeOf(name))
                    UnregisterScriptType(old);
            ElementType type;
            if (const ElementType* base = NativeBase(t))
            {
                type.attributes = nullptr;
                type.paint = base->paint;
                type.attach = base->attach;
            }
            type.create = create;
            type.content = &ObjectContent;
            for (Atom name : {t->name, t->className})
            {
                T().byName.Set(name, t);
                type.name = name;
                RegisterElementType(type);
            }
        }

        // The C# side registers its classes with HxmlElement: their markup name and class name, index, attributes (each
        // ended by a zero) and the type they derive from.
        void RegisterManagedClass(const char* name, i32 nameLength, const char* className, i32 classLength, i32 managed, const char* attributes,
                                  i32 attributesLength, const char* base, i32 baseLength)
        {
            auto* t = new ScriptType();
            t->name = Intern(StringView(name, usize(nameLength)));
            t->className = Intern(StringView(className, usize(classLength)));
            t->managed = managed;
            t->base = Intern(baseLength > 0 ? StringView(base, usize(baseLength)) : StringView("Element"));
            StringView list(attributes, usize(attributesLength));
            usize at = 0;
            while (at < list.Size())
            {
                usize end = list.Find('\0', at);
                if (end == StringView::npos)
                    end = list.Size();
                if (end > at)
                    t->attributes.Push(Intern(list.Substr(at, end - at)));
                at = end + 1;
            }
            RegisterScriptType(t, &CreateManaged);
        }

        void SetElementFactory(void* factory, void* namer)
        {
            T().factory = std::bit_cast<u64 (*)(i32, const char*, i32)>(factory);
            T().namer = std::bit_cast<i32 (*)(u64, char*, i32)>(namer);
        }

        // C++ classes of game modules deriving from the script API's Element (HX_CLASS(HxmlElement)).
        void OnNativeClass(const scripting::NativeHandleClass& c, bool added, void*)
        {
            const scripting::NativeTag* tag = c.Tag("HxmlElement");
            Atom name = Intern(tag && !tag->value.Empty() ? tag->value : c.name);
            if (!added)
            {
                // Its code goes with its module: the parts made of it first.
                Types& s = T();
                for (u32 i = 0; i < s.parts.Size(); i++)
                    if (s.parts[i].c == &c)
                        DestroyPart(ElementHandle{i, s.parts[i].generation});
                if (ScriptType* t = ScriptTypeOf(name); t && t->native == &c)
                    UnregisterScriptType(t);
                return;
            }
            if (!tag)
                return;
            auto* t = new ScriptType();
            t->name = name;
            t->className = Intern(c.name);
            t->native = &c;
            t->base = Intern("Element");
            for (const scripting::NativeHandleField& f : c.fields)
                for (const scripting::NativeTag& ft : f.tags)
                    if (ft.key == StringView("HxmlAttribute"))
                    {
                        t->attributes.Push(Intern(ft.value.Empty() ? Kebab(f.name).View() : ft.value));
                        t->fields.Push(&f);
                    }
            RegisterScriptType(t, &CreateNative);
        }

        void* LookupPart(u64 id, const void* key, void*)
        {
            Element* x = ElementById(id);
            if (!x || !x->alive())
                return nullptr;
            ElementHandle e = x->handle();
            const Types& s = T();
            if (e.index >= s.parts.Size() || s.parts[e.index].generation != e.generation || !s.parts[e.index].c || s.parts[e.index].c->key != key)
                return nullptr;
            return s.parts[e.index].object;
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
                scripting::RegisterNativeExport("HelixUI.RegisterElementClass", std::bit_cast<const void*>(&RegisterManagedClass));
                scripting::RegisterNativeExport("HelixUI.SetElementFactory", std::bit_cast<const void*>(&SetElementFactory));
                // The element types of C++ classes of game modules.
                scripting::SetNativeHandleHandler("HelixEngine.UI.Element", &OnNativeClass, nullptr);
                scripting::SetNativeHandleLookup("HelixEngine.UI.Element", &LookupPart, nullptr);
            }
        };
        const ExportRegistrar registrar;

        // The panel whose root holds an element.
        Panel* PanelOf(ElementWorld& world, ElementHandle e)
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

    Element::Element() : Element(Intern("Element")) {}

    Element::Element(Atom type) : world_(&ElementWorld::Main())
    {
        InstallHooks(*world_);
        handle_ = tree().Create(type);
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
        // The object class of its type, or an Element.
        if (ObjectMaker* maker = Makers().Find(world.tree().Type(e)))
            return (*maker)(world, e);
        return new Element(world, e);
    }

    void Element::RegisterObjectClass(StringView type, ObjectMaker maker) { Makers().Set(Intern(type), maker); }

    bool Element::MarkAttached(ElementWorld& world, ElementHandle e)
    {
        Registry& r = RegistryOf(world);
        if (!world.tree().IsAlive(e))
            return false;
        if (r.attached.Size() <= e.index)
            r.attached.Resize(e.index + 1, ~0u);
        if (r.attached[e.index] == e.generation)
            return false;
        r.attached[e.index] = e.generation;
        return true;
    }

    // ---------------------------------------------------------------------------------------- styleSheets

    i32 Element::StyleSheetCount() const { return alive() ? i32(world_->styles().StyleSheets(handle_).Size()) : 0; }

    StyleSheet* Element::StyleSheetAt(i32 index) const
    {
        if (!alive() || index < 0)
            return nullptr;
        auto sheets = world_->styles().StyleSheets(handle_);
        return usize(index) < sheets.Size() ? StyleSheet::Of(sheets[usize(index)].get()) : nullptr;
    }

    void Element::AddStyleSheet(StyleSheet* sheet)
    {
        if (alive() && sheet && !ContainsStyleSheet(sheet))
            world_->styles().AddStyleSheet(handle_, sheet->sheet());
    }

    void Element::InsertStyleSheet(i32 index, StyleSheet* sheet)
    {
        if (alive() && sheet && !ContainsStyleSheet(sheet))
            world_->styles().InsertStyleSheet(handle_, index < 0 ? 0u : u32(index), sheet->sheet());
    }

    bool Element::RemoveStyleSheet(StyleSheet* sheet)
    {
        if (!alive() || !sheet || !ContainsStyleSheet(sheet))
            return false;
        world_->styles().RemoveStyleSheet(handle_, sheet->sheet().get());
        return true;
    }

    bool Element::ContainsStyleSheet(StyleSheet* sheet) const
    {
        if (!alive() || !sheet)
            return false;
        for (const auto& s : world_->styles().StyleSheets(handle_))
            if (s.get() == sheet->sheet().get())
                return true;
        return false;
    }

    void Element::ClearStyleSheets()
    {
        if (alive())
            world_->styles().ClearStyleSheets(handle_);
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

    void Element::SetAnchored(ElementWorld& world, ElementHandle e, Panel* panel)
    {
        Registry& r = RegistryOf(world);
        if (r.anchored.Size() <= e.index)
            r.anchored.Resize(e.index + 1);
        r.anchored[e.index] = panel;
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
        if (e.index < r.attached.Size())
            r.attached[e.index] = ~0u;
        world.callbacks().Clear(e);
        // Its C++ part goes with it.
        if (&world == &ElementWorld::Main())
            DestroyPart(e);
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
        // A C# class's object: its elements' type is the class's name (what type selectors match), asked once.
        if (!named_ && Wrapper() != 0 && alive() && T().namer)
        {
            named_ = true;
            char buffer[256];
            i32 n = T().namer(Wrapper(), buffer, i32(sizeof(buffer)));
            if (n > 0 && n <= i32(sizeof(buffer)))
            {
                Atom type = Intern(StringView(buffer, usize(n)));
                if (tree().Type(handle_) != type)
                    tree().SetType(handle_, type);
            }
        }
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

    Element* Element::contentContainer()
    {
        // Itself, unless a template names its content container.
        if (!alive())
            return this;
        ElementHandle c = tree().ContentContainer(handle_);
        Element* x = c == handle_ ? this : Of(*world_, c);
        return x ? x : this;
    }

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

    Panel* Element::panel() const { return alive() ? PanelOf(*world_, handle_) : nullptr; }

    EventSystem* Element::events() const
    {
        Panel* p = panel();
        return p ? &p->events() : nullptr;
    }

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

    bool Element::HasCustomStyle(StringView name) const
    {
        String text;
        return alive() && world_->styles().CustomProperty(handle_, name, text);
    }

    String Element::CustomStyleText(StringView name) const
    {
        String text;
        if (alive())
            world_->styles().CustomProperty(handle_, name, text);
        return text;
    }

    engine::Texture2D* Element::CustomStyleTexture(StringView name) const
    {
        String text;
        Panel* p = panel();
        if (!p || !p->Host() || !world_->styles().CustomProperty(handle_, name, text))
            return nullptr;
        // url("path") or resource("path").
        StringView t = text.View().Trimmed();
        usize open = t.Find('(');
        usize close = t.Find(')');
        if (open == StringView::npos || close == StringView::npos || close < open)
            return nullptr;
        StringView inside = t.Substr(open + 1, close - open - 1).Trimmed();
        if (inside.Size() >= 2 && (inside[0] == '"' || inside[0] == '\''))
            inside = inside.Substr(1, inside.Size() - 2);
        return p->Host()->TextureAt(Intern(inside));
    }

    namespace
    {
        // The computed values of a property.
        u32 ComputedValues(const ElementWorld& world, ElementHandle e, StringView property, StyleValue (&out)[kMaxValues])
        {
            PropertyId id = FindProperty(property);
            if (id == kNoProperty)
                return 0;
            return GetValues(Property(id), const_cast<ElementWorld&>(world).styles().Computed(e), out);
        }

        f32 NumberOf(const StyleValue& v) { return v.kind == StyleValue::Kind::Length ? v.length.value : v.kind == StyleValue::Kind::Angle ? AngleDegrees(v) : v.number; }

        i32 UnitOf(const StyleValue& v) { return v.kind == StyleValue::Kind::Length ? i32(v.length.unit) : 1; }
    }

    i32 Element::InlineStyleCount(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial))
            return 0;
        return i32(v.Size());
    }

    f32 Element::InlineStyleNumberAt(StringView property, i32 index) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || index < 0 || usize(index) >= v.Size())
            return 0;
        return NumberOf(v[usize(index)]);
    }

    i32 Element::InlineStyleUnitAt(StringView property, i32 index) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || index < 0 || usize(index) >= v.Size())
            return 0;
        return UnitOf(v[usize(index)]);
    }

    Color Element::InlineStyleColorAt(StringView property, i32 index) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || index < 0 || usize(index) >= v.Size())
            return Color(0, 0, 0, 0);
        return v[usize(index)].color;
    }

    String Element::InlineStyleWordAt(StringView property, i32 index) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || index < 0 || usize(index) >= v.Size())
            return String();
        return WordOf(FindProperty(property), v[usize(index)]);
    }

    f32 Element::ResolvedStyleNumberAt(StringView property, i32 index) const
    {
        StyleValue v[kMaxValues];
        u32 n = alive() ? ComputedValues(*world_, handle_, property, v) : 0;
        return index >= 0 && u32(index) < n ? NumberOf(v[index]) : 0.0f;
    }

    i32 Element::ResolvedStyleUnitAt(StringView property, i32 index) const
    {
        StyleValue v[kMaxValues];
        u32 n = alive() ? ComputedValues(*world_, handle_, property, v) : 0;
        return index >= 0 && u32(index) < n ? UnitOf(v[index]) : 0;
    }

    Color Element::ResolvedStyleColorAt(StringView property, i32 index) const
    {
        StyleValue v[kMaxValues];
        u32 n = alive() ? ComputedValues(*world_, handle_, property, v) : 0;
        return index >= 0 && u32(index) < n ? v[index].color : Color(0, 0, 0, 0);
    }

    String Element::ResolvedStyleWordAt(StringView property, i32 index) const
    {
        StyleValue v[kMaxValues];
        u32 n = alive() ? ComputedValues(*world_, handle_, property, v) : 0;
        return index >= 0 && u32(index) < n ? WordOf(FindProperty(property), v[index]) : String();
    }

    void Element::SetInlineStyleTexture(StringView property, engine::Texture2D* texture, Vector2 hotspot)
    {
        PropertyId id = FindProperty(property);
        if (!alive() || id == kNoProperty)
            return;
        // Textures set from scripts are their objects' ids in the style (paths are the sheets').
        Atom resource = EntityResource(texture);
        StyleValue values[3] = {StyleValue::OfResource(resource), StyleValue::OfNumber(hotspot.x), StyleValue::OfNumber(hotspot.y)};
        u32 count = Property(id).count >= 3 ? 3u : 1u;
        world_->styles().SetInlineValues(handle_, id, Span<const StyleValue>(values, count));
    }

    engine::Texture2D* Element::TextureOf(Atom resource) const
    {
        if (resource == kNoAtom)
            return nullptr;
        if (engine::Texture2D* set = EntityResourceTexture(AtomName(resource)))
            return set;
        Panel* p = panel();
        return p && p->Host() ? p->Host()->TextureAt(resource) : nullptr;
    }

    engine::Texture2D* Element::InlineStyleTexture(StringView property) const
    {
        Array<StyleValue> v;
        bool initial = false;
        if (!alive() || !InlineOf(*world_, handle_, property, v, initial) || v.Empty() || v[0].kind != StyleValue::Kind::Resource)
            return nullptr;
        return TextureOf(v[0].resource);
    }

    engine::Texture2D* Element::ResolvedStyleTexture(StringView property) const
    {
        StyleValue v[kMaxValues];
        u32 n = alive() ? ComputedValues(*world_, handle_, property, v) : 0;
        return n > 0 && v[0].kind == StyleValue::Kind::Resource ? TextureOf(v[0].resource) : nullptr;
    }

    // -------------------------------------------------------------------------------------------- the boxes

    Rect Element::layout() const
    {
        if (!alive())
            return Rect();
        const LayoutBox& b = world_->layout().Box(handle_);
        return Rect(b.x, b.y, b.width, b.height);
    }

    Rect Element::worldBound() const
    {
        if (!alive())
            return Rect();
        // The bounds of the border box as shown (its transforms and its ancestors').
        Rect r = world_->layout().WorldRect(handle_);
        Affine m = WorldTransform(world_->tree(), world_->layout(), world_->styles(), handle_);
        return m.IsIdentity() ? r : m.Bounds(r);
    }

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
        if (!alive())
            return p;
        Rect r = world_->layout().WorldRect(handle_);
        Vector2 q = WorldTransform(world_->tree(), world_->layout(), world_->styles(), handle_).Inverse().Apply(p);
        return Vector2(q.x - r.x, q.y - r.y);
    }

    Vector2 Element::LocalToWorld(Vector2 p) const
    {
        if (!alive())
            return p;
        Rect r = world_->layout().WorldRect(handle_);
        return WorldTransform(world_->tree(), world_->layout(), world_->styles(), handle_).Apply(Vector2(p.x + r.x, p.y + r.y));
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

// ---------------------------------------------------------------------------------------------- TemplateContainer

namespace helix::ui
{
    namespace
    {
        struct TemplateContainerClass
        {
            TemplateContainerClass()
            {
                Element::RegisterObjectClass("TemplateContainer", [](ElementWorld& world, ElementHandle e) -> Element* { return new TemplateContainer(world, e); });
            }
        } templateContainerClass;
    }

    TemplateContainer::TemplateContainer() : Element(Intern("TemplateContainer")) {}

    TemplateContainer::TemplateContainer(StringView templateId) : Element(Intern("TemplateContainer")), templateId_(templateId) {}

    scripting::TypeId TemplateContainer::HelixType() const { return scripting::TypeOf<TemplateContainer>(); }

    UITemplate* TemplateContainer::templateSource() const { return ScriptObjectAs<UITemplate>(source_.Get()); }

    void TemplateContainer::SetTemplateSource(UITemplate* source) { source_ = engine::PPtr<engine::Object>(source); }
}
