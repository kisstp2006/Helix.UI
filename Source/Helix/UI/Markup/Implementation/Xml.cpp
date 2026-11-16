#include <Helix/UI/Markup/Xml.h>

#include <Helix/Core/Strings/Format.h>

namespace helix::ui
{
    const String* XmlNode::Attribute(StringView n) const
    {
        for (const XmlAttribute& a : attributes)
            if (a.name.View() == n)
                return &a.value;
        return nullptr;
    }

    StringView XmlNode::LocalName() const
    {
        StringView n = name.View();
        usize colon = n.Find(':');
        return colon == StringView::npos ? n : n.Substr(colon + 1);
    }

    namespace
    {
        bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
        bool IsNameChar(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':';
        }

        class Reader
        {
        public:
            Reader(StringView text, XmlError& error) : text_(text), error_(error) {}

            std::unique_ptr<XmlNode> Document()
            {
                SkipMisc();
                if (!At('<'))
                    return Fail("an element expected");
                auto root = Element();
                if (!root)
                    return nullptr;
                SkipMisc();
                if (at_ < text_.Size())
                    return Fail("text after the root element");
                return root;
            }

        private:
            bool At(char c) const { return at_ < text_.Size() && text_[at_] == c; }
            bool At(StringView s) const { return text_.Substr(at_).StartsWith(s); }
            void Advance(usize n = 1)
            {
                for (usize i = 0; i < n && at_ < text_.Size(); i++)
                {
                    if (text_[at_] == '\n')
                        line_++, column_ = 1;
                    else
                        column_++;
                    at_++;
                }
            }
            void SkipSpace()
            {
                while (at_ < text_.Size() && IsSpace(text_[at_]))
                    Advance();
            }
            std::nullptr_t Fail(StringView message)
            {
                if (error_.message.Empty())
                {
                    error_.line = line_, error_.column = column_;
                    error_.message = message;
                }
                return nullptr;
            }

            // Comments, the declaration, processing instructions and space between elements.
            void SkipMisc()
            {
                for (;;)
                {
                    SkipSpace();
                    if (At("<!--"))
                    {
                        usize end = text_.Find("-->", at_ + 4);
                        Advance(end == StringView::npos ? text_.Size() - at_ : end + 3 - at_);
                    }
                    else if (At("<?"))
                    {
                        usize end = text_.Find("?>", at_ + 2);
                        Advance(end == StringView::npos ? text_.Size() - at_ : end + 2 - at_);
                    }
                    else
                        return;
                }
            }

            StringView Name()
            {
                usize start = at_;
                while (at_ < text_.Size() && IsNameChar(text_[at_]))
                    Advance();
                return text_.Substr(start, at_ - start);
            }

            // Text with its character references replaced.
            bool Decode(StringView raw, String& out)
            {
                for (usize i = 0; i < raw.Size(); i++)
                {
                    if (raw[i] != '&')
                    {
                        out.Append(raw[i]);
                        continue;
                    }
                    usize end = raw.Find(';', i);
                    if (end == StringView::npos)
                    {
                        Fail("a character reference without ';'");
                        return false;
                    }
                    StringView ref = raw.Substr(i + 1, end - i - 1);
                    if (ref == StringView("lt"))
                        out.Append('<');
                    else if (ref == StringView("gt"))
                        out.Append('>');
                    else if (ref == StringView("amp"))
                        out.Append('&');
                    else if (ref == StringView("quot"))
                        out.Append('"');
                    else if (ref == StringView("apos"))
                        out.Append('\'');
                    else if (ref.StartsWith("#"))
                    {
                        u32 code = 0;
                        bool hex = ref.Size() > 1 && (ref[1] == 'x' || ref[1] == 'X');
                        for (usize k = hex ? 2 : 1; k < ref.Size(); k++)
                        {
                            char c = ref[k];
                            u32 d = c >= '0' && c <= '9' ? u32(c - '0') : hex && c >= 'a' && c <= 'f' ? u32(c - 'a' + 10) : hex && c >= 'A' && c <= 'F' ? u32(c - 'A' + 10) : 99;
                            if (d > (hex ? 15u : 9u))
                            {
                                Fail("a bad numeric character reference");
                                return false;
                            }
                            code = code * (hex ? 16 : 10) + d;
                        }
                        // UTF-8.
                        if (code < 0x80)
                            out.Append(char(code));
                        else if (code < 0x800)
                            out.Append(char(0xC0 | (code >> 6))), out.Append(char(0x80 | (code & 0x3F)));
                        else if (code < 0x10000)
                            out.Append(char(0xE0 | (code >> 12))), out.Append(char(0x80 | ((code >> 6) & 0x3F))), out.Append(char(0x80 | (code & 0x3F)));
                        else
                            out.Append(char(0xF0 | (code >> 18))), out.Append(char(0x80 | ((code >> 12) & 0x3F))),
                                out.Append(char(0x80 | ((code >> 6) & 0x3F))), out.Append(char(0x80 | (code & 0x3F)));
                    }
                    else
                    {
                        Fail(Format("unknown character reference '&{};'", ref));
                        return false;
                    }
                    i = end;
                }
                return true;
            }

