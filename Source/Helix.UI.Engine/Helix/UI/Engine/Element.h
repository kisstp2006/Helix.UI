#pragma once

#include <Helix/Core/Basics/Tags.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Math/Geometry.h>
#include <Helix/Math/Vector.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Types/Type.h>
#include <Helix/UI/Events/EventSystem.h>
#include <Helix/UI/World/ElementWorld.h>

// The UI's objects for scripts: CallbackEventHandler (the root of the module's objects, by id), Focusable and Element,
// the reference's chain. An Element is an object over an element of a world, made when a script first needs it (C#
// `new Element()`, or an element of markup reached from a script). An element lives while it is in a tree or a
// script holds it: its object keeps its C# object alive while the element is in a tree (or is a panel's root) and lets
// it go when it leaves; destroying a subtree takes the elements a script holds out instead of destroying them.
namespace helix::ui
{
    class Element;
    class Panel;
    class StyleSheet;

    HX_CLASS(Root, Owned, Namespace = "HelixEngine.UI", Abstract)
    class CallbackEventHandler
    {
    public:
        virtual ~CallbackEventHandler();
        CallbackEventHandler(const CallbackEventHandler&) = delete;
        CallbackEventHandler& operator=(const CallbackEventHandler&) = delete;

        u64 Id() const { return id_; }
        u64& Wrapper() { return wrapper_; }
        virtual scripting::TypeId HelixType() const = 0;
        static CallbackEventHandler* Find(u64 id);
        virtual Element* AsElement() { return nullptr; }

    protected:
        CallbackEventHandler();

    private:
        u64 id_ = 0;
        u64 wrapper_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.UI", Abstract)
    class Focusable : public CallbackEventHandler
    {
    protected:
        Focusable() = default;
    };

    HX_CLASS(Namespace = "HelixEngine.UI")
    class Element : public Focusable
    {
    public:
        // A new element of the process's world, of no parent.
        HX_FUNCTION(Constructor) Element();
        ~Element() override;
        scripting::TypeId HelixType() const override;

        // The object of an element of a world (made once; null for a dead handle).
        static Element* Of(ElementWorld& world, ElementHandle e);
        static Element* Of(ElementHandle e) { return Of(ElementWorld::Main(), e); }
        ElementHandle handle() const { return handle_; }
        ElementWorld& world() const { return *world_; }
        bool alive() const;
        // The element of a script object's id (for the C# side's own code).
        HX_FUNCTION(Internal) static Element* FromHandle(u32 index, u32 generation);
        HX_FUNCTION(Internal) u32 handleIndex() const { return handle_.index; }
        HX_FUNCTION(Internal) u32 handleGeneration() const { return handle_.generation; }

        // ------------------------------------------------------------------------------------------- hierarchy
        HX_PROPERTY() String name() const;
        void SetName(StringView name);
        HX_PROPERTY() Element* parent() const;
        // Where Add puts children (the element itself unless a control says otherwise).
        HX_PROPERTY() virtual Element* contentContainer();
        HX_PROPERTY() i32 childCount() const;
        // ElementAt, Insert, Remove and RemoveAt check their arguments in C# (the reference's exceptions).
        HX_FUNCTION(Internal) Element* ElementAtChecked(i32 index) const;
        // The first element of the name in its subtree, itself first (C++ scripts' Find; C#'s Find is a query).
        HX_FUNCTION(Internal, ApiName = "Find") Element* FindNative(StringView name) const;
        HX_FUNCTION() i32 IndexOf(Element* child) const;
        HX_FUNCTION() void Add(Element* child);
        HX_FUNCTION(Internal) bool InsertChecked(i32 index, Element* child);
        HX_FUNCTION(Internal) bool RemoveChecked(Element* child);
        HX_FUNCTION(Internal) bool RemoveAtChecked(i32 index);
        HX_FUNCTION() void Clear();
        HX_FUNCTION() void RemoveFromHierarchy();
        HX_FUNCTION() void BringToFront();
        HX_FUNCTION() void SendToBack();
        HX_FUNCTION() void PlaceBehind(Element* sibling);
        HX_FUNCTION() void PlaceInFront(Element* sibling);
        HX_FUNCTION() bool Contains(Element* child) const;
        HX_FUNCTION() Element* FindCommonAncestor(Element* other) const;
        // The hierarchy itself (no content container): the C# side's `hierarchy`.
        HX_FUNCTION(Internal) i32 HierarchyChildCount() const;
        HX_FUNCTION(Internal) Element* HierarchyElementAt(i32 index) const;
        HX_FUNCTION(Internal) void HierarchyInsert(i32 index, Element* child);
        HX_FUNCTION(Internal) void HierarchyRemove(Element* child);

