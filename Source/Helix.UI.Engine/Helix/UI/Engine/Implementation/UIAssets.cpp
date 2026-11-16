#include <Helix/UI/Engine/UIAssets.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Asset/AssetLoader.h>
#include <Helix/Scripting/Types/Type.h>

namespace helix::ui
{
    bool UITemplate::SetText(StringView path, StringView text, Array<HxmlError>& errors)
    {
        HxmlDocument parsed;
        if (!HxmlDocument::Parse(text, parsed, errors))
            return false;
        document_ = Move(parsed);
        path_ = path;
        return true;
    }

    void StyleSheetAsset::SetText(StringView path, StringView text)
    {
        sheet_ = std::make_shared<StyleSheet>(StyleSheet::Parse(text));
        path_ = path;
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

        bool IntegrateSheet(void* decoded, engine::AssetObjects& out, Array<String>& errors)
        {
            auto* d = static_cast<Decoded*>(decoded);
            auto* asset = static_cast<StyleSheetAsset*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<StyleSheetAsset>()));
            if (!asset)
            {
                errors.Push(String("the UI module's StyleSheetAsset class is not registered"));
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
        sheets.integrate = &IntegrateSheet;
        sheets.release = &Release;
        engine::RegisterAssetLoader(sheets);
    }
}
