#include <Helix/UI/Text/Fonts/SystemFonts.h>

#include <Helix/Core/Strings/Format.h>
#include <Helix/Platform/IO/FileSystem.h>

#include <cstdlib>
#include <mutex>

namespace helix::ui
{
    namespace
    {
        // The platform's font folders: an environment variable (empty: none) and a path below it.
        struct DefaultFolder
        {
            const char* variable;
            const char* path;
        };
#if defined(_WIN32)
        constexpr DefaultFolder kDefaults[] = {{"WINDIR", "Fonts"}, {"LOCALAPPDATA", "Microsoft/Windows/Fonts"}};
#elif defined(__ANDROID__)
        constexpr DefaultFolder kDefaults[] = {{"", "/system/fonts"}};
#elif defined(__EMSCRIPTEN__)
        constexpr DefaultFolder kDefaults[] = {{"", ""}};
#else
        constexpr DefaultFolder kDefaults[] = {{"", "/usr/share/fonts"}, {"", "/usr/local/share/fonts"}, {"HOME", ".local/share/fonts"}, {"HOME", ".fonts"}};
#endif

        struct State
        {
            std::mutex lock;
            Array<String> folders;
            Array<SystemFontFace> faces;
            bool defaults = false;
            bool scanned = false;
        };

        State& S()
        {
            static State* s = new State();
            return *s;
        }

        void AddDefaults(State& s)
        {
            if (s.defaults)
                return;
            s.defaults = true;
            for (const DefaultFolder& d : kDefaults)
            {
                if (!d.path[0])
                    continue;
                if (!d.variable[0])
                {
                    s.folders.Push(String(d.path));
                    continue;
                }
                if (const char* base = std::getenv(d.variable))
                    s.folders.Push(platform::JoinPath(StringView(base), StringView(d.path)));
            }
        }

        bool IsFontFile(StringView name)
        {
            StringView ext = platform::GetExtension(name);
            for (StringView e : {StringView(".ttf"), StringView(".otf"), StringView(".ttc"), StringView(".otc"), StringView(".TTF"), StringView(".OTF"),
                                 StringView(".TTC")})
                if (ext == e)
                    return true;
            return false;
        }

        void Scan(StringView folder, Array<SystemFontFace>& out, u32 depth)
        {
            if (depth > 6)
                return;
            struct Listing
            {
                Array<String> files, folders;
            } list;
            platform::ListDirectory(
                folder,
                [](const platform::DirectoryEntry& e, void* user) {
                    auto* l = static_cast<Listing*>(user);
                    if (e.name == "." || e.name == "..")
                        return;
                    (e.isDirectory ? l->folders : l->files).Push(String(e.name));
                },
                &list);
            for (const String& f : list.files)
                if (IsFontFile(f.View()))
                    SystemFonts::ReadNames(platform::JoinPath(folder, f.View()), out);
            for (const String& d : list.folders)
                Scan(platform::JoinPath(folder, d.View()), out, depth + 1);
        }

        bool SameText(StringView a, StringView b)
        {
            if (a.Size() != b.Size())
                return false;
            for (usize i = 0; i < a.Size(); i++)
            {
                char x = a[i], y = b[i];
                x = x >= 'A' && x <= 'Z' ? char(x + 32) : x;
                y = y >= 'A' && y <= 'Z' ? char(y + 32) : y;
                if (x != y)
                    return false;
            }
            return true;
        }

        u32 Be32(const u8* p) { return u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3]; }
        u16 Be16(const u8* p) { return u16(p[0] << 8 | p[1]); }

        bool ReadAt(platform::File& f, i64 at, usize size, Array<u8>& out)
        {
            out.Resize(size);
            return f.Seek(at) && f.Read(out.Data(), size) == size;
        }

