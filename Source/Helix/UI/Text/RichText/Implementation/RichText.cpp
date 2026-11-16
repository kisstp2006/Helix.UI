#include <Helix/UI/Text/RichText/RichText.h>

#include <cstdlib>

namespace helix::ui
{
    f32 TextLength::Resolve(f32 size, f32 reference, f32 current) const
    {
        switch (unit)
        {
        case Unit::Percent: return value * reference / 100.0f;
        case Unit::Em: return value * size;
        case Unit::Relative: return current + value;
        default: return value;
        }
    }

    bool TextStyle::operator==(const TextStyle& o) const
    {
        auto same = [](const TextLength& a, const TextLength& b) { return a.value == b.value && a.unit == b.unit; };
        return flags == o.flags && hasColor == o.hasColor && color == o.color && alpha == o.alpha && hasSize == o.hasSize && same(size, o.size) &&
               font == o.font && hasAlign == o.hasAlign && align == o.align && markColor == o.markColor && hasLineHeight == o.hasLineHeight &&
               same(lineHeight, o.lineHeight) && same(characterSpacing, o.characterSpacing) && same(monospace, o.monospace) && same(indent, o.indent) &&
               same(marginLeft, o.marginLeft) && same(marginRight, o.marginRight) && same(verticalOffset, o.verticalOffset) && same(width, o.width) &&
               monospaced == o.monospaced && hasWidth == o.hasWidth && rotate == o.rotate && link == o.link && gradient == o.gradient;
    }