        // --------------------------------------------------------------------------------------------- classes
        HX_FUNCTION() void AddToClassList(StringView className);
        HX_FUNCTION() void RemoveFromClassList(StringView className);
        HX_FUNCTION() bool ClassListContains(StringView className) const;
        HX_FUNCTION() void EnableInClassList(StringView className, bool enable);
        HX_FUNCTION() void ToggleInClassList(StringView className);
        HX_FUNCTION() void ClearClassList();
        HX_FUNCTION() Array<String> GetClasses() const;

        // ------------------------------------------------------------------------------------ state and focus
        HX_PROPERTY() bool enabledSelf() const;
        HX_PROPERTY() bool enabledInHierarchy() const;
        HX_FUNCTION() void SetEnabled(bool value);
        HX_PROPERTY() bool visible() const;
        void SetVisible(bool value);
        HX_PROPERTY() PickingMode pickingMode() const;
        void SetPickingMode(PickingMode value);
        HX_PROPERTY() bool focusable() const;
        void SetFocusable(bool value);
        HX_PROPERTY() i32 tabIndex() const;
        void SetTabIndex(i32 value);
        HX_PROPERTY() bool delegatesFocus() const;
        void SetDelegatesFocus(bool value);
        HX_PROPERTY() bool canGrabFocus() const;
        HX_FUNCTION() void Focus();
        HX_FUNCTION() void Blur();
        HX_PROPERTY() String tooltip() const { return tooltip_; }
        void SetTooltip(StringView value) { tooltip_ = String(value); }
        HX_PROPERTY() String viewDataKey() const { return viewDataKey_; }
        void SetViewDataKey(StringView value) { viewDataKey_ = String(value); }
        HX_FUNCTION() void MarkDirtyRepaint();
        // The element's script draws content of its own (C#'s generateVisualContent is set).
        HX_FUNCTION(Internal) void SetScriptPaintInternal(bool on);

        // ------------------------------------------------------------------------- styleSheets, for C#
        HX_FUNCTION(Internal) i32 StyleSheetCount() const;
        HX_FUNCTION(Internal) StyleSheet* StyleSheetAt(i32 index) const;
        HX_FUNCTION(Internal) void AddStyleSheet(StyleSheet* sheet);
        HX_FUNCTION(Internal) void InsertStyleSheet(i32 index, StyleSheet* sheet);
        HX_FUNCTION(Internal) bool RemoveStyleSheet(StyleSheet* sheet);
        HX_FUNCTION(Internal) bool ContainsStyleSheet(StyleSheet* sheet) const;
        HX_FUNCTION(Internal) void ClearStyleSheets();

