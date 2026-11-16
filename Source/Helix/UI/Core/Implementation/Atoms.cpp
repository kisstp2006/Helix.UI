#include <Helix/UI/Core/Atoms.h>

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Core/Strings/String.h>

#include <memory>

namespace helix::ui
{
    namespace
    {
        // Never destroyed: atoms stay valid until the process ends.
        struct AtomTable
        {
            HashMap<StringView, Atom> byName;
            // The names, kept where their views point.
            Array<std::unique_ptr<String>> names;

            AtomTable() { names.Push(std::make_unique<String>("")); }
        };

        AtomTable& Table()
        {
            static AtomTable* table = new AtomTable();
            return *table;
        }
    }

    Atom Intern(StringView name)
    {
        if (name.Empty())
            return kNoAtom;
        AtomTable& t = Table();
        if (const Atom* found = t.byName.Find(name))
            return *found;
        Atom atom = Atom(t.names.Size());
        t.names.Push(std::make_unique<String>(name));
        t.byName.Set(t.names.Back()->View(), atom);
        return atom;
    }

    Atom FindAtom(StringView name)
    {
        const Atom* found = Table().byName.Find(name);
        return found ? *found : kNoAtom;
    }

    StringView AtomName(Atom atom)
    {
        AtomTable& t = Table();
        return atom < t.names.Size() ? t.names[atom]->View() : StringView();
    }
}
