#include <Helix/UI/Engine/UIAssets.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/TemplateContainer.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Asset/AssetLoader.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Scripting/Types/Type.h>

namespace helix::ui
{
    namespace
    {
        LogChannel UILog{"UI"};

        StringView FolderOf(StringView path)
        {
            for (usize i = path.Size(); i-- > 0;)
                if (path[i] == '/')
                    return path.Substr(0, i + 1);
            return StringView();
        }
    }

    std::shared_ptr<const StyleSheetData> DefaultThemeSheet()
    {
        static const u8 kText[] = {
#include <HelixDefaultTheme.inc>
        };
        static const std::shared_ptr<const StyleSheetData> sheet =
            std::make_shared<StyleSheetData>(StyleSheetData::Parse(StringView(reinterpret_cast<const char*>(kText), sizeof(kText))));
        return sheet;
    }

    String ResolveAssetPath(StringView base, StringView src)
    {
        if (src.StartsWith("/"))
            return String(src.Substr(1));
        String out(FolderOf(base));
        while (src.StartsWith("../"))
        {
            src = src.Substr(3);
            StringView parent = FolderOf(out.View().Substr(0, out.Size() > 0 ? out.Size() - 1 : 0));
            out = String(parent);
        }
        out += src;
        return out;
    }

    const HxmlDocument* AssetHxmlResolver::Template(StringView src)
    {
        String path = ResolveAssetPath(base_.View(), src);
        if (read_)
            read_->Push(path);
        const engine::AssetEntry* e = engine::FindAssetAtPath(path.View());
        auto* t = e ? ScriptObjectAs<UITemplate>(engine::LoadAsset(e->guid)) : nullptr;
        return t ? &t->document() : nullptr;
    }

    std::shared_ptr<const StyleSheetData> AssetHxmlResolver::Style(StringView src)
    {
        bool cycle = false;
        return Sheet(ResolveAssetPath(base_.View(), src), cycle);
    }

    void AssetHxmlResolver::Instanced(ElementHandle container, StringView name, StringView src)
    {
        // The container's object knows its template.
        ElementWorld& world = ElementWorld::Main();
        auto* c = static_cast<TemplateContainer*>(Element::Of(world, container));
        if (!c || c->HelixType() != scripting::TypeOf<TemplateContainer>())
            return;
        c->NameTemplate(name);
        const engine::AssetEntry* e = engine::FindAssetAtPath(ResolveAssetPath(base_.View(), src).View());
        c->SetTemplateSource(e ? ScriptObjectAs<UITemplate>(engine::LoadAsset(e->guid)) : nullptr);
    }

    // A sheet and the sheets it imports (relative to it), found again on every build; a sheet importing itself through
    // others is reported and that import left out.
    std::shared_ptr<const StyleSheetData> AssetHxmlResolver::Sheet(const String& path, bool& cycle)
    {
        for (const String& open : importing_)
            if (open == path)
            {
                HX_LOG_WARNING(UILog, "{}: imported again by a sheet it imports", path.View());
                cycle = true;
                return nullptr;
            }
        if (read_)
            read_->Push(path);
        const engine::AssetEntry* e = engine::FindAssetAtPath(path.View());
        auto* s = e ? ScriptObjectAs<StyleSheet>(engine::LoadAsset(e->guid)) : nullptr;
        if (!s)
            return nullptr;
        importing_.Push(path);
        Array<std::shared_ptr<const StyleSheetData>> imported;
        for (const String& i : s->sheet()->Imports())
        {
            bool again = false;
            if (auto sheet = Sheet(ResolveAssetPath(path.View(), i.View()), again))
                imported.Push(Move(sheet));
            else if (!again)
                HX_LOG_WARNING(UILog, "{}: no style sheet at {}", path.View(), i.View());
        }
        importing_.Pop();
        s->SetImported(Move(imported));
        return s->sheet();
    }

    TemplateContainer* UITemplate::Instantiate()
    {
        auto* container = new TemplateContainer();
        container->SetTemplateSource(this);
        CloneTree(container);
        return container;
    }

    TemplateContainer* UITemplate::CloneTree() { return Instantiate(); }

    void UITemplate::CloneTree(Element* target)
    {
        if (!target || !target->alive())
            return;
        ElementWorld& world = target->world();
        Element* into = target->contentContainer();
        if (!into || !into->alive())
            into = target;
        AssetHxmlResolver resolver(path_.View());
        Array<HxmlError> errors;
        ::helix::ui::Instantiate(document_, world.tree(), world.styles(), into->handle(), &resolver, errors);
        for (const HxmlError& e : errors)
            HX_LOG_WARNING(UILog, "{}:{}:{}: {}", path_.View(), e.line, e.column, e.message.View());
        // In a panel: the types' behavior at once (else when the elements come into one).
        if (Panel* p = into->panel())
            p->AttachTypes(into->handle());
    }