        // A name of the table: Windows Unicode (UTF-16 to UTF-8) preferred, else Mac Roman (ASCII kept).
        String NameOf(const Array<u8>& table, u16 id)
        {
            if (table.Size() < 6)
                return String();
            u16 count = Be16(table.Data() + 2), strings = Be16(table.Data() + 4);
            String mac;
            for (u32 i = 0; i < count && 6 + 12 * (i + 1) <= table.Size(); i++)
            {
                const u8* r = table.Data() + 6 + 12 * i;
                u16 platformId = Be16(r), encoding = Be16(r + 2), language = Be16(r + 4), nameId = Be16(r + 6), length = Be16(r + 8), offset = Be16(r + 10);
                if (nameId != id || usize(strings) + offset + length > table.Size())
                    continue;
                const u8* text = table.Data() + strings + offset;
                if (platformId == 3 && (encoding == 1 || encoding == 10) && (language == 0x409 || mac.Empty()))
                {
                    String utf8;
                    for (u32 k = 0; k + 1 < length; k += 2)
                    {
                        u32 c = Be16(text + k);
                        if (c >= 0xD800 && c <= 0xDBFF && k + 3 < length)
                        {
                            c = 0x10000 + ((c - 0xD800) << 10) + (Be16(text + k + 2) - 0xDC00);
                            k += 2;
                        }
                        char buf[4];
                        u32 n = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
                        if (n == 1)
                            buf[0] = char(c);
                        else if (n == 2)
                            buf[0] = char(0xC0 | (c >> 6)), buf[1] = char(0x80 | (c & 0x3F));
                        else if (n == 3)
                            buf[0] = char(0xE0 | (c >> 12)), buf[1] = char(0x80 | ((c >> 6) & 0x3F)), buf[2] = char(0x80 | (c & 0x3F));
                        else
                            buf[0] = char(0xF0 | (c >> 18)), buf[1] = char(0x80 | ((c >> 12) & 0x3F)), buf[2] = char(0x80 | ((c >> 6) & 0x3F)),
                            buf[3] = char(0x80 | (c & 0x3F));
                        utf8 += StringView(buf, n);
                    }
                    if (language == 0x409)
                        return utf8;
                    mac = utf8;
                }
                else if (platformId == 1 && encoding == 0 && mac.Empty())
                {
                    for (u32 k = 0; k < length; k++)
                        if (text[k] < 0x80)
                            mac += StringView(reinterpret_cast<const char*>(text + k), 1);
                }
            }
            return mac;
        }
    }

    void SystemFonts::AddFolder(StringView folder)
    {
        State& s = S();
        std::lock_guard<std::mutex> guard(s.lock);
        AddDefaults(s);
        s.folders.Push(String(folder));
        s.scanned = false;
    }

    Span<const String> SystemFonts::Folders()
    {
        State& s = S();
        std::lock_guard<std::mutex> guard(s.lock);
        AddDefaults(s);
        return Span<const String>(s.folders.Data(), s.folders.Size());
    }

    Span<const SystemFontFace> SystemFonts::Faces()
    {
        State& s = S();
        std::lock_guard<std::mutex> guard(s.lock);
        AddDefaults(s);
        if (!s.scanned)
        {
            s.scanned = true;
            s.faces.Clear();
            for (const String& folder : s.folders)
                Scan(folder.View(), s.faces, 0);
        }
        return Span<const SystemFontFace>(s.faces.Data(), s.faces.Size());
    }

    const SystemFontFace* SystemFonts::Find(StringView family, StringView style)
    {
        StringView wanted = style.Empty() ? StringView("Regular") : style;
        const SystemFontFace* first = nullptr;
        for (const SystemFontFace& f : Faces())
        {
            if (!SameText(f.family.View(), family))
                continue;
            if (SameText(f.style.View(), wanted))
                return &f;
            if (!first || (style.Empty() && (SameText(f.style.View(), "Normal") || SameText(f.style.View(), "Book"))))
                first = &f;
        }
        return first;
    }

    bool SystemFonts::ReadNames(StringView path, Array<SystemFontFace>& out)
    {
        platform::File f;
        if (!f.Open(path, platform::FileMode::Read))
            return false;
        Array<u8> head;
        if (!ReadAt(f, 0, 12, head))
            return false;
        Array<u32> offsets;
        if (Be32(head.Data()) == 0x74746366) // 'ttcf'
        {
            u32 count = Be32(head.Data() + 8);
            Array<u8> list;
            if (count > 4096 || !ReadAt(f, 12, usize(count) * 4, list))
                return false;
            for (u32 i = 0; i < count; i++)
                offsets.Push(Be32(list.Data() + 4 * i));
        }
        else
            offsets.Push(0);
        bool any = false;
        for (u32 index = 0; index < offsets.Size(); index++)
        {
            Array<u8> dir;
            if (!ReadAt(f, offsets[index], 12, dir))
                continue;
            u16 tables = Be16(dir.Data() + 4);
            Array<u8> records;
            if (!ReadAt(f, i64(offsets[index]) + 12, usize(tables) * 16, records))
                continue;
            for (u32 t = 0; t < tables; t++)
            {
                const u8* r = records.Data() + 16 * t;
                if (Be32(r) != 0x6E616D65) // 'name'
                    continue;
                u32 offset = Be32(r + 8), length = Be32(r + 12);
                Array<u8> table;
                if (length > (1u << 20) || !ReadAt(f, offset, length, table))
                    break;
                SystemFontFace face;
                face.path = String(path);
                face.index = index;
                face.family = NameOf(table, 16);
                if (face.family.Empty())
                    face.family = NameOf(table, 1);
                face.style = NameOf(table, 17);
                if (face.style.Empty())
                    face.style = NameOf(table, 2);
                if (!face.family.Empty())
                {
                    out.Push(Move(face));
                    any = true;
                }
                break;
            }
        }
        return any;
    }
}
