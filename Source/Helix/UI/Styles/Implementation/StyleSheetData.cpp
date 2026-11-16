#include <Helix/UI/Styles/StyleSheetData.h>

#include <Helix/Core/Strings/Format.h>

#include <cstdlib>

namespace helix::ui
{
    namespace
    {
        // ------------------------------------------------------------------------------------------------ tokens

        enum class Tok : u8
        {
            End,
            Ident,
            Function,
            Hash,
            String,
            Number,
            Percentage,
            Dimension,
            Colon,
            Semicolon,
            Comma,
            LeftBrace,
            RightBrace,
            RightParen,
            Delim,
            Space,
        };

        struct Token
        {
            Tok kind = Tok::End;
            StringView text;
            StringView unit;
            f32 number = 0;
            u32 line = 1, column = 1;
            usize offset = 0;
        };

        bool IsNameStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-'; }
        bool IsName(char c) { return IsNameStart(c) || (c >= '0' && c <= '9'); }
        bool IsDigit(char c) { return c >= '0' && c <= '9'; }
        bool IsSpaceChar(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

        class Lexer
        {
        public:
            explicit Lexer(StringView text) : text_(text) {}

            Token Next()
            {
                SkipComments();
                Token t;
                t.line = line_, t.column = column_, t.offset = at_;
                if (at_ >= text_.Size())
                    return t;
                char c = text_[at_];
                if (IsSpaceChar(c))
                {
                    while (at_ < text_.Size() && IsSpaceChar(text_[at_]))
                        Advance();
                    t.kind = Tok::Space;
                    return t;
                }
                if (IsDigit(c) || (c == '.' && at_ + 1 < text_.Size() && IsDigit(text_[at_ + 1])) ||
                    ((c == '-' || c == '+') && at_ + 1 < text_.Size() && (IsDigit(text_[at_ + 1]) || text_[at_ + 1] == '.')))
                    return NumberToken(t);
                if (IsNameStart(c))
                {
                    usize start = at_;
                    while (at_ < text_.Size() && IsName(text_[at_]))
                        Advance();
                    t.text = text_.Substr(start, at_ - start);
                    if (at_ < text_.Size() && text_[at_] == '(')
                    {
                        Advance();
                        t.kind = Tok::Function;
                    }
                    else
                        t.kind = Tok::Ident;
                    return t;
                }
                if (c == '#')
                {
                    Advance();
                    usize start = at_;
                    while (at_ < text_.Size() && IsName(text_[at_]))
                        Advance();
                    t.kind = Tok::Hash;
                    t.text = text_.Substr(start, at_ - start);
                    return t;
                }
                if (c == '"' || c == '\'')
                {
                    Advance();
                    usize start = at_;
                    while (at_ < text_.Size() && text_[at_] != c)
                        Advance();
                    t.kind = Tok::String;
                    t.text = text_.Substr(start, at_ - start);
                    if (at_ < text_.Size())
                        Advance();
                    return t;
                }
                Advance();
                t.text = text_.Substr(t.offset, 1);
                switch (c)
                {
                case ':': t.kind = Tok::Colon; break;
                case ';': t.kind = Tok::Semicolon; break;
                case ',': t.kind = Tok::Comma; break;
                case '{': t.kind = Tok::LeftBrace; break;
                case '}': t.kind = Tok::RightBrace; break;
                case ')': t.kind = Tok::RightParen; break;
                default: t.kind = Tok::Delim; break;
                }
                return t;
            }

            usize Offset() const { return at_; }
            StringView Text() const { return text_; }

        private:
            void Advance()
            {
                if (text_[at_] == '\n')
                    line_++, column_ = 1;
                else
                    column_++;
                at_++;
            }

            void SkipComments()
            {
                while (at_ + 1 < text_.Size() && text_[at_] == '/' && text_[at_ + 1] == '*')
                {
                    Advance(), Advance();
                    while (at_ + 1 < text_.Size() && !(text_[at_] == '*' && text_[at_ + 1] == '/'))
                        Advance();
                    if (at_ + 1 < text_.Size())
                        Advance(), Advance();
                    else
                        at_ = text_.Size();
                }
            }

            Token NumberToken(Token t)
            {
                usize start = at_;
                if (text_[at_] == '-' || text_[at_] == '+')
                    Advance();
                while (at_ < text_.Size() && (IsDigit(text_[at_]) || text_[at_] == '.'))
                    Advance();
                char buffer[64];
                usize n = at_ - start < 63 ? at_ - start : 63;
                for (usize i = 0; i < n; i++)
                    buffer[i] = text_[start + i];
                buffer[n] = 0;
                t.number = f32(std::strtod(buffer, nullptr));
                t.text = text_.Substr(start, at_ - start);
                if (at_ < text_.Size() && text_[at_] == '%')
                {
                    Advance();
                    t.kind = Tok::Percentage;
                }
                else if (at_ < text_.Size() && IsNameStart(text_[at_]))
                {
                    usize u = at_;
                    while (at_ < text_.Size() && IsName(text_[at_]))
                        Advance();
                    t.kind = Tok::Dimension;
                    t.unit = text_.Substr(u, at_ - u);
                }
                else
                    t.kind = Tok::Number;
                return t;
            }

            StringView text_;
            usize at_ = 0;
            u32 line_ = 1, column_ = 1;
        };

        // The tokens of a value (no spaces kept; a function's arguments follow it up to its closing parenthesis).
        void Tokenize(StringView text, Array<Token>& out)
        {
            Lexer lex(text);
            for (;;)
            {
                Token t = lex.Next();
                if (t.kind == Tok::End)
                    return;
                if (t.kind != Tok::Space)
                    out.Push(t);
            }
        }

        // ------------------------------------------------------------------------------------------------ values

        struct NamedColor
        {
            StringView name;
            u32 rgb;
        };
        // The basic CSS color names.
        const NamedColor kColors[] = {
            {"black", 0x000000},   {"white", 0xFFFFFF},  {"red", 0xFF0000},    {"green", 0x008000},  {"lime", 0x00FF00},
            {"blue", 0x0000FF},    {"yellow", 0xFFFF00}, {"cyan", 0x00FFFF},   {"aqua", 0x00FFFF},   {"magenta", 0xFF00FF},
            {"fuchsia", 0xFF00FF}, {"gray", 0x808080},   {"grey", 0x808080},   {"silver", 0xC0C0C0}, {"maroon", 0x800000},
            {"olive", 0x808000},   {"navy", 0x000080},   {"purple", 0x800080}, {"teal", 0x008080},   {"orange", 0xFFA500},
            {"pink", 0xFFC0CB},    {"brown", 0xA52A2A},  {"gold", 0xFFD700},   {"indigo", 0x4B0082}, {"violet", 0xEE82EE},
        };

        u32 Hex(char c) { return c >= '0' && c <= '9' ? u32(c - '0') : c >= 'a' && c <= 'f' ? u32(c - 'a' + 10) : c >= 'A' && c <= 'F' ? u32(c - 'A' + 10) : 99; }

        bool HexColor(StringView h, Color& out)
        {
            u32 d[8];
            for (usize i = 0; i < h.Size(); i++)
                if (i >= 8 || (d[i] = Hex(h[i])) > 15)
                    return false;
            auto pair = [&](u32 i) { return f32(d[i] * 16 + d[i + 1]) / 255.0f; };
            auto single = [&](u32 i) { return f32(d[i] * 17) / 255.0f; };
            switch (h.Size())
            {
            case 3: out = Color(single(0), single(1), single(2), 1); return true;
            case 4: out = Color(single(0), single(1), single(2), single(3)); return true;
            case 6: out = Color(pair(0), pair(2), pair(4), 1); return true;
            case 8: out = Color(pair(0), pair(2), pair(4), pair(6)); return true;
            default: return false;
            }
        }

        bool AngleToken(const Token& k, StyleValue& v)
        {
            static const StringView kUnits[] = {"deg", "grad", "rad", "turn"};
            if (k.kind == Tok::Number && k.number == 0)
            {
                v = StyleValue::OfAngle(0, u32(AngleUnit::Degree));
                return true;
            }
            if (k.kind == Tok::Ident && k.text == StringView("none"))
            {
                v = StyleValue::OfAngle(0, u32(AngleUnit::Degree));
                return true;
            }
            if (k.kind == Tok::Dimension)
                for (u32 u = 0; u < 4; u++)
                    if (k.unit == kUnits[u])
                    {
                        v = StyleValue::OfAngle(k.number, u);
                        return true;
                    }
            return false;
        }

        // A length (px, %, a bare 0) or false.
        bool LengthToken(const Token& k, Length& out)
        {
            if (k.kind == Tok::Number && k.number == 0)
                out = Length::Px(0);
            else if (k.kind == Tok::Dimension && k.unit == StringView("px"))
                out = Length::Px(k.number);
            else if (k.kind == Tok::Percentage)
                out = Length::Percent(k.number);
            else
                return false;
            return true;
        }

        // One value of a type from tokens[at...]; at moves past it.
        bool ParseOne(const PropertyInfo& p, const Array<Token>& t, usize& at, StyleValue& v, String& error)
        {
            if (at >= t.Size())
            {
                error = "a value is missing";
                return false;
            }
            const Token& k = t[at];
            switch (p.type)
            {
            case ValueType::Length:
                if (k.kind == Tok::Number && k.number == 0)
                    v = StyleValue::OfLength(Length::Px(0));
                else if (k.kind == Tok::Dimension && k.unit == StringView("px"))
                    v = StyleValue::OfLength(Length::Px(k.number));
                else if (k.kind == Tok::Percentage)
                    v = StyleValue::OfLength(Length::Percent(k.number));
                else if (k.kind == Tok::Ident && (k.text == StringView("auto") || k.text == StringView("none")))
                    v = StyleValue::OfLength(Length::Auto());
                else
                {
                    error = Format("'{}' is not a length (px, %, auto)", k.text);
                    return false;
                }
                break;
            case ValueType::Number:
                if (k.kind != Tok::Number)
                {
                    error = Format("'{}' is not a number", k.text);
                    return false;
                }
                v = StyleValue::OfNumber(k.number);
                break;
            case ValueType::Time:
                if (k.kind == Tok::Dimension && k.unit == StringView("s"))
                    v = StyleValue::OfTime(k.number);
                else if (k.kind == Tok::Dimension && k.unit == StringView("ms"))
                    v = StyleValue::OfTime(k.number * 0.001f);
                else if (k.kind == Tok::Number && k.number == 0)
                    v = StyleValue::OfTime(0);
                else
                {
                    error = Format("'{}' is not a time (s, ms)", k.text);
                    return false;
                }
                break;
            case ValueType::Color:
            {
                Color c;
                if (k.kind == Tok::Hash && HexColor(k.text, c))
                {
                    v = StyleValue::OfColor(c);
                    break;
                }
                if (k.kind == Tok::Ident)
                {
                    if (k.text == StringView("transparent"))
                    {
                        v = StyleValue::OfColor(Color(0, 0, 0, 0));
                        break;
                    }
                    for (const NamedColor& n : kColors)
                        if (n.name == k.text)
                        {
                            v = StyleValue::OfColor(Color(f32((n.rgb >> 16) & 255) / 255.0f, f32((n.rgb >> 8) & 255) / 255.0f, f32(n.rgb & 255) / 255.0f, 1));
                            break;
                        }
                    if (v.kind == StyleValue::Kind::Color)
                        break;
                }
                if (k.kind == Tok::Function && (k.text == StringView("rgb") || k.text == StringView("rgba")))
                {
                    f32 c4[4] = {0, 0, 0, 1};
                    u32 n = 0;
                    usize i = at + 1;
                    for (; i < t.Size() && t[i].kind != Tok::RightParen; i++)
                        if ((t[i].kind == Tok::Number || t[i].kind == Tok::Percentage) && n < 4)
                        {
                            f32 x = t[i].number;
                            c4[n] = n < 3 ? (t[i].kind == Tok::Percentage ? x / 100.0f : x / 255.0f) : (t[i].kind == Tok::Percentage ? x / 100.0f : x);
                            n++;
                        }
                    if (n < 3 || i >= t.Size())
                    {
                        error = "rgb() needs three numbers";
                        return false;
                    }
                    v = StyleValue::OfColor(Color(c4[0], c4[1], c4[2], c4[3]));
                    at = i;
                    break;
                }
                error = Format("'{}' is not a color", k.text);
                return false;
            }
            case ValueType::Enum:
            {
                if (k.kind == Tok::Ident)
                    for (u32 i = 0; i < p.keywords.Size(); i++)
                        if (p.keywords[i] == k.text)
                        {
                            v = StyleValue::OfKeyword(i);
                            break;
                        }
                if (v.kind != StyleValue::Kind::Keyword)
                {
                    error = Format("'{}' is not a value of {}", k.text, p.name);
                    return false;
                }
                break;
            }
            case ValueType::Resource:
                if (k.kind == Tok::Function && (k.text == StringView("url") || k.text == StringView("resource")) && at + 1 < t.Size())
                {
                    const Token& arg = t[at + 1];
                    v.kind = StyleValue::Kind::Resource;
                    v.resource = Intern(arg.text);
                    at += 2;
                    if (at >= t.Size() || t[at].kind != Tok::RightParen)
                    {
                        error = "url() and resource() take one path";
                        return false;
                    }
                    break;
                }
                if (k.kind == Tok::Ident && k.text == StringView("none"))
                {
                    v.kind = StyleValue::Kind::Resource;
                    v.resource = kNoAtom;
                    break;
                }
                error = Format("'{}' is not url() or resource()", k.text);
                return false;
            case ValueType::Name:
                if (k.kind != Tok::String && k.kind != Tok::Ident)
                {
                    error = Format("'{}' is not a name (quote names with spaces)", k.text);
                    return false;
                }
                v.kind = StyleValue::Kind::Resource;
                v.resource = Intern(k.text);
                break;
            case ValueType::PropertyName:
                if (k.kind != Tok::Ident)
                {
                    error = Format("'{}' is not a property name", k.text);
                    return false;
                }
                v = StyleValue::OfKeyword(k.text == StringView("all") ? kNoAtom : Intern(k.text));
                break;
            case ValueType::Angle:
                if (!AngleToken(k, v))
                {
                    error = Format("'{}' is not an angle (deg, grad, rad, turn)", k.text);
                    return false;
                }
                break;
            case ValueType::Compound:
                error = Format("{} has no single value", p.name);
                return false;
            }
            at++;
            return true;
        }

        bool ParseTokens(const PropertyInfo& p, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            // `none` for a property of several values: its initial values.
            if (p.count > 1 && t.Size() == 1 && t[0].kind == Tok::Ident && t[0].text == StringView("none"))
            {
                static const ComputedStyle initial;
                StyleValue values[kMaxValues];
                u32 n = GetValues(p, initial, values);
                for (u32 i = 0; i < n; i++)
                    out.Push(values[i]);
                return true;
            }
            usize at = 0;
            usize first = out.Size();
            for (;;)
            {
                StyleValue v;
                if (!ParseOne(p, t, at, v, error))
                    return false;
                out.Push(v);
                if (at >= t.Size())
                    return true;
                if (p.list && t[at].kind == Tok::Comma)
                {
                    at++;
                    continue;
                }
                // Values separated by spaces, up to the property's count (a third value of the reference's 3D
                // forms, z, is read and not used).
                if (out.Size() - first < p.count)
                    continue;
                error = Format("'{}' after the value of {}", t[at].text, p.name);
                return false;
            }
        }

        // ------------------------------------------------------------------------------- own value parsers

        // Properties whose values are no plain list of one type parse them here.
        using ValueParser = bool (*)(const PropertyInfo& p, const Array<Token>& t, Array<StyleValue>& out, String& error);

        bool KeywordIndex(Span<const StringView> words, const Token& k, u32& out)
        {
            if (k.kind != Tok::Ident)
                return false;
            for (u32 i = 0; i < words.Size(); i++)
                if (words[i] == k.text)
                {
                    out = i;
                    return true;
                }
            return false;
        }

        // A place on an axis: a keyword (left, center, right, top, bottom) as a percentage, or a length; the
        // keyword's axis (0 x, 1 y, 2 either) in axis.
        bool PlaceToken(const Token& k, Length& out, u32& axis)
        {
            axis = 2;
            if (LengthToken(k, out))
                return true;
            if (k.kind != Tok::Ident)
                return false;
            if (k.text == StringView("left") || k.text == StringView("top"))
                out = Length::Percent(0);
            else if (k.text == StringView("center"))
                out = Length::Percent(50);
            else if (k.text == StringView("right") || k.text == StringView("bottom"))
                out = Length::Percent(100);
            else
                return false;
            axis = k.text == StringView("left") || k.text == StringView("right") ? 0u : k.text == StringView("center") ? 2u : 1u;
            return true;
        }

        bool TransformOrigin(const PropertyInfo&, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            if (t.Empty() || t.Size() > 3)
            {
                error = "one to three values";
                return false;
            }
            Length v[2] = {Length::Percent(50), Length::Percent(50)};
            u32 axes[2] = {2, 2};
            for (usize i = 0; i < t.Size() && i < 2; i++)
                if (!PlaceToken(t[i], v[i], axes[i]))
                {
                    error = Format("'{}' is no place (left, center, right, top, bottom, a length)", t[i].text);
                    return false;
                }
            if (t.Size() == 3)
            {
                Length z;
                if (!LengthToken(t[2], z) || z.unit == Length::Unit::Percent)
                {
                    error = "the third value is a length in pixels";
                    return false;
                }
            }
            if (t.Size() == 1)
            {
                // One keyword of the y axis places y; anything else is x.
                if (axes[0] == 1)
                    v[1] = v[0], v[0] = Length::Percent(50);
            }
            else if (axes[0] == 1 || axes[1] == 0)
            {
                Length swap = v[0];
                v[0] = v[1], v[1] = swap;
            }
            out.Push(StyleValue::OfLength(v[0]));
            out.Push(StyleValue::OfLength(v[1]));
            return true;
        }

        // background-position-x / -y: a keyword of the axis and an offset from it, or an offset from the start.
        bool PositionAxis(const PropertyInfo& p, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            bool vertical = p.name == StringView("background-position-y");
            u32 keyword = u32(vertical ? BackgroundPositionKeyword::Top : BackgroundPositionKeyword::Left);
            Length offset = Length::Px(0);
            usize at = 0;
            if (at < t.Size() && KeywordIndex(p.keywords, t[at], keyword))
            {
                BackgroundPositionKeyword k = BackgroundPositionKeyword(keyword);
                bool fits = k == BackgroundPositionKeyword::Center ||
                            (vertical ? k == BackgroundPositionKeyword::Top || k == BackgroundPositionKeyword::Bottom
                                      : k == BackgroundPositionKeyword::Left || k == BackgroundPositionKeyword::Right);
                if (!fits)
                {
                    error = Format("'{}' is no keyword of {}", t[at].text, p.name);
                    return false;
                }
                at++;
            }
            if (at < t.Size())
            {
                if (!LengthToken(t[at], offset))
                {
                    error = Format("'{}' is not a length", t[at].text);
                    return false;
                }
                at++;
            }
            if (at == 0 || at < t.Size())
            {
                error = "a keyword and an offset";
                return false;
            }
            out.Push(StyleValue::OfKeyword(keyword));
            out.Push(StyleValue::OfLength(offset));
            return true;
        }

        bool Repeat(const PropertyInfo& p, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            if (t.Size() == 1 && t[0].kind == Tok::Ident && (t[0].text == StringView("repeat-x") || t[0].text == StringView("repeat-y")))
            {
                bool x = t[0].text == StringView("repeat-x");
                out.Push(StyleValue::OfKeyword(u32(x ? BackgroundRepeat::Repeat : BackgroundRepeat::NoRepeat)));
                out.Push(StyleValue::OfKeyword(u32(x ? BackgroundRepeat::NoRepeat : BackgroundRepeat::Repeat)));
                return true;
            }
            if (t.Empty() || t.Size() > 2)
            {
                error = "one or two values";
                return false;
            }
            for (const Token& k : t)
            {
                u32 i = 0;
                if (!KeywordIndex(p.keywords, k, i))
                {
                    error = Format("'{}' is not a value of {}", k.text, p.name);
                    return false;
                }
                out.Push(StyleValue::OfKeyword(i));
            }
            return true;
        }

        bool Size(const PropertyInfo&, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            if (t.Size() == 1 && t[0].kind == Tok::Ident && (t[0].text == StringView("cover") || t[0].text == StringView("contain")))
            {
                out.Push(StyleValue::OfKeyword(u32(t[0].text == StringView("cover") ? BackgroundSizeType::Cover : BackgroundSizeType::Contain)));
                out.Push(StyleValue::OfLength(Length::Auto()));
                out.Push(StyleValue::OfLength(Length::Auto()));
                return true;
            }
            if (t.Empty() || t.Size() > 2)
            {
                error = "cover, contain or one or two lengths";
                return false;
            }
            out.Push(StyleValue::OfKeyword(u32(BackgroundSizeType::Length)));
            for (const Token& k : t)
            {
                Length l;
                if (k.kind == Tok::Ident && k.text == StringView("auto"))
                    l = Length::Auto();
                else if (!LengthToken(k, l))
                {
                    error = Format("'{}' is not a length", k.text);
                    return false;
                }
                out.Push(StyleValue::OfLength(l));
            }
            if (t.Size() == 1)
                out.Push(StyleValue::OfLength(Length::Auto()));
            return true;
        }

        bool TextShadow(const PropertyInfo&, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            if (t.Size() == 1 && t[0].kind == Tok::Ident && t[0].text == StringView("none"))
            {
                for (u32 i = 0; i < 3; i++)
                    out.Push(StyleValue::OfLength(Length::Px(0)));
                out.Push(StyleValue::OfColor(Color(0, 0, 0, 0)));
                return true;
            }
            // Two or three lengths in pixels, a color before or after them.
            static const PropertyInfo colorInfo{"color", ValueType::Color, false, false, false, {}, nullptr, nullptr};
            f32 lengths[3] = {0, 0, 0};
            u32 n = 0;
            StyleValue color = StyleValue::OfColor(Color(0, 0, 0, 1));
            bool colored = false;
            usize at = 0;
            while (at < t.Size())
            {
                Length l;
                if (LengthToken(t[at], l) && l.unit == Length::Unit::Pixel)
                {
                    if (n >= 3)
                    {
                        error = "at most three lengths";
                        return false;
                    }
                    lengths[n++] = l.value;
                    at++;
                    continue;
                }
                if (colored || !ParseOne(colorInfo, t, at, color, error))
                {
                    if (colored)
                        error = Format("'{}' after the shadow's color", t[at].text);
                    return false;
                }
                colored = true;
            }
            if (n < 2)
            {
                error = "an offset of two lengths";
                return false;
            }
            for (f32 l : lengths)
                out.Push(StyleValue::OfLength(Length::Px(l)));
            out.Push(color);
            return true;
        }

        bool CursorValue(const PropertyInfo& p, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            u32 keyword = 0;
            if (t.Size() == 1 && KeywordIndex(p.keywords, t[0], keyword))
            {
                out.Push(StyleValue::OfKeyword(keyword));
                return true;
            }
            static const PropertyInfo resourceInfo{"cursor", ValueType::Resource, false, false, false, {}, nullptr, nullptr};
            usize at = 0;
            StyleValue image;
            if (!ParseOne(resourceInfo, t, at, image, error))
            {
                error = Format("'{}' is no cursor (url() or resource() with a hotspot, or a keyword)", t.Empty() ? StringView() : t[0].text);
                return false;
            }
            out.Push(image);
            f32 hotspot[2] = {0, 0};
            u32 n = 0;
            for (; at < t.Size(); at++)
            {
                if (t[at].kind != Tok::Number || n >= 2)
                {
                    error = "the hotspot is two numbers";
                    return false;
                }
                hotspot[n++] = t[at].number;
            }
            if (n == 1)
            {
                error = "the hotspot is two numbers";
                return false;
            }
            out.Push(StyleValue::OfNumber(hotspot[0]));
            out.Push(StyleValue::OfNumber(hotspot[1]));
            return true;
        }

        struct OwnParser
        {
            StringView property;
            ValueParser parse;
        };

        const OwnParser kOwnParsers[] = {
            {"transform-origin", TransformOrigin}, {"background-position-x", PositionAxis}, {"background-position-y", PositionAxis},
            {"background-repeat", Repeat},         {"background-size", Size},               {"text-shadow", TextShadow},
            {"cursor", CursorValue},
        };

        // A property's values from tokens: its own parser, or values of its type.
        bool ParseProperty(const PropertyInfo& p, const Array<Token>& t, Array<StyleValue>& out, String& error)
        {
            for (const OwnParser& own : kOwnParsers)
                if (own.property == p.name)
                    return own.parse(p, t, out, error);
            return ParseTokens(p, t, out, error);
        }

        // ------------------------------------------------------------------------------------------------ shorthands

        struct Longhand
        {
            StringView property;
            StyleValue values[kMaxValues];
            u32 count = 1;

            Longhand(StringView p, const StyleValue& v) : property(p) { values[0] = v; }
            Longhand(StringView p, Span<const StyleValue> v) : property(p), count(u32(v.Size() < kMaxValues ? v.Size() : kMaxValues))
            {
                for (u32 i = 0; i < count; i++)
                    values[i] = v[i];
            }
        };

        // Four sides from one to four values (top, right, bottom, left as in CSS), written to longhands ordered left,
        // top, right, bottom.
        bool Sides(const StringView (&names)[4], const Array<Token>& t, Array<Longhand>& out, String& error)
        {
            const PropertyInfo& p = Property(FindProperty(names[0]));
            Array<StyleValue> v;
            usize at = 0;
            while (at < t.Size())
            {
                StyleValue x;
                if (!ParseOne(p, t, at, x, error))
                    return false;
                v.Push(x);
            }
            if (v.Empty() || v.Size() > 4)
            {
                error = "one to four values";
                return false;
            }
            StyleValue top = v[0], right = v.Size() > 1 ? v[1] : top, bottom = v.Size() > 2 ? v[2] : top, left = v.Size() > 3 ? v[3] : right;
            out.Push({names[0], left});
            out.Push({names[1], top});
            out.Push({names[2], right});
            out.Push({names[3], bottom});
            return true;
        }

        const StringView kMargins[4] = {"margin-left", "margin-top", "margin-right", "margin-bottom"};
        const StringView kPaddings[4] = {"padding-left", "padding-top", "padding-right", "padding-bottom"};
        const StringView kBorderWidths[4] = {"border-left-width", "border-top-width", "border-right-width", "border-bottom-width"};
        const StringView kBorderColors[4] = {"border-left-color", "border-top-color", "border-right-color", "border-bottom-color"};
        // In the order the shorthand's sides map: left is the bottom left corner (CSS's fourth value).
        const StringView kRadii[4] = {"border-bottom-left-radius", "border-top-left-radius", "border-top-right-radius", "border-bottom-right-radius"};

        using Expander = bool (*)(const Array<Token>&, Array<Longhand>&, String&);
        struct Shorthand
        {
            StringView name;
            Expander expand;
        };

        bool Flex(const Array<Token>& t, Array<Longhand>& out, String& error)
        {
            // flex: none | <grow> [<shrink>] [<basis>] | <basis>
            if (t.Size() == 1 && t[0].kind == Tok::Ident && t[0].text == StringView("none"))
            {
                out.Push({"flex-grow", StyleValue::OfNumber(0)});
                out.Push({"flex-shrink", StyleValue::OfNumber(0)});
                out.Push({"flex-basis", StyleValue::OfLength(Length::Auto())});
                return true;
            }
            f32 numbers[2] = {1, 1};
            u32 n = 0;
            StyleValue basis = StyleValue::OfLength(Length::Percent(0));
            usize at = 0;
            const PropertyInfo& b = Property(FindProperty("flex-basis"));
            while (at < t.Size())
            {
                if (t[at].kind == Tok::Number && n < 2)
                {
                    numbers[n++] = t[at].number;
                    at++;
                    continue;
                }
                if (!ParseOne(b, t, at, basis, error))
                    return false;
            }
            if (n == 0)
                numbers[0] = 1;
            out.Push({"flex-grow", StyleValue::OfNumber(numbers[0])});
            out.Push({"flex-shrink", StyleValue::OfNumber(n > 1 ? numbers[1] : 1)});
            out.Push({"flex-basis", basis});
            return true;
        }

        bool Transition(const Array<Token>& t, Array<Longhand>& out, String& error)
        {
            // transition: <property> <duration> [<easing>] [<delay>], ...
            const PropertyInfo& names = Property(FindProperty("transition-property"));
            const PropertyInfo& times = Property(FindProperty("transition-duration"));
            const PropertyInfo& easings = Property(FindProperty("transition-timing-function"));
            usize at = 0;
            while (at < t.Size())
            {
                StyleValue property = StyleValue::OfKeyword(kNoAtom), duration = StyleValue::OfTime(0), delay = StyleValue::OfTime(0),
                           easing = StyleValue::OfKeyword(0);
                u32 timesSeen = 0;
                while (at < t.Size() && t[at].kind != Tok::Comma)
                {
                    StyleValue v;
                    String ignored;
                    usize back = at;
                    if (t[at].kind == Tok::Dimension || (t[at].kind == Tok::Number && t[at].number == 0))
                    {
                        if (!ParseOne(times, t, at, v, error))
                            return false;
                        (timesSeen++ == 0 ? duration : delay) = v;
                        continue;
                    }
                    if (ParseOne(easings, t, at, v, ignored))
                    {
                        easing = v;
                        continue;
                    }
                    at = back;
                    if (!ParseOne(names, t, at, v, error))
                        return false;
                    property = v;
                }
                out.Push({"transition-property", property});
                out.Push({"transition-duration", duration});
                out.Push({"transition-timing-function", easing});
                out.Push({"transition-delay", delay});
                if (at < t.Size())
                    at++;
            }
            return true;
        }

        // background-position: one to four values (keywords and offsets) for the x and y longhands.
        bool BackgroundPosition(const Array<Token>& t, Array<Longhand>& out, String& error)
        {
            struct Item
            {
                bool keyword = false;
                bool paired = false;
                u32 axis = 2;
                BackgroundPositionKeyword k = BackgroundPositionKeyword::Center;
                Length offset = Length::Px(0);
            };
            Array<Item> items;
            for (usize at = 0; at < t.Size(); at++)
            {
                Item item;
                const Token& k = t[at];
                if (LengthToken(k, item.offset))
                {
                    // In the forms of three and four values an offset after a keyword (not center) belongs to it.
                    if (t.Size() > 2 && !items.Empty() && items.Back().keyword && !items.Back().paired &&
                        items.Back().k != BackgroundPositionKeyword::Center)
                    {
                        items.Back().offset = item.offset;
                        items.Back().paired = true;
                        continue;
                    }
                    items.Push(item);
                    continue;
                }
                static const StringView kWords[] = {"center", "top", "bottom", "left", "right"};
                u32 index = 0;
                if (!KeywordIndex(Span<const StringView>(kWords, 5), k, index))
                {
                    error = Format("'{}' is no position", k.text);
                    return false;
                }
                item.keyword = true;
                item.k = BackgroundPositionKeyword(index);
                item.axis = item.k == BackgroundPositionKeyword::Left || item.k == BackgroundPositionKeyword::Right ? 0u
                            : item.k == BackgroundPositionKeyword::Center                                           ? 2u
                                                                                                                    : 1u;
                items.Push(item);
            }
            if (items.Empty() || items.Size() > 2)
            {
                error = "one to four values";
                return false;
            }
            Item x, y;
            x.k = BackgroundPositionKeyword::Center, y.k = BackgroundPositionKeyword::Center;
            auto asX = [](Item i) {
                if (!i.keyword)
                    i.k = BackgroundPositionKeyword::Left;
                return i;
            };
            auto asY = [](Item i) {
                if (!i.keyword)
                    i.k = BackgroundPositionKeyword::Top;
                return i;
            };
            if (items.Size() == 1)
            {
                if (items[0].axis == 1)
                    y = items[0];
                else
                    x = asX(items[0]);
            }
            else
            {
                bool swap = items[0].axis == 1 || items[1].axis == 0;
                x = asX(items[swap ? 1 : 0]);
                y = asY(items[swap ? 0 : 1]);
            }
            const StyleValue xs[2] = {StyleValue::OfKeyword(u32(x.k)), StyleValue::OfLength(x.offset)};
            const StyleValue ys[2] = {StyleValue::OfKeyword(u32(y.k)), StyleValue::OfLength(y.offset)};
            out.Push(Longhand("background-position-x", Span<const StyleValue>(xs, 2)));
            out.Push(Longhand("background-position-y", Span<const StyleValue>(ys, 2)));
            return true;
        }

        // -helix-background-scale-mode: the position, size and repetition of the reference's three modes.
        bool ScaleMode(const Array<Token>& t, Array<Longhand>& out, String& error)
        {
            if (t.Size() != 1 || t[0].kind != Tok::Ident)
            {
                error = "stretch-to-fill, scale-and-crop or scale-to-fit";
                return false;
            }
            StyleValue size[3] = {StyleValue::OfKeyword(u32(BackgroundSizeType::Length)), StyleValue::OfLength(Length::Percent(100)),
                                  StyleValue::OfLength(Length::Percent(100))};
            if (t[0].text == StringView("scale-and-crop"))
                size[0] = StyleValue::OfKeyword(u32(BackgroundSizeType::Cover));
            else if (t[0].text == StringView("scale-to-fit"))
                size[0] = StyleValue::OfKeyword(u32(BackgroundSizeType::Contain));
            else if (t[0].text != StringView("stretch-to-fill"))
            {
                error = Format("'{}' is no scale mode", t[0].text);
                return false;
            }
            if (size[0].keyword != u32(BackgroundSizeType::Length))
                size[1] = size[2] = StyleValue::OfLength(Length::Auto());
            const StyleValue center[2] = {StyleValue::OfKeyword(u32(BackgroundPositionKeyword::Center)), StyleValue::OfLength(Length::Px(0))};
            const StyleValue repeat[2] = {StyleValue::OfKeyword(u32(BackgroundRepeat::NoRepeat)), StyleValue::OfKeyword(u32(BackgroundRepeat::NoRepeat))};
            out.Push(Longhand("background-position-x", Span<const StyleValue>(center, 2)));
            out.Push(Longhand("background-position-y", Span<const StyleValue>(center, 2)));
            out.Push(Longhand("background-size", Span<const StyleValue>(size, 3)));
            out.Push(Longhand("background-repeat", Span<const StyleValue>(repeat, 2)));
            return true;
        }

        // -helix-text-outline: a width and a color, in either order.
        bool TextOutline(const Array<Token>& t, Array<Longhand>& out, String& error)
        {
            static const PropertyInfo colorInfo{"color", ValueType::Color, false, false, false, {}, nullptr, nullptr};
            StyleValue width = StyleValue::OfLength(Length::Px(0)), color = StyleValue::OfColor(Color(0, 0, 0, 1));
            bool hasWidth = false, hasColor = false;
            usize at = 0;
            while (at < t.Size())
            {
                Length l;
                if (!hasWidth && LengthToken(t[at], l) && l.unit == Length::Unit::Pixel)
                {
                    width = StyleValue::OfLength(l);
                    hasWidth = true;
                    at++;
                    continue;
                }
                if (hasColor || !ParseOne(colorInfo, t, at, color, error))
                {
                    if (hasColor)
                        error = Format("'{}' after the outline", t[at].text);
                    return false;
                }
                hasColor = true;
            }
            if (!hasWidth && !hasColor)
            {
                error = "a width and a color";
                return false;
            }
            out.Push(Longhand("-helix-text-outline-width", width));
            out.Push(Longhand("-helix-text-outline-color", color));
            return true;
        }

        const Shorthand kShorthands[] = {
            {"background-position", BackgroundPosition},
            {"-helix-background-scale-mode", ScaleMode},
            {"-helix-text-outline", TextOutline},
            {"margin", [](const Array<Token>& t, Array<Longhand>& o, String& e) { return Sides(kMargins, t, o, e); }},
            {"padding", [](const Array<Token>& t, Array<Longhand>& o, String& e) { return Sides(kPaddings, t, o, e); }},
            {"border-width", [](const Array<Token>& t, Array<Longhand>& o, String& e) { return Sides(kBorderWidths, t, o, e); }},
            {"border-color", [](const Array<Token>& t, Array<Longhand>& o, String& e) { return Sides(kBorderColors, t, o, e); }},
            {"border-radius", [](const Array<Token>& t, Array<Longhand>& o, String& e) { return Sides(kRadii, t, o, e); }},
            {"flex", Flex},
            {"transition", Transition},
        };

        // ------------------------------------------------------------------------------------------------ parser

        class Parser
        {
        public:
            Parser(StringView text, Array<StyleError>& errors) : lex_(text), errors_(errors) { Next(); }

            void Sheet(Array<StyleRule>& rules, Array<String>& imports)
            {
                for (;;)
                {
                    SkipSpace();
                    if (t_.kind == Tok::End)
                        return;
                    if (t_.kind == Tok::Delim && t_.text == StringView("@"))
                    {
                        AtRule(rules.Empty(), imports);
                        continue;
                    }
                    StyleRule rule;
                    if (!Selectors(rule.selectors))
                    {
                        // Skip the broken rule.
                        while (t_.kind != Tok::End && t_.kind != Tok::RightBrace)
                            Next();
                        Next();
                        continue;
                    }
                    Next();
                    Declarations(rule.declarations, true);
                    if (t_.kind == Tok::RightBrace)
                        Next();
                    else
                        Error(t_, "'}' expected");
                    rules.Push(Move(rule));
                }
            }

            // Declarations up to '}' (in a rule) or the end.
            void Declarations(Array<Declaration>& out, bool inRule)
            {
                for (;;)
                {
                    SkipSpace();
                    if (t_.kind == Tok::End || (inRule && t_.kind == Tok::RightBrace))
                        return;
                    if (t_.kind == Tok::Semicolon)
                    {
                        Next();
                        continue;
                    }
                    if (t_.kind != Tok::Ident)
                    {
                        Error(t_, Format("a property name expected, '{}' found", t_.text));
                        SkipToSemicolon(inRule);
                        continue;
                    }
                    Token name = t_;
                    Next();
                    SkipSpace();
                    if (t_.kind != Tok::Colon)
                    {
                        Error(t_, "':' expected after the property name");
                        SkipToSemicolon(inRule);
                        continue;
                    }
                    Next();
                    // The value's text up to ';' or '}' (outside parentheses).
                    usize start = t_.offset;
                    usize end = start;
                    i32 depth = 0;
                    while (t_.kind != Tok::End && !(depth == 0 && (t_.kind == Tok::Semicolon || (inRule && t_.kind == Tok::RightBrace))))
                    {
                        if (t_.kind == Tok::Function)
                            depth++;
                        else if (t_.kind == Tok::RightParen)
                            depth--;
                        end = t_.offset + t_.text.Size() + (t_.kind == Tok::Hash ? 1 : 0) + (t_.kind == Tok::String ? 2 : 0) +
                              (t_.kind == Tok::Function ? 1 : 0) + t_.unit.Size() + (t_.kind == Tok::Percentage ? 1 : 0);
                        Next();
                    }
                    StringView value = lex_.Text().Substr(start, end > start ? end - start : 0).Trimmed();
                    Declare(name, value, out);
                }
            }

        private:
            // @import url("path"); or @import "path"; before the rules; other at-rules are reported and skipped.
            void AtRule(bool beforeRules, Array<String>& imports)
            {
                Token at = t_;
                Next();
                if (t_.kind != Tok::Ident || t_.text != StringView("import"))
                {
                    Error(at, Format("unknown at-rule '@{}'", t_.text));
                    while (t_.kind != Tok::End && t_.kind != Tok::Semicolon && t_.kind != Tok::LeftBrace)
                        Next();
                    if (t_.kind == Tok::LeftBrace)
                    {
                        i32 depth = 0;
                        do
                        {
                            depth += t_.kind == Tok::LeftBrace ? 1 : t_.kind == Tok::RightBrace ? -1 : 0;
                            Next();
                        } while (t_.kind != Tok::End && depth > 0);
                    }
                    else
                        Next();
                    return;
                }
                Next();
                SkipSpace();
                StringView path;
                bool ok = false;
                if (t_.kind == Tok::String)
                {
                    path = t_.text;
                    ok = true;
                    Next();
                }
                else if (t_.kind == Tok::Function && (t_.text == StringView("url") || t_.text == StringView("resource")))
                {
                    Next();
                    SkipSpace();
                    if (t_.kind == Tok::String || t_.kind == Tok::Ident)
                    {
                        path = t_.text;
                        Next();
                        SkipSpace();
                        ok = t_.kind == Tok::RightParen;
                        if (ok)
                            Next();
                    }
                }
                SkipSpace();
                if (!ok || t_.kind != Tok::Semicolon)
                    Error(at, "@import takes url(\"path\") or \"path\" and a ';'");
                else if (!beforeRules)
                    Error(at, "@import comes before the rules");
                else
                    imports.Push(String(path));
                SkipToSemicolon(false);
                if (t_.kind == Tok::Semicolon)
                    Next();
            }

            void Next() { t_ = lex_.Next(); }
            void SkipSpace()
            {
                while (t_.kind == Tok::Space)
                    Next();
            }
            void SkipToSemicolon(bool inRule)
            {
                while (t_.kind != Tok::End && t_.kind != Tok::Semicolon && !(inRule && t_.kind == Tok::RightBrace))
                    Next();
            }
            void Error(const Token& at, StringView message)
            {
                StyleError e;
                e.line = at.line, e.column = at.column;
                e.message = message;
                errors_.Push(Move(e));
            }

            void Declare(const Token& name, StringView value, Array<Declaration>& out)
            {
                if (name.text.StartsWith("--"))
                {
                    Declaration d;
                    d.custom = Intern(name.text);
                    d.raw = value;
                    out.Push(Move(d));
                    return;
                }
                bool hasVar = value.Contains("var(");
                if (value == StringView("initial"))
                {
                    PropertyId id = FindProperty(name.text);
                    if (id == kNoProperty)
                    {
                        Error(name, Format("unknown property '{}'", name.text));
                        return;
                    }
                    Declaration d;
                    d.property = id;
                    d.initial = true;
                    out.Push(Move(d));
                    return;
                }
                Array<Token> tokens;
                Tokenize(value, tokens);
                for (const Shorthand& s : kShorthands)
                    if (s.name == name.text)
                    {
                        if (hasVar)
                        {
                            // Expanded when the element's variables are known.
                            Declaration d;
                            d.shorthand = Intern(name.text);
                            d.hasVar = true;
                            d.raw = value;
                            out.Push(Move(d));
                            return;
                        }
                        Array<Longhand> longhands;
                        String error;
                        usize first = out.Size();
                        if (!s.expand(tokens, longhands, error))
                        {
                            Error(name, Format("{}: {}", name.text, error));
                            return;
                        }
                        for (const Longhand& l : longhands)
                        {
                            Declaration d;
                            d.property = FindProperty(l.property);
                            for (u32 i = 0; i < l.count; i++)
                                d.values.Push(l.values[i]);
                            // A list longhand from a list shorthand gathers its values.
                            bool merged = false;
                            if (Property(d.property).list)
                                for (usize k = first; k < out.Size() && !merged; k++)
                                    if (out[k].property == d.property)
                                    {
                                        out[k].values.Push(l.values[0]);
                                        merged = true;
                                    }
                            if (!merged)
                                out.Push(Move(d));
                        }
                        return;
                    }
                PropertyId id = FindProperty(name.text);
                if (id == kNoProperty)
                {
                    Error(name, Format("unknown property '{}'", name.text));
                    return;
                }
                Declaration d;
                d.property = id;
                if (hasVar)
                {
                    d.hasVar = true;
                    d.raw = value;
                    out.Push(Move(d));
                    return;
                }
                String error;
                if (!ParseProperty(Property(id), tokens, d.values, error))
                {
                    Error(name, Format("{}: {}", name.text, error));
                    return;
                }
                out.Push(Move(d));
            }

            bool Selectors(Array<Selector>& out)
            {
                for (;;)
                {
                    Selector s;
                    if (!OneSelector(s))
                        return false;
                    out.Push(Move(s));
                    SkipSpace();
                    if (t_.kind == Tok::Comma)
                    {
                        Next();
                        SkipSpace();
                        continue;
                    }
                    if (t_.kind == Tok::LeftBrace)
                        return true;
                    Error(t_, Format("an opening brace expected, '{}' found", t_.text));
                    return false;
                }
            }

            bool OneSelector(Selector& s)
            {
                // Parsed left to right, stored right to left.
                Array<SelectorPart> parts;
                Combinator pending = Combinator::None;
                for (;;)
                {
                    SelectorPart part;
                    part.left = pending;
                    bool any = false;
                    for (;;)
                    {
                        if (t_.kind == Tok::Ident && !any)
                        {
                            part.type = Intern(t_.text);
                            any = true;
                            Next();
                        }
                        else if (t_.kind == Tok::Delim && t_.text == StringView("*") && !any)
                        {
                            any = true;
                            Next();
                        }
                        else if (t_.kind == Tok::Hash)
                        {
                            part.name = Intern(t_.text);
                            any = true;
                            Next();
                        }
                        else if (t_.kind == Tok::Delim && t_.text == StringView("."))
                        {
                            Next();
                            if (t_.kind != Tok::Ident)
                            {
                                Error(t_, "a class name expected after '.'");
                                return false;
                            }
                            part.classes.Push(Intern(t_.text));
                            any = true;
                            Next();
                        }
                        else if (t_.kind == Tok::Colon)
                        {
                            Next();
                            if (t_.kind != Tok::Ident)
                            {
                                Error(t_, "a pseudo state expected after ':'");
                                return false;
                            }
                            StringView p = t_.text;
                            PseudoStates state = p == StringView("hover")      ? PseudoStates::Hover
                                                 : p == StringView("active")   ? PseudoStates::Active
                                                 : p == StringView("focus")    ? PseudoStates::Focus
                                                 : p == StringView("checked")  ? PseudoStates::Checked
                                                 : p == StringView("disabled") ? PseudoStates::Disabled
                                                 : p == StringView("enabled")  ? PseudoStates::Disabled
                                                 : p == StringView("root")     ? PseudoStates::Root
                                                                               : PseudoStates::None;
                            if (state == PseudoStates::None)
                            {
                                Error(t_, Format("unknown pseudo state ':{}'", p));
                                return false;
                            }
                            if (p == StringView("enabled"))
                                part.forbidden = part.forbidden | state;
                            else
                                part.required = part.required | state;
                            any = true;
                            Next();
                        }
                        else
                            break;
                    }
                    if (!any)
                    {
                        Error(t_, Format("a selector expected, '{}' found", t_.text));
                        return false;
                    }
                    parts.Push(Move(part));
                    bool space = t_.kind == Tok::Space;
                    SkipSpace();
                    if (t_.kind == Tok::Delim && t_.text == StringView(">"))
                    {
                        pending = Combinator::Child;
                        Next();
                        SkipSpace();
                        continue;
                    }
                    if (space && t_.kind != Tok::Comma && t_.kind != Tok::LeftBrace && t_.kind != Tok::End)
                    {
                        pending = Combinator::Descendant;
                        continue;
                    }
                    break;
                }
                // Right to left, each part's combinator relating it to the part on its left.
                for (usize i = parts.Size(); i-- > 0;)
                    s.parts.Push(Move(parts[i]));
                u32 names = 0, classes = 0, types = 0;
                for (usize i = 0; i < s.parts.Size(); i++)
                {
                    const SelectorPart& p = s.parts[i];
                    names += p.name != kNoAtom ? 1 : 0;
                    classes += u32(p.classes.Size());
                    for (u32 b = 0; b < 32; b++)
                        classes += (u32(p.required | p.forbidden) >> b) & 1u;
                    types += p.type != kNoAtom ? 1 : 0;
                    if (i > 0)
                    {
                        if (p.type != kNoAtom)
                            s.ancestorAtoms.Push(p.type);
                        if (p.name != kNoAtom)
                            s.ancestorAtoms.Push(p.name);
                        for (Atom c : p.classes)
                            s.ancestorAtoms.Push(c);
                    }
                }
                s.specificity = (names < 255 ? names : 255) << 16 | (classes < 255 ? classes : 255) << 8 | (types < 255 ? types : 255);
                return true;
            }

            Lexer lex_;
            Token t_;
            Array<StyleError>& errors_;
        };
    }

