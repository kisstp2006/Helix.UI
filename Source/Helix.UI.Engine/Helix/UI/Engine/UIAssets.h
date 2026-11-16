#pragma once

#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Scripting/Types/Type.h>
#include <Helix/UI/Markup/Hxml.h>

// The UI's assets: markup (.hxml) as a UITemplate and style sheets (.hss) as a StyleSheetAsset. Their loaders are in
// the engine's asset registry once the UI module is loaded.
namespace helix::ui
{
    // A markup document.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class UITemplate : public engine::ScriptableObject
    {
    public:
        const HxmlDocument& document() const { return document_; }
        // The asset's path in its source (other assets of the markup are found relative to it).
        StringView path() const { return path_.View(); }
        // The markup parsed again from text; false (the old document kept) when it is no HXML.
        bool SetText(StringView path, StringView text, Array<HxmlError>& errors);

    private:
        HxmlDocument document_;
        String path_;
    };

    // A style sheet.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class StyleSheetAsset : public engine::ScriptableObject
    {
    public:
        const std::shared_ptr<const StyleSheet>& sheet() const { return sheet_; }
        StringView path() const { return path_.View(); }
        // The sheet parsed again from text (its errors are in the sheet).
        void SetText(StringView path, StringView text);

    private:
        std::shared_ptr<const StyleSheet> sheet_ = std::make_shared<StyleSheet>();
        String path_;
    };

    // Registers the loaders of .hxml and .hss (once).
    void RegisterUIAssetLoaders();

    // The object as a script object of class T (or a subclass), null when it is another object.
    template <class T>
    T* ScriptObjectAs(engine::Object* object)
    {
        if (!object || !object->Is<engine::ScriptableObject>())
            return nullptr;
        const engine::ScriptClass* s = static_cast<engine::ScriptableObject*>(object)->script();
        return s && scripting::IsAssignableFrom(scripting::TypeOf<T>(), s->type) ? static_cast<T*>(object) : nullptr;
    }
}