            std::unique_ptr<XmlNode> Element()
            {
                auto node = std::make_unique<XmlNode>();
                node->line = line_, node->column = column_;
                Advance();
                StringView name = Name();
                if (name.Empty())
                    return Fail("an element name expected after '<'");
                node->name = name;
                for (;;)
                {
                    SkipSpace();
                    if (At("/>"))
                    {
                        Advance(2);
                        return node;
                    }
                    if (At('>'))
                    {
                        Advance();
                        break;
                    }
                    StringView attribute = Name();
                    if (attribute.Empty())
                        return Fail(Format("an attribute or the end of <{}> expected", name));
                    SkipSpace();
                    if (!At('='))
                        return Fail(Format("'=' expected after the attribute {}", attribute));
                    Advance();
                    SkipSpace();
                    if (!At('"') && !At('\''))
                        return Fail(Format("the value of {} must be quoted", attribute));
                    char quote = text_[at_];
                    Advance();
                    usize start = at_;
                    while (at_ < text_.Size() && text_[at_] != quote)
                        Advance();
                    if (at_ >= text_.Size())
                        return Fail(Format("the value of {} is not closed", attribute));
                    XmlAttribute a;
                    a.name = attribute;
                    if (!Decode(text_.Substr(start, at_ - start), a.value))
                        return nullptr;
                    Advance();
                    for (const XmlAttribute& prior : node->attributes)
                        if (prior.name == a.name)
                            return Fail(Format("the attribute {} is given twice", attribute));
                    node->attributes.Push(Move(a));
                }
                // Content: text, comments and child elements up to the closing tag.
                String text;
                for (;;)
                {
                    if (at_ >= text_.Size())
                        return Fail(Format("<{}> is not closed", name));
                    if (At("<!--"))
                    {
                        SkipMisc();
                        continue;
                    }
                    // Character data as written (rich text tags in a label's text).
                    if (At("<![CDATA["))
                    {
                        usize end = text_.Find("]]>", at_ + 9);
                        if (end == StringView::npos)
                            return Fail("<![CDATA[ is not closed");
                        text.Append(text_.Substr(at_ + 9, end - at_ - 9));
                        Advance(end + 3 - at_);
                        continue;
                    }
                    if (At("</"))
                    {
                        Advance(2);
                        StringView closing = Name();
                        if (closing != name)
                            return Fail(Format("</{}> closes <{}>", closing, name));
                        SkipSpace();
                        if (!At('>'))
                            return Fail("'>' expected");
                        Advance();
                        node->text = text.View().Trimmed();
                        return node;
                    }
                    if (At('<'))
                    {
                        auto child = Element();
                        if (!child)
                            return nullptr;
                        node->children.Push(Move(child));
                        continue;
                    }
                    usize start = at_;
                    while (at_ < text_.Size() && !At('<'))
                        Advance();
                    if (!Decode(text_.Substr(start, at_ - start), text))
                        return nullptr;
                }
            }

            StringView text_;
            XmlError& error_;
            usize at_ = 0;
            u32 line_ = 1, column_ = 1;
        };
    }

    std::unique_ptr<XmlNode> ParseXml(StringView text, XmlError& error)
    {
        error = XmlError();
        Reader reader(text, error);
        return reader.Document();
    }
}