    StyleSheetData StyleSheetData::Parse(StringView text)
    {
        StyleSheetData sheet;
        Parser p(text, sheet.errors_);
        p.Sheet(sheet.rules_, sheet.imports_);
        sheet.Index();
        return sheet;
    }

    void StyleSheetData::Index()
    {
        for (u32 r = 0; r < rules_.Size(); r++)
            for (u32 s = 0; s < rules_[r].selectors.Size(); s++)
            {
                const SelectorPart& right = rules_[r].selectors[s].parts[0];
                u32 key = r << 16 | s;
                if (right.name != kNoAtom)
                    byName_[right.name].Push(key);
                else if (!right.classes.Empty())
                    byClass_[right.classes[0]].Push(key);
                else if (right.type != kNoAtom)
                    byType_[right.type].Push(key);
                else
                    universal_.Push(key);
            }
    }

    void ParseDeclarations(StringView text, Array<Declaration>& out, Array<StyleError>& errors)
    {
        Parser p(text, errors);
        p.Declarations(out, false);
    }

    bool ExpandShorthand(Atom shorthand, StringView text, Array<Declaration>& out, String& error)
    {
        StringView name = AtomName(shorthand);
        for (const Shorthand& s : kShorthands)
            if (s.name == name)
            {
                Array<Token> tokens;
                Tokenize(text, tokens);
                Array<Longhand> longhands;
                if (!s.expand(tokens, longhands, error))
                    return false;
                usize first = out.Size();
                for (const Longhand& l : longhands)
                {
                    PropertyId id = FindProperty(l.property);
                    bool merged = false;
                    if (Property(id).list)
                        for (usize k = first; k < out.Size() && !merged; k++)
                            if (out[k].property == id)
                            {
                                out[k].values.Push(l.values[0]);
                                merged = true;
                            }
                    if (merged)
                        continue;
                    Declaration d;
                    d.property = id;
                    for (u32 i = 0; i < l.count; i++)
                        d.values.Push(l.values[i]);
                    out.Push(Move(d));
                }
                return true;
            }
        error = Format("'{}' is no shorthand", name);
        return false;
    }

    bool ParseValue(PropertyId property, StringView text, Array<StyleValue>& out, String& error)
    {
        if (property == kNoProperty)
        {
            error = "unknown property";
            return false;
        }
        Array<Token> tokens;
        Tokenize(text, tokens);
        return ParseProperty(Property(property), tokens, out, error);
    }
}
