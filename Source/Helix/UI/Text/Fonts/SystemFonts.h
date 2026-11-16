#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>

// The fonts of the machine: the font files (.ttf, .otf, .ttc, .otc) of registered folders, each face known by its family
// and style read from the file's name table (only the table directory and the name table are read). The folders of the
// platform are registered first (Windows: the system's and the user's Fonts; Linux: the usual font folders and the
// user's; Android: /system/fonts); more can be added. Nothing is scanned until a face is asked for.
namespace helix::ui
{
    struct SystemFontFace
    {
        String path;
        u32 index = 0;
        String family, style;
    };

    class SystemFonts
    {
    public:
        // A folder searched (with its subfolders) at the next scan.
        static void AddFolder(StringView folder);
        static Span<const String> Folders();
        // Every face of the folders (scanned on first use and after AddFolder).
        static Span<const SystemFontFace> Faces();
        // A face by family (case does not matter) and style ("Regular" when empty; the family's first face when the
        // style is missing); null when no face has the family.
        static const SystemFontFace* Find(StringView family, StringView style = StringView());
        // The family and style names of the faces of one file.
        static bool ReadNames(StringView path, Array<SystemFontFace>& out);
    };
}
