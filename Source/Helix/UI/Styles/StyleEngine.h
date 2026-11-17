#pragma once

#include <Helix/UI/Layout/Layout.h>
#include <Helix/UI/Styles/StyleSheetData.h>

#include <memory>

// Computes the styles of an element tree: the sheets attached to an element apply to it and its descendants (a closer
// sheet wins over an outer one at the same specificity), then its inline style. Inherited properties come from the
// parent, var() takes the custom properties of the element and its ancestors. Only elements whose style is dirty are
// computed again. A changed value of a property with a transition moves there over time. The layout inputs go to the
// layout engine when they change.
namespace helix::ui
{
    class StyleEngine
    {
    public:
        StyleEngine(ElementTree& tree, LayoutEngine& layout) : tree_(tree), layout_(layout) {}

        // A sheet for an element's subtree (the engine keeps the shared pointer).
        void AddStyleSheet(ElementHandle owner, std::shared_ptr<const StyleSheetData> sheet);
        void RemoveStyleSheet(ElementHandle owner, const StyleSheetData* sheet);
        void ClearStyleSheets(ElementHandle owner);
        void InsertStyleSheet(ElementHandle owner, u32 index, std::shared_ptr<const StyleSheetData> sheet);
        // The sheets added to an element, in order (empty for a gone element).
        Span<const std::shared_ptr<const StyleSheetData>> StyleSheets(ElementHandle owner) const;
        // An element's inline style (declarations, as in a style attribute); errors are returned.
        void SetInlineStyle(ElementHandle e, StringView declarations, Array<StyleError>* errors = nullptr);
        // One property of the inline style, as scripts set it: its value's text (false with the reason when it is no
        // value of the property), `initial`, or gone (the sheets decide again).
        bool SetInlineProperty(ElementHandle e, PropertyId property, StringView value, String* error = nullptr);
        void SetInlineInitial(ElementHandle e, PropertyId property);
        // One property of the inline style as values (what scripts set that has no text: a texture's object).
        void SetInlineValues(ElementHandle e, PropertyId property, Span<const StyleValue> values);
        void ClearInlineProperty(ElementHandle e, PropertyId property);
        // The inline values of a property (false when the inline style does not set it; `initial` gives no values).
        bool InlineProperty(ElementHandle e, PropertyId property, Array<StyleValue>& out, bool* initial = nullptr) const;

        // Computes the dirty styles under root and advances the transitions by dt seconds.
        void Update(ElementHandle root, f32 dt);

        // The style as shown now (transitions included).
        const ComputedStyle& Computed(ElementHandle e) const;
        // Whether a transition is still running (something to draw again).
        bool Animating() const { return animating_ > 0; }
        // How many elements the last Update computed.
        u32 LastComputed() const { return computed_; }

        // A custom property (--name) the element's rules or inline style declare, its var() replaced: its text
        // (false when it declares none of the name). The names it declares.
        bool CustomProperty(ElementHandle e, StringView name, String& out) const;
        void CustomPropertyNames(ElementHandle e, Array<Atom>& out) const;
        // The elements the last Update computed that have custom properties (or lost their last ones).
        Span<const ElementHandle> CustomResolved() const { return Span<const ElementHandle>(customResolved_.Data(), customResolved_.Size()); }

    private:
        struct Running
        {
            PropertyId property = kNoProperty;
            // Every value of the property (several for translate, scale, a shadow, ...).
            StyleValue from[kMaxValues], to[kMaxValues];
            u32 count = 1;
            f32 elapsed = 0, duration = 0, delay = 0;
            Easing easing = Easing::Ease;
        };

        struct Slot
        {
            u32 generation = ~0u;
            bool computedOnce = false;
            // The cascade's result and what is shown (the result with the running transitions).
            ComputedStyle target, shown;
            Array<Declaration> inlineStyle;
            Array<Running> running;
            // The custom properties declared on the element (name, text).
            Array<Atom> varNames;
            Array<String> varValues;
            Array<std::shared_ptr<const StyleSheetData>> sheets;
            // It had custom properties when last computed.
            bool hadCustom = false;
        };

        Slot& At(ElementHandle e);
        void Compute(ElementHandle e, const ComputedStyle* parent);
        bool Matches(const Selector& s, ElementHandle e) const;
        bool MatchFrom(const Selector& s, u32 part, ElementHandle e) const;
        bool MatchPart(const SelectorPart& p, ElementHandle e) const;
        void Apply(ComputedStyle& style, const Declaration& d, ElementHandle e);
        StringView Variable(ElementHandle e, Atom name) const;
        bool Substitute(ElementHandle e, StringView raw, String& text) const;
        void StartTransitions(Slot& s, const ComputedStyle& before);
        void Animate(ElementHandle e, Slot& s, f32 dt);

        ElementTree& tree_;
        LayoutEngine& layout_;
        Array<Slot> slots_;
        // The sheets in effect while walking the tree (outer first) and the ancestor filter's counters.
        Array<const StyleSheetData*> active_;
        // The element each sheet in effect was added to, and the one of the sheet being matched (its :root).
        Array<ElementHandle> activeOwners_;
        ElementHandle matchRoot_;
        u16 bloom_[4096] = {};
        u32 computed_ = 0;
        u32 animating_ = 0;
        Array<ElementHandle> customResolved_;
    };
}