        // ----------------------------------------------------------------------- styles, for the C# side
        // The inline style by property name: a value's text, StyleKeyword Null (0: gone) or Initial (1), and what it
        // holds: its kind (-2 unset, -1 initial, else the value's kind), number, length unit, color, keyword, resource.
        HX_FUNCTION(Internal) bool SetInlineStyleText(StringView property, StringView value);
        HX_FUNCTION(Internal) void SetInlineStyleKeyword(StringView property, i32 keyword);
        HX_FUNCTION(Internal) i32 InlineStyleKind(StringView property) const;
        HX_FUNCTION(Internal) f32 InlineStyleNumber(StringView property) const;
        HX_FUNCTION(Internal) i32 InlineStyleUnit(StringView property) const;
        HX_FUNCTION(Internal) Color InlineStyleColor(StringView property) const;
        HX_FUNCTION(Internal) String InlineStyleWord(StringView property) const;
        HX_FUNCTION(Internal) String InlineStyleResource(StringView property) const;
        // The computed style's value of a property (resolvedStyle): number (lengths in pixels), color, keyword.
        HX_FUNCTION(Internal) f32 ResolvedStyleNumber(StringView property) const;
        HX_FUNCTION(Internal) Color ResolvedStyleColor(StringView property) const;
        HX_FUNCTION(Internal) String ResolvedStyleWord(StringView property) const;
        // Properties of several values: how many the inline style holds, and the index-th value's parts; the
        // computed ones (lengths with their unit: 0 auto, 1 pixels, 2 percent; angles in degrees).
        HX_FUNCTION(Internal) i32 InlineStyleCount(StringView property) const;
        HX_FUNCTION(Internal) f32 InlineStyleNumberAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) i32 InlineStyleUnitAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) Color InlineStyleColorAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) String InlineStyleWordAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) f32 ResolvedStyleNumberAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) i32 ResolvedStyleUnitAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) Color ResolvedStyleColorAt(StringView property, i32 index) const;
        HX_FUNCTION(Internal) String ResolvedStyleWordAt(StringView property, i32 index) const;
        // A texture as a property's value (background-image, cursor with its hotspot; null: none), and the texture a
        // value names (a texture set so, or a path found by the element's panel).
        HX_FUNCTION(Internal) void SetInlineStyleTexture(StringView property, engine::Texture2D* texture, Vector2 hotspot);
        HX_FUNCTION(Internal) engine::Texture2D* InlineStyleTexture(StringView property) const;
        HX_FUNCTION(Internal) engine::Texture2D* ResolvedStyleTexture(StringView property) const;
        // The custom properties (customStyle): whether the element declares one, its text (var() replaced), and the
        // texture its url() or resource() names (as the element's panel finds it; null outside of one).
        HX_FUNCTION(Internal) bool HasCustomStyle(StringView name) const;
        HX_FUNCTION(Internal) String CustomStyleText(StringView name) const;
        HX_FUNCTION(Internal) engine::Texture2D* CustomStyleTexture(StringView name) const;

        // ------------------------------------------------------------------------------------------- the boxes
        // The border box in the parent's space, in the panel's space, and the content box in the element's.
        HX_PROPERTY() Rect layout() const;
        HX_PROPERTY() Rect worldBound() const;
        HX_PROPERTY() Rect localBound() const;
        HX_PROPERTY() Rect contentRect() const;
        HX_PROPERTY() Rect paddingRect() const;
        HX_FUNCTION() bool ContainsPoint(Vector2 localPoint) const;
        HX_FUNCTION() Vector2 WorldToLocal(Vector2 p) const;
        HX_FUNCTION() Vector2 LocalToWorld(Vector2 p) const;
        HX_FUNCTION() Vector2 ChangeCoordinatesTo(Element* dest, Vector2 point) const;

        // ------------------------------------------------------------------------------------------ for C++
        // The world's hooks: kept while a script holds it, its object told when the element goes or moves.
        static void InstallHooks(ElementWorld& world);
        // Panel roots count as being in a tree; their panel gives the focus to the elements under them.
        static void SetAnchored(ElementWorld& world, ElementHandle e, Panel* panel);
        // Marks that an element took its type's behavior (true the first time).
        static bool MarkAttached(ElementWorld& world, ElementHandle e);
        // The object class of an element type (TemplateContainer, ...): Of makes such objects for its elements.
        using ObjectMaker = Element* (*)(ElementWorld& world, ElementHandle e);
        static void RegisterObjectClass(StringView type, ObjectMaker maker);
        // The panel the element is in and its events (null outside of one).
        Panel* panel() const;
        EventSystem* events() const;
        Element* AsElement() override { return this; }
        // Its C# object holds state (callbacks, userData): kept while the element is in a tree. Called from the C# side
        // through an export (never inside a binding's call, which must not call back into C#).
        void KeepScriptState() { UpdateRetain(); }
        // The C# side's scheduler, once a frame (the views call it).
        static void TickScripts(f64 time);

        // ---------------------------------------------------------------------- the panel, for the C# side
        HX_FUNCTION(Internal) bool CapturePointerInternal(i32 pointerId);
        HX_FUNCTION(Internal) void ReleasePointerInternal(i32 pointerId);
        HX_FUNCTION(Internal) bool HasPointerCaptureInternal(i32 pointerId) const;
        HX_FUNCTION(Internal) Element* CapturingElementInternal(i32 pointerId) const;
        HX_FUNCTION(Internal) Element* FocusedElementInternal() const;
        HX_FUNCTION(Internal) Element* PanelRootInternal() const;
        HX_FUNCTION(Internal) Element* PickInternal(Vector2 point) const;
        HX_FUNCTION(Internal) Array<Element*> PickAllInternal(Vector2 point) const;

    protected:
        // The object of an existing element (Of makes them).
        Element(ElementWorld& world, ElementHandle e);
        // A new element of a type (the classes of element types).
        explicit Element(Atom type);

    private:
        friend struct scripting::ObjectHandles<CallbackEventHandler>;
        engine::Texture2D* TextureOf(Atom resource) const;
        ElementTree& tree() const { return world_->tree(); }
        // The C# object held strongly while the element is in a tree, let go when it leaves.
        void UpdateRetain();
        static bool Keep(void* user, ElementHandle e);
        static void Destroyed(void* user, ElementHandle e);
        static void Moved(void* user, ElementHandle e);

        ElementWorld* world_ = nullptr;
        ElementHandle handle_;
        u64 retained_ = 0;
        // Its type was named after its C# class.
        bool named_ = false;
        String tooltip_, viewDataKey_;
    };
}

template <>
struct helix::scripting::ObjectHandles<helix::ui::CallbackEventHandler>
{
    static u64 ToId(const ui::CallbackEventHandler* h) { return h->Id(); }
    static ui::CallbackEventHandler* FromId(u64 id);
    static TypeId TypeOf(const ui::CallbackEventHandler* h) { return h->HelixType(); }
    static u64& WrapperOf(ui::CallbackEventHandler* h) { return h->Wrapper(); }
};
