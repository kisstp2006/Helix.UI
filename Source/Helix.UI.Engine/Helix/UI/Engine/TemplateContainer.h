#pragma once

#include <Helix/Engine/Object/Object.h>
#include <Helix/UI/Engine/Element.h>

// The element a template's elements are built under (UITemplate.Instantiate, CloneTree, <Instance> in markup): its
// template and its id in the markup, and its content container (the template's element marked content-container).
namespace helix::ui
{
    class UITemplate;

    HX_CLASS(Namespace = "HelixEngine.UI")
    class TemplateContainer : public Element
    {
    public:
        HX_FUNCTION(Constructor) TemplateContainer();
        HX_FUNCTION(Constructor) explicit TemplateContainer(StringView templateId);
        scripting::TypeId HelixType() const override;

        // The <Template> name an <Instance> named, empty otherwise.
        HX_PROPERTY() String templateId() const { return templateId_; }
        void NameTemplate(StringView id) { templateId_ = String(id); }
        HX_PROPERTY() UITemplate* templateSource() const;
        void SetTemplateSource(UITemplate* source);

        // The object of an existing element of the type (Element::Of makes them).
        TemplateContainer(ElementWorld& world, ElementHandle e) : Element(world, e) {}

    private:
        String templateId_;
        engine::PPtr<engine::Object> source_;
    };
}
