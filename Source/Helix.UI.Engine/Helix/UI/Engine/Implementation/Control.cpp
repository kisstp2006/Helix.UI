#include <Helix/UI/Engine/Control.h>

#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/UI/Styles/Properties.h>
#include <Helix/UI/Styles/StyleSheetData.h>

#include <bit>
#include <cstdlib>

namespace helix::ui
{
    namespace
    {
        // C#'s function of the controls' notifications: (the element's index, generation, what).
        using ManagedNotify = void (*)(u32 index, u32 generation, i32 what);
        ManagedNotify managedNotify = nullptr;

        void SetControlNotify(void* function) { managedNotify = std::bit_cast<ManagedNotify>(function); }

        struct Ticking
        {
            ElementHandle element;
            ControlTick tick;
        };

        Array<Ticking>& Ticks()
        {
            static Array<Ticking>* ticks = new Array<Ticking>();
            return *ticks;
        }

        struct ExportRegistrar
        {
            ExportRegistrar() { scripting::RegisterNativeExport("HelixUI.SetControlNotify", std::bit_cast<const void*>(&SetControlNotify)); }
        } registrar;
    }

    void NotifyControl(ElementHandle e, ControlNotify what)
    {
        if (managedNotify && ElementWorld::Main().tree().IsAlive(e))
            managedNotify(e.index, e.generation, i32(what));
    }

    bool ParseBoolText(StringView text, bool& out)
    {
        StringView t = text.Trimmed();
        if (t == StringView("true") || t == StringView("True"))
            out = true;
        else if (t == StringView("false") || t == StringView("False"))
            out = false;
        else
            return false;
        return true;
    }

    bool ParseIntText(StringView text, i64& out)
    {
        String t(text.Trimmed());
        if (t.Empty())
            return false;
        char* end = nullptr;
        long long v = std::strtoll(t.CStr(), &end, 10);
        if (end != t.CStr() + t.Size())
            return false;
        out = i64(v);
        return true;
    }

    bool ParseUIntText(StringView text, u64& out)
    {
        String t(text.Trimmed());
        if (t.Empty() || t[0] == '-')
            return false;
        char* end = nullptr;
        unsigned long long v = std::strtoull(t.CStr(), &end, 10);
        if (end != t.CStr() + t.Size())
            return false;
        out = u64(v);
        return true;
    }

    bool ParseFloatText(StringView text, f64& out)
    {
        String t(text.Trimmed());
        if (t.Empty())
            return false;
        char* end = nullptr;
        f64 v = std::strtod(t.CStr(), &end);
        if (end != t.CStr() + t.Size())
            return false;
        out = v;
        return true;
    }

    bool ParseEnumText(StringView text, Span<const StringView> names, i32& out)
    {
        StringView t = text.Trimmed();
        for (u32 i = 0; i < names.Size(); i++)
            if (names[i] == t)
            {
                out = i32(i);
                return true;
            }
        i64 n = 0;
        if (ParseIntText(t, n) && n >= 0 && n < i64(names.Size()))
        {
            out = i32(n);
            return true;
        }
        return false;
    }

    void StartControlTick(Element& self, ControlTick tick)
    {
        for (const Ticking& t : Ticks())
            if (t.element == self.handle() && t.tick == tick)
                return;
        Ticks().Push({self.handle(), tick});
    }

    void TickControls(f64 time)
    {
        Array<Ticking>& ticks = Ticks();
        ElementWorld& world = ElementWorld::Main();
        for (usize i = 0; i < ticks.Size();)
        {
            Ticking t = ticks[i];
            Element* o = world.tree().IsAlive(t.element) ? Element::Of(world, t.element) : nullptr;
            if (!o || !t.tick(*o, time))
            {
                // The list may have grown in the tick: find the entry again.
                for (usize k = 0; k < ticks.Size(); k++)
                    if (ticks[k].element == t.element && ticks[k].tick == t.tick)
                    {
                        ticks.RemoveAt(k);
                        break;
                    }
                continue;
            }
            i++;
        }
    }

    namespace detail
    {
        ElementHandle ControlContent(ElementTree& tree, ElementHandle e)
        {
            ElementWorld& world = ElementWorld::Main();
            if (&tree != &world.tree())
                return e;
            Element* x = Element::Of(world, e);
            Element* c = x ? x->contentContainer() : nullptr;
            return c && c->alive() ? c->handle() : e;
        }
    }
}
