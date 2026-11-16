#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Containers/HashMap.h>
#include <Helix/UI/Elements/ElementTree.h>
#include <Helix/UI/Styles/Properties.h>

#include <memory>

// HSS style sheets: rules of selectors and declarations, parsed from text; @import url("...") at the start brings in
// other sheets, whose rules come before the sheet's own (whoever loads the sheet finds them, see SetImported). Selectors are type names, #names, .classes
// and :pseudo states (hover, active, focus, checked, disabled, enabled, root), joined by descendant (space) and child
// (>) combinators, in lists separated by commas. Declarations set properties (Properties.h), custom properties
// (--name) and take var(--name, fallback) values.
namespace helix::ui
{
    struct StyleError
    {
        u32 line = 0, column = 0;
        String message;
    };

    enum class Combinator : u8
    {
        // The leftmost part.
        None,
        Descendant,
        Child,
    };

    struct SelectorPart
    {
        // kNoAtom: any type.
        Atom type = kNoAtom;
        Atom name = kNoAtom;
        Array<Atom> classes;
        PseudoStates required = PseudoStates::None;
        PseudoStates forbidden = PseudoStates::None;
        // How the part to its left relates to it.
        Combinator left = Combinator::None;
    };

    struct Selector
    {
        // The rightmost part first.
        Array<SelectorPart> parts;
        // Names, then classes and pseudo states, then types (8 bits each).
        u32 specificity = 0;
        // The atoms the element's ancestors must have (for the ancestor filter).
        Array<Atom> ancestorAtoms;
    };

    struct Declaration
    {
        PropertyId property = kNoProperty;
        // A custom property (--name), with its text in raw.
        Atom custom = kNoAtom;
        Array<StyleValue> values;
        // The value's text when it holds var() (parsed once the variables are known) or for a custom property.
        String raw;
        bool hasVar = false;
        // `initial`: the property's initial value.
        bool initial = false;
        // A shorthand whose value holds var(): its name (expanded into its longhands once the variables are known).
        Atom shorthand = kNoAtom;
    };

    struct StyleRule
    {
        Array<Selector> selectors;
        Array<Declaration> declarations;
    };

    class StyleSheetData
    {
    public:
        // Parses a sheet; what cannot be parsed is reported and skipped (the rest still applies).
        static StyleSheetData Parse(StringView text);

        Span<const StyleRule> Rules() const { return Span<const StyleRule>(rules_.Data(), rules_.Size()); }
        Span<const StyleError> Errors() const { return Span<const StyleError>(errors_.Data(), errors_.Size()); }
        // The paths of its @import rules, as written, and the sheets found for them (in order; a missing one is left
        // out).
        Span<const String> Imports() const { return Span<const String>(imports_.Data(), imports_.Size()); }
        Span<const std::shared_ptr<const StyleSheetData>> Imported() const
        {
            return Span<const std::shared_ptr<const StyleSheetData>>(imported_.Data(), imported_.Size());
        }
        void SetImported(Array<std::shared_ptr<const StyleSheetData>> sheets) { imported_ = Move(sheets); }

        // The selectors (rule << 16 | selector) whose rightmost part may match an element of this type, name and
        // classes: those keyed by its name, its classes, its type, and those that match any element.
        template <typename F>
        void ForEachCandidate(Atom type, Atom name, Span<const Atom> classes, F&& f) const
        {
            if (name != kNoAtom)
                if (const Array<u32>* list = byName_.Find(name))
                    for (u32 s : *list)
                        f(s);
            for (Atom c : classes)
                if (const Array<u32>* list = byClass_.Find(c))
                    for (u32 s : *list)
                        f(s);
            if (const Array<u32>* list = byType_.Find(type))
                for (u32 s : *list)
                    f(s);
            for (u32 s : universal_)
                f(s);
        }

        const Selector& SelectorAt(u32 key) const { return rules_[key >> 16].selectors[key & 0xFFFF]; }

    private:
        void Index();

        Array<StyleRule> rules_;
        Array<StyleError> errors_;
        Array<String> imports_;
        Array<std::shared_ptr<const StyleSheetData>> imported_;
        HashMap<Atom, Array<u32>> byName_, byClass_, byType_;
        Array<u32> universal_;
    };

    // The declarations of an inline style ("width: 10px; color: red").
    void ParseDeclarations(StringView text, Array<Declaration>& out, Array<StyleError>& errors);
    // A property's value from text (shorthands are not values); false with the reason.
    bool ParseValue(PropertyId property, StringView text, Array<StyleValue>& out, String& error);
    // A shorthand's longhands from its value's text (var() already replaced); false with the reason.
    bool ExpandShorthand(Atom shorthand, StringView text, Array<Declaration>& out, String& error);
}
