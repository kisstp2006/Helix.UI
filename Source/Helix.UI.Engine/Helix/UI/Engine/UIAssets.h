#pragma once

#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Scripting/Types/Type.h>
#include <Helix/UI/Markup/Hxml.h>

// The UI's assets: markup (.hxml) as a UITemplate, style sheets (.hss) as a StyleSheet and theme style sheets (.tss,
// a sheet that imports others and sets a theme's variables) as a ThemeStyleSheet. Their loaders are in the engine's
// asset registry once the UI module is loaded.
namespace helix::ui
{
    class Element;
    class TemplateContainer;

    // A src of markup or a sheet: relative to the file's folder, or from the source's root when it starts with '/'.
    String ResolveAssetPath(StringView base, StringView src);

    // The templates and sheets markup names, from the asset catalog relative to a file (and the sheets those import);
    // the paths it read go to read (for watching them).
    class AssetHxmlResolver final : public HxmlResolver
    {
    public:
        explicit AssetHxmlResolver(StringView base, Array<String>* read = nullptr) : base_(base), read_(read) {}

        const HxmlDocument* Template(StringView src) override;
        std::shared_ptr<const StyleSheetData> Style(StringView src) override;
        void Instanced(ElementHandle container, StringView name, StringView src) override;

    private:
        std::shared_ptr<const StyleSheetData> Sheet(const String& path, bool& cycle);

        String base_;
        Array<String>* read_ = nullptr;
        Array<String> importing_;
    };

    // A markup document.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class UITemplate : public engine::ScriptableObject
    {
    public:
        // Its elements built under a new TemplateContainer (whose templateSource it is).
        HX_FUNCTION() TemplateContainer* Instantiate();
        HX_FUNCTION() TemplateContainer* CloneTree();
        // Its elements built into an element (its content container); the errors go to the log.
        HX_FUNCTION() void CloneTree(Element* target);

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
    class StyleSheet : public engine::ScriptableObject
    {
    public:
        StyleSheet();
        ~StyleSheet() override;
        // The asset of a sheet (null for one of no asset).
        static StyleSheet* Of(const StyleSheetData* sheet);

        std::shared_ptr<const StyleSheetData> sheet() const { return sheet_; }
        StringView path() const { return path_.View(); }
        // The sheet parsed again from text (its errors are in the sheet).
        void SetText(StringView path, StringView text);
        // The sheets its @import rules name, as its loader found them.
        void SetImported(Array<std::shared_ptr<const StyleSheetData>> sheets) { sheet_->SetImported(Move(sheets)); }

        // A hash of its text; whether parsing it found errors (its warnings: none are reported apart).
        HX_PROPERTY() i32 contentHash() const { return contentHash_; }
        HX_PROPERTY() bool importedWithErrors() const { return !sheet_->Errors().Empty(); }
        HX_PROPERTY() bool importedWithWarnings() const { return false; }

    private:
        std::shared_ptr<StyleSheetData> sheet_ = std::make_shared<StyleSheetData>();
        String path_;
        i32 contentHash_ = 0;
    };

    // A theme: a sheet of the theme's variables (at :root) that imports the sheets of its look.
    HX_CLASS(Namespace = "HelixEngine.UI")
    class ThemeStyleSheet : public StyleSheet
    {
    };

    // The sheet of Helix's default theme (panels whose settings name no theme use it).
    std::shared_ptr<const StyleSheetData> DefaultThemeSheet();

    // A texture a script set as a style's value: its object's id as the value's resource ("entity:<id>"), for
    // properties naming textures by path otherwise. Null for other resources.
    Atom EntityResource(const engine::Object* object);
    engine::Texture2D* EntityResourceTexture(StringView resource);

    // Registers the loaders of .hxml, .hss and .tss (once).
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
