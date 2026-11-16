#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>

#include <memory>

// A small XML reader for HXML: elements, attributes (single or double quotes), text, comments, the declaration and
// the predefined and numeric character references. Namespace prefixes are kept in the names.
namespace helix::ui
{
    struct XmlAttribute
    {
        String name;
        String value;
    };

    struct XmlNode
    {
        String name;
        Array<XmlAttribute> attributes;
        Array<std::unique_ptr<XmlNode>> children;
        // The text inside the element (its parts joined, trimmed).
        String text;
        u32 line = 0, column = 0;

        // The value of an attribute (nullptr when missing).
        const String* Attribute(StringView name) const;
        // The name without its namespace prefix.
        StringView LocalName() const;
    };

    struct XmlError
    {
        u32 line = 0, column = 0;
        String message;
    };

    // Parses a document; null with the first error.
    std::unique_ptr<XmlNode> ParseXml(StringView text, XmlError& error);
}