    namespace
    {
        bool SameText(StringView a, StringView b)
        {
            if (a.Size() != b.Size())
                return false;
            for (usize i = 0; i < a.Size(); i++)
            {
                char x = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] + 32) : a[i];
                char y = b[i] >= 'A' && b[i] <= 'Z' ? char(b[i] + 32) : b[i];
                if (x != y)
                    return false;
            }
            return true;
        }

        StringView Trim(StringView s)
        {
            while (!s.Empty() && (s[0] == ' ' || s[0] == '"' || s[0] == '\''))
                s = s.Substr(1);
            while (!s.Empty() && (s[s.Size() - 1] == ' ' || s[s.Size() - 1] == '"' || s[s.Size() - 1] == '\''))
                s = s.Substr(0, s.Size() - 1);
            return s;
        }

        bool ParseLength(StringView text, TextLength& out, bool signRelative)
        {
            text = Trim(text);
            if (text.Empty())
                return false;
            out.unit = TextLength::Unit::Pixels;
            bool relative = signRelative && (text[0] == '+' || text[0] == '-');
            if (text.EndsWith("px"))
                text = text.Substr(0, text.Size() - 2);
            else if (text.EndsWith("%"))
            {
                out.unit = TextLength::Unit::Percent;
                text = text.Substr(0, text.Size() - 1);
            }
            else if (text.EndsWith("em"))
            {
                out.unit = TextLength::Unit::Em;
                text = text.Substr(0, text.Size() - 2);
            }
            String number(text);
            char* end = nullptr;
            out.value = std::strtof(number.Data(), &end);
            if (end == number.Data() || *end != 0)
                return false;
            if (relative && out.unit == TextLength::Unit::Pixels)
                out.unit = TextLength::Unit::Relative;
            return true;
        }

        u32 Hex(char c) { return c >= '0' && c <= '9' ? u32(c - '0') : c >= 'a' && c <= 'f' ? u32(c - 'a' + 10) : c >= 'A' && c <= 'F' ? u32(c - 'A' + 10) : 99; }

        struct Parser;

        // A tag: what opening it does to the style (false: the value is not one it takes) and what closing it puts back
        // from the style before it opened. Tags without a closing (null) stand alone.
        struct TagEntry
        {
            const char* name;
            bool (*open)(Parser& p, StringView value, StringView attributes);
            void (*close)(TextStyle& style, const TextStyle& before);
        };

        struct Opened
        {
            u32 tag;
            TextStyle before;
            String closing;
        };

        struct Parser
        {
            ParsedText& out;
            const TextStyleSheetLookup* sheets;
            TextStyle style;
            Array<Opened> opened;
            u32 source = 0;
            u32 depth = 0;
            u16 lastStyle = 0;

            u16 StyleIndex()
            {
                if (lastStyle < out.styles.Size() && out.styles[lastStyle] == style)
                    return lastStyle;
                for (u32 i = 0; i < out.styles.Size(); i++)
                    if (out.styles[i] == style)
                        return lastStyle = u16(i);
                out.styles.Push(style);
                return lastStyle = u16(out.styles.Size() - 1);
            }

            void Emit(TextElement e)
            {
                e.style = StyleIndex();
                e.source = source;
                out.text.Push(e.kind == TextElementKind::Character ? e.code : e.kind == TextElementKind::PageBreak ? char32_t(0xC) : char32_t(0xFFFC));
                if (style.link >= 0)
                    out.links[u32(style.link)].count = u32(out.elements.Size()) + 1 - out.links[u32(style.link)].firstElement;
                out.elements.Push(Move(e));
            }

            void Characters(StringView text, u32 base)
            {
                for (usize at = 0; at < text.Size();)
                {
                    source = base + u32(at);
                    TextElement e;
                    e.code = DecodeUtf8(text, at);
                    Emit(Move(e));
                }
            }

            void Run(StringView text, u32 base, bool rich);
        };

        void SetFlag(Parser& p, u32 f) { p.style.flags |= f; }
        void RestoreFlags(TextStyle& s, const TextStyle& b, u32 f) { s.flags = (s.flags & ~f) | (b.flags & f); }

        template <u32 F>
        bool OpenFlag(Parser& p, StringView, StringView)
        {
            SetFlag(p, F);
            return true;
        }
        template <u32 F>
        void CloseFlag(TextStyle& s, const TextStyle& b)
        {
            RestoreFlags(s, b, F);
        }

        bool OpenColor(Parser& p, StringView v, StringView)
        {
            Color c;
            if (!ParseTextColor(v, c))
                return false;
            p.style.color = c;
            p.style.hasColor = true;
            return true;
        }
        void CloseColor(TextStyle& s, const TextStyle& b)
        {
            s.color = b.color;
            s.hasColor = b.hasColor;
        }

        bool OpenAlpha(Parser& p, StringView v, StringView)
        {
            v = Trim(v);
            if (v.Size() != 3 || v[0] != '#' || Hex(v[1]) > 15 || Hex(v[2]) > 15)
                return false;
            p.style.alpha = f32(Hex(v[1]) * 16 + Hex(v[2])) / 255.0f;
            return true;
        }
        void CloseAlpha(TextStyle& s, const TextStyle& b) { s.alpha = b.alpha; }

        bool OpenSize(Parser& p, StringView v, StringView)
        {
            TextLength l;
            if (!ParseLength(v, l, true))
                return false;
            p.style.size = l;
            p.style.hasSize = true;
            return true;
        }
        void CloseSize(TextStyle& s, const TextStyle& b)
        {
            s.size = b.size;
            s.hasSize = b.hasSize;
        }

        bool OpenFont(Parser& p, StringView v, StringView)
        {
            v = Trim(v);
            if (v.Empty())
                return false;
            p.style.font = String(v);
            return true;
        }
        void CloseFont(TextStyle& s, const TextStyle& b) { s.font = b.font; }

        bool OpenAlign(Parser& p, StringView v, StringView)
        {
            static const struct
            {
                const char* name;
                TextAlignment align;
            } kAligns[] = {{"left", TextAlignment::Left},
                           {"center", TextAlignment::Center},
                           {"right", TextAlignment::Right},
                           {"justified", TextAlignment::Justified},
                           {"flush", TextAlignment::Flush}};
            for (const auto& a : kAligns)
                if (SameText(Trim(v), a.name))
                {
                    p.style.align = a.align;
                    p.style.hasAlign = true;
                    return true;
                }
            return false;
        }
        void CloseAlign(TextStyle& s, const TextStyle& b)
        {
            s.align = b.align;
            s.hasAlign = b.hasAlign;
        }

        bool OpenMark(Parser& p, StringView v, StringView)
        {
            if (!Trim(v).Empty() && !ParseTextColor(v, p.style.markColor))
                return false;
            SetFlag(p, TextFlags::Mark);
            return true;
        }
        void CloseMark(TextStyle& s, const TextStyle& b)
        {
            RestoreFlags(s, b, TextFlags::Mark);
            s.markColor = b.markColor;
        }

        bool OpenLineHeight(Parser& p, StringView v, StringView)
        {
            if (!ParseLength(v, p.style.lineHeight, false))
                return false;
            p.style.hasLineHeight = true;
            return true;
        }
        void CloseLineHeight(TextStyle& s, const TextStyle& b)
        {
            s.lineHeight = b.lineHeight;
            s.hasLineHeight = b.hasLineHeight;
        }

        // Tags of one length field.
        template <TextLength TextStyle::*Field>
        bool OpenLength(Parser& p, StringView v, StringView)
        {
            return ParseLength(v, p.style.*Field, false);
        }
        template <TextLength TextStyle::*Field>
        void CloseLength(TextStyle& s, const TextStyle& b)
        {
            s.*Field = b.*Field;
        }

        bool OpenMonospace(Parser& p, StringView v, StringView)
        {
            if (!ParseLength(v, p.style.monospace, false))
                return false;
            p.style.monospaced = true;
            return true;
        }
        void CloseMonospace(TextStyle& s, const TextStyle& b)
        {
            s.monospace = b.monospace;
            s.monospaced = b.monospaced;
        }

        bool OpenMargin(Parser& p, StringView v, StringView)
        {
            if (!ParseLength(v, p.style.marginLeft, false))
                return false;
            p.style.marginRight = p.style.marginLeft;
            return true;
        }
        void CloseMargin(TextStyle& s, const TextStyle& b)
        {
            s.marginLeft = b.marginLeft;
            s.marginRight = b.marginRight;
        }

        bool OpenWidth(Parser& p, StringView v, StringView)
        {
            if (!ParseLength(v, p.style.width, false))
                return false;
            p.style.hasWidth = true;
            return true;
        }
        void CloseWidth(TextStyle& s, const TextStyle& b)
        {
            s.width = b.width;
            s.hasWidth = b.hasWidth;
        }

        bool OpenRotate(Parser& p, StringView v, StringView)
        {
            TextLength l;
            if (!ParseLength(v, l, false))
                return false;
            p.style.rotate = l.value;
            return true;
        }
        void CloseRotate(TextStyle& s, const TextStyle& b) { s.rotate = b.rotate; }

        bool OpenLink(Parser& p, StringView v, StringView)
        {
            TextLink link;
            link.id = String(Trim(v));
            link.firstElement = u32(p.out.elements.Size());
            p.out.links.Push(Move(link));
            p.style.link = i32(p.out.links.Size() - 1);
            return true;
        }
        void CloseLink(TextStyle& s, const TextStyle& b) { s.link = b.link; }

        bool OpenGradient(Parser& p, StringView v, StringView)
        {
            v = Trim(v);
            if (v.Empty())
                return false;
            p.style.gradient = String(v);
            return true;
        }
        void CloseGradient(TextStyle& s, const TextStyle& b) { s.gradient = b.gradient; }

        // Standalone tags.
        bool OpenBreak(Parser& p, StringView, StringView)
        {
            TextElement e;
            e.code = '\n';
            p.Emit(Move(e));
            return true;
        }

        bool OpenSpace(Parser& p, StringView v, StringView)
        {
            TextElement e;
            e.kind = TextElementKind::Space;
            if (!ParseLength(v, e.length, false))
                return false;
            p.Emit(Move(e));
            return true;
        }

        bool OpenPosition(Parser& p, StringView v, StringView)
        {
            TextElement e;
            e.kind = TextElementKind::Position;
            if (!ParseLength(v, e.length, false))
                return false;
            p.Emit(Move(e));
            return true;
        }

        bool OpenPage(Parser& p, StringView, StringView)
        {
            TextElement e;
            e.kind = TextElementKind::PageBreak;
            p.Emit(Move(e));
            return true;
        }

        // An attribute of a tag ("name=value" among the others), empty when it is not there.
        StringView Attribute(StringView attributes, StringView name)
        {
            for (usize i = 0; i + name.Size() < attributes.Size(); i++)
            {
                if (!SameText(attributes.Substr(i, name.Size()), name) || attributes[i + name.Size()] != '=' || (i > 0 && attributes[i - 1] != ' '))
                    continue;
                usize s = i + name.Size() + 1, e = s;
                bool quoted = s < attributes.Size() && attributes[s] == '"';
                if (quoted)
                    for (e = s + 1; e < attributes.Size() && attributes[e] != '"'; e++)
                    {
                    }
                else
                    while (e < attributes.Size() && attributes[e] != ' ')
                        e++;
                return Trim(attributes.Substr(s, e - s + (quoted ? 1 : 0)));
            }
            return StringView();
        }

        bool OpenSprite(Parser& p, StringView v, StringView attributes)
        {
            TextElement e;
            e.kind = TextElementKind::Sprite;
            v = Trim(v);
            // <sprite=3>, <sprite="asset" index=3>, <sprite name="smile">.
            bool number = !v.Empty();
            for (char c : v)
                number = number && c >= '0' && c <= '9';
            if (number)
                e.spriteIndex = std::atoi(String(v).Data());
            else
                e.spriteAsset = String(v);
            if (StringView index = Attribute(attributes, "index"); !index.Empty())
                e.spriteIndex = std::atoi(String(index).Data());
            if (StringView name = Attribute(attributes, "name"); !name.Empty())
                e.spriteName = String(name);
            if (e.spriteIndex < 0 && e.spriteName.Empty())
                return false;
            p.Emit(Move(e));
            return true;
        }

        const TagEntry kTags[] = {
            {"b", &OpenFlag<TextFlags::Bold>, &CloseFlag<TextFlags::Bold>},
            {"i", &OpenFlag<TextFlags::Italic>, &CloseFlag<TextFlags::Italic>},
            {"u", &OpenFlag<TextFlags::Underline>, &CloseFlag<TextFlags::Underline>},
            {"s", &OpenFlag<TextFlags::Strikethrough>, &CloseFlag<TextFlags::Strikethrough>},
            {"lowercase", &OpenFlag<TextFlags::Lowercase>, &CloseFlag<TextFlags::Lowercase>},
            {"uppercase", &OpenFlag<TextFlags::Uppercase>, &CloseFlag<TextFlags::Uppercase>},
            {"allcaps", &OpenFlag<TextFlags::Uppercase>, &CloseFlag<TextFlags::Uppercase>},
            {"smallcaps", &OpenFlag<TextFlags::SmallCaps>, &CloseFlag<TextFlags::SmallCaps>},
            {"sup", &OpenFlag<TextFlags::Superscript>, &CloseFlag<TextFlags::Superscript>},
            {"sub", &OpenFlag<TextFlags::Subscript>, &CloseFlag<TextFlags::Subscript>},
            {"nobr", &OpenFlag<TextFlags::NoBreak>, &CloseFlag<TextFlags::NoBreak>},
            {"color", &OpenColor, &CloseColor},
            {"alpha", &OpenAlpha, &CloseAlpha},
            {"size", &OpenSize, &CloseSize},
            {"font", &OpenFont, &CloseFont},
            {"align", &OpenAlign, &CloseAlign},
            {"mark", &OpenMark, &CloseMark},
            {"line-height", &OpenLineHeight, &CloseLineHeight},
            {"cspace", &OpenLength<&TextStyle::characterSpacing>, &CloseLength<&TextStyle::characterSpacing>},
            {"mspace", &OpenMonospace, &CloseMonospace},
            {"indent", &OpenLength<&TextStyle::indent>, &CloseLength<&TextStyle::indent>},
            {"margin", &OpenMargin, &CloseMargin},
            {"margin-left", &OpenLength<&TextStyle::marginLeft>, &CloseLength<&TextStyle::marginLeft>},
            {"margin-right", &OpenLength<&TextStyle::marginRight>, &CloseLength<&TextStyle::marginRight>},
            {"voffset", &OpenLength<&TextStyle::verticalOffset>, &CloseLength<&TextStyle::verticalOffset>},
            {"width", &OpenWidth, &CloseWidth},
            {"rotate", &OpenRotate, &CloseRotate},
            {"link", &OpenLink, &CloseLink},
            {"gradient", &OpenGradient, &CloseGradient},
            {"br", &OpenBreak, nullptr},
            {"space", &OpenSpace, nullptr},
            {"pos", &OpenPosition, nullptr},
            {"page", &OpenPage, nullptr},
            {"sprite", &OpenSprite, nullptr},
        };
        constexpr u32 kTagCount = sizeof(kTags) / sizeof(kTags[0]);
        constexpr u32 kStyleTag = kTagCount;
        constexpr u32 kMaxStyleDepth = 8;

        void Parser::Run(StringView text, u32 base, bool rich)
        {
            usize plain = 0;
            for (usize i = 0; i < text.Size();)
            {
                if (!rich || text[i] != '<')
                {
                    i++;
                    continue;
                }
                usize end = i + 1;
                while (end < text.Size() && end - i < 256 && text[end] != '>' && text[end] != '<')
                    end++;
                if (end >= text.Size() || text[end] != '>')
                {
                    i++;
                    continue;
                }
                StringView inner = text.Substr(i + 1, end - i - 1);
                bool closing = !inner.Empty() && inner[0] == '/';
                if (closing)
                    inner = inner.Substr(1);
                // The name, its value after '=' and the other attributes after a space.
                usize n = 0;
                while (n < inner.Size() && inner[n] != '=' && inner[n] != ' ')
                    n++;
                StringView name = inner.Substr(0, n), value, attributes;
                if (n < inner.Size() && inner[n] == '=')
                {
                    usize v = n + 1, e = v;
                    bool quoted = v < inner.Size() && inner[v] == '"';
                    if (quoted)
                    {
                        for (e = v + 1; e < inner.Size() && inner[e] != '"'; e++)
                        {
                        }
                        e = e < inner.Size() ? e + 1 : e;
                    }
                    else
                        while (e < inner.Size() && inner[e] != ' ')
                            e++;
                    value = inner.Substr(v, e - v);
                    attributes = e < inner.Size() ? inner.Substr(e + 1) : StringView();
                }
                else if (n < inner.Size())
                    attributes = inner.Substr(n + 1);

                bool handled = false;
                if (!closing && SameText(name, "noparse"))
                {
                    // Everything up to </noparse> as written.
                    Characters(text.Substr(plain, i - plain), base + u32(plain));
                    usize stop = end + 1;
                    while (stop < text.Size() && !(text[stop] == '<' && SameText(text.Substr(stop, 10), "</noparse>")))
                        stop++;
                    Characters(text.Substr(end + 1, stop - end - 1), base + u32(end + 1));
                    i = plain = stop < text.Size() ? stop + 10 : stop;
                    continue;
                }
                u32 tag = kTagCount + 1;
                for (u32 t = 0; t < kTagCount; t++)
                    if (SameText(name, kTags[t].name))
                        tag = t;
                if (SameText(name, "style"))
                    tag = kStyleTag;
                // The text before the tag goes out with the style it had.
                if (tag <= kTagCount)
                    Characters(text.Substr(plain, i - plain), base + u32(plain));
                source = base + u32(i);
                if (tag == kStyleTag)
                {
                    if (!closing)
                    {
                        String opening, closingText;
                        if (sheets && depth < kMaxStyleDepth && sheets->Find(Trim(value), opening, closingText))
                        {
                            opened.Push({kStyleTag, style, closingText});
                            depth++;
                            Run(opening.View(), source, true);
                            depth--;
                            handled = true;
                        }
                    }
                    else
                        for (u32 k = u32(opened.Size()); k-- > 0;)
                            if (opened[k].tag == kStyleTag)
                            {
                                String closingText = Move(opened[k].closing);
                                opened.RemoveAt(k);
                                depth++;
                                Run(closingText.View(), source, true);
                                depth--;
                                handled = true;
                                break;
                            }
                }
                else if (tag < kTagCount)
                {
                    const TagEntry& entry = kTags[tag];
                    if (!closing)
                    {
                        TextStyle before = style;
                        if (entry.open(*this, value, attributes))
                        {
                            handled = true;
                            if (entry.close)
                                opened.Push({tag, before, String()});
                        }
                    }
                    else if (entry.close)
                        for (u32 k = u32(opened.Size()); k-- > 0;)
                            if (opened[k].tag == tag)
                            {
                                entry.close(style, opened[k].before);
                                opened.RemoveAt(k);
                                handled = true;
                                break;
                            }
                }
                if (handled)
                    plain = i = end + 1;
                else
                {
                    if (tag <= kTagCount)
                    {
                        // Not a tag after all: written out as text.
                        Characters(text.Substr(i, end + 1 - i), base + u32(i));
                        plain = end + 1;
                    }
                    i = end + 1;
                }
            }
            Characters(text.Substr(plain), base + u32(plain));
        }
    }

    void ParseRichText(StringView source, bool richText, const TextStyle& base, ParsedText& out, const TextStyleSheetLookup* styles)
    {
        out.elements.Clear();
        out.styles.Clear();
        out.links.Clear();
        out.text.Clear();
        Parser p{out, styles, base, Array<Opened>()};
        p.Run(source, 0, richText);
        if (out.styles.Empty())
            out.styles.Push(base);
    }

    bool ParseTextColor(StringView text, Color& out)
    {
        text = Trim(text);
        static const struct
        {
            const char* name;
            Color color;
        } kNames[] = {{"black", Color(0, 0, 0, 1)},          {"blue", Color(0, 0, 1, 1)},         {"green", Color(0, 0.5f, 0, 1)},
                      {"orange", Color(1, 0.502f, 0, 1)},    {"purple", Color(0.627f, 0.125f, 0.941f, 1)}, {"red", Color(1, 0, 0, 1)},
                      {"white", Color(1, 1, 1, 1)},          {"yellow", Color(1, 1, 0, 1)},       {"lightblue", Color(0.678f, 0.847f, 0.902f, 1)},
                      {"grey", Color(0.5f, 0.5f, 0.5f, 1)},  {"gray", Color(0.5f, 0.5f, 0.5f, 1)}};
        for (const auto& n : kNames)
            if (SameText(text, n.name))
            {
                out = n.color;
                return true;
            }
        if (text.Empty() || text[0] != '#')
            return false;
        StringView h = text.Substr(1);
        for (char c : h)
            if (Hex(c) > 15)
                return false;
        auto pair = [&](usize i) { return f32(Hex(h[i]) * 16 + Hex(h[i + 1])) / 255.0f; };
        auto single = [&](usize i) { return f32(Hex(h[i]) * 17) / 255.0f; };
        if (h.Size() == 3 || h.Size() == 4)
        {
            out = Color(single(0), single(1), single(2), h.Size() == 4 ? single(3) : 1);
            return true;
        }
        if (h.Size() == 6 || h.Size() == 8)
        {
            out = Color(pair(0), pair(2), pair(4), h.Size() == 8 ? pair(6) : 1);
            return true;
        }
        return false;
    }
}