    bool UITemplate::SetText(StringView path, StringView text, Array<HxmlError>& errors)
    {
        HxmlDocument parsed;
        if (!HxmlDocument::Parse(text, parsed, errors))
            return false;
        document_ = Move(parsed);
        path_ = path;
        return true;
    }

    namespace
    {
        // The assets of the sheets they hold.
        HashMap<const StyleSheetData*, StyleSheet*>& SheetAssets()
        {
            static auto* map = new HashMap<const StyleSheetData*, StyleSheet*>();
            return *map;
        }

    }

    StyleSheet::StyleSheet() { SheetAssets().Set(sheet_.get(), this); }

    StyleSheet::~StyleSheet() { SheetAssets().Remove(sheet_.get()); }

    StyleSheet* StyleSheet::Of(const StyleSheetData* sheet)
    {
        StyleSheet** found = sheet ? SheetAssets().Find(sheet) : nullptr;
        return found ? *found : nullptr;
    }

    void StyleSheet::SetText(StringView path, StringView text)
    {
        SheetAssets().Remove(sheet_.get());
        sheet_ = std::make_shared<StyleSheetData>(StyleSheetData::Parse(text));
        SheetAssets().Set(sheet_.get(), this);
        path_ = path;
        // FNV-1a of the text.
        u32 h = 2166136261u;
        for (char c : text)
            h = (h ^ u8(c)) * 16777619u;
        contentHash_ = i32(h);
    }

    namespace
    {
        constexpr StringView kEntity = "entity:";
    }

    Atom EntityResource(const engine::Object* object)
    {
        return object ? Intern(Format("{}{}", kEntity, object->GetEntityId().value)) : kNoAtom;
    }

    engine::Texture2D* EntityResourceTexture(StringView resource)
    {
        if (!resource.StartsWith(kEntity))
            return nullptr;
        u64 raw = 0;
        for (char c : resource.Substr(kEntity.Size()))
            raw = c >= '0' && c <= '9' ? raw * 10 + u64(c - '0') : raw;
        return engine::EntityIdToObject<engine::Texture2D>(engine::EntityId::FromULong(raw));
    }

    namespace
    {
        struct Decoded
        {
            String path;
            String text;
        };

        void* Decode(const engine::AssetDecodeInput& input, String&)
        {
            auto* d = new Decoded();
            d->path = input.path;
            d->text = StringView(reinterpret_cast<const char*>(input.bytes.Data()), input.bytes.Size());
            return d;
        }

        void Release(void* decoded) { delete static_cast<Decoded*>(decoded); }

        bool IntegrateTemplate(void* decoded, engine::AssetObjects& out, Array<String>& errors)
        {
            auto* d = static_cast<Decoded*>(decoded);
            auto* asset = static_cast<UITemplate*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<UITemplate>()));
            if (!asset)
            {
                errors.Push(String("the UI module's UITemplate class is not registered"));
                return false;
            }
            Array<HxmlError> found;
            asset->SetText(d->path.View(), d->text.View(), found);
            for (const HxmlError& e : found)
                errors.Push(Format("{}:{}: {}", e.line, e.column, e.message.View()));
            out.Add(engine::MainAssetFileId(engine::ClassId::MonoBehaviour), asset, true);
            return true;
        }

        template <class T>
        bool IntegrateSheet(void* decoded, engine::AssetObjects& out, Array<String>& errors)
        {
            auto* d = static_cast<Decoded*>(decoded);
            auto* asset = static_cast<StyleSheet*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<T>()));
            if (!asset)
            {
                errors.Push(String("the UI module's StyleSheet class is not registered"));
                return false;
            }
            asset->SetText(d->path.View(), d->text.View());
            for (const StyleError& e : asset->sheet()->Errors())
                errors.Push(Format("{}:{}: {}", e.line, e.column, e.message.View()));
            out.Add(engine::MainAssetFileId(engine::ClassId::MonoBehaviour), asset, true);
            return true;
        }
    }

    void RegisterUIAssetLoaders()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;
        engine::AssetLoader markup;
        markup.name = "HelixUI.Markup";
        markup.extensions.Push(String("hxml"));
        markup.decode = &Decode;
        markup.integrate = &IntegrateTemplate;
        markup.release = &Release;
        engine::RegisterAssetLoader(markup);
        engine::AssetLoader sheets;
        sheets.name = "HelixUI.StyleSheets";
        sheets.extensions.Push(String("hss"));
        sheets.decode = &Decode;
        sheets.integrate = &IntegrateSheet<StyleSheet>;
        sheets.release = &Release;
        engine::RegisterAssetLoader(sheets);
        engine::AssetLoader themes;
        themes.name = "HelixUI.ThemeStyleSheets";
        themes.extensions.Push(String("tss"));
        themes.decode = &Decode;
        themes.integrate = &IntegrateSheet<ThemeStyleSheet>;
        themes.release = &Release;
        engine::RegisterAssetLoader(themes);
    }
}
