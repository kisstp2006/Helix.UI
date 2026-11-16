#pragma once

#include <Helix/Core/Basics/Types.h>
#include <Helix/Core/Strings/StringView.h>

// Interned names: element types, names, classes, style properties and keywords compare as integers. The table lives
// for the process and is used from the thread that runs the UI.
namespace helix::ui
{
    using Atom = u32;
    inline constexpr Atom kNoAtom = 0;

    // The atom of a name, made on first use.
    Atom Intern(StringView name);
    // The atom of a name already interned, kNoAtom otherwise.
    Atom FindAtom(StringView name);
    // The name of an atom ("" for kNoAtom).
    StringView AtomName(Atom atom);
}
