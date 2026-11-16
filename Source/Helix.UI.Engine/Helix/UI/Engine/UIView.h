#pragma once

#include <Helix/Engine/Script/GameBehaviour.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/UIAssets.h>

#include <memory>

namespace helix::engine
{
    class Material;
    class Mesh;
}

// A view: a panel built from a markup asset, laid out in the screen's size and drawn over the image of the game's
// cameras (after each camera, in the order of the views' sorting orders). With watchFiles the markup and its sheets
// are read again from their source while the game runs and the panel built again when they change.
namespace helix::ui
{
    HX_CLASS(Namespace = "HelixEngine.UI")
    class UIView : public engine::GameBehaviour
    {
    public:
        UIView() = default;
        // A copy (Instantiate) takes the serialized fields; its panel is built when it is enabled.
        UIView(const UIView& other);
        UIView& operator=(const UIView&) = delete;
        ~UIView() override;

        HX_PROPERTY() UITemplate* source() const { return source_.Get(); }
        void SetSource(UITemplate* value);
        // Views with a higher order draw over the others.
        HX_PROPERTY() i32 sortingOrder() const { return sortingOrder_; }
        void SetSortingOrder(i32 value) { sortingOrder_ = value; }
        HX_PROPERTY() bool watchFiles() const { return watchFiles_; }
        void SetWatchFiles(bool value) { watchFiles_ = value; }

        void OnEnable();
        void OnDisable();
        void Update();

        // The panel (null while the view is disabled).
        Panel* panel() const { return panel_.get(); }
        // The errors of the last build.
        Span<const HxmlError> errors() const { return Span<const HxmlError>(errors_.Data(), errors_.Size()); }
        // The panel is built again from the source (at the next Update).
        void Rebuild() { built_ = false; }

        // The views that draw, in drawing order (for the drawer).
        static Span<UIView* const> Views();
        // The quads of the last Update in a mesh (null before the first), a submesh per batch.
        engine::Mesh* mesh() const;
        Span<const PanelBatch> batches() const { return Span<const PanelBatch>(batches_.Data(), batches_.Size()); }

    private:
        class TextHost;
        void Build();
        bool SourcesChanged();

        HX_FIELD(SerializeField, Name = "m_Source") engine::PPtr<UITemplate> source_;
        HX_FIELD(SerializeField, Name = "m_SortingOrder") i32 sortingOrder_ = 0;
        HX_FIELD(SerializeField, Name = "m_WatchFiles") bool watchFiles_ = false;

        std::unique_ptr<PanelTextHost> host_;
        std::unique_ptr<Panel> panel_;
        Array<HxmlError> errors_;
        // The files the last build read (the markup first) and their bytes, for watchFiles.
        struct Watched
        {
            String path;
            Array<u8> bytes;
        };
        Array<Watched> watched_;
        f64 lastWatch_ = 0;
        bool built_ = false;
        Array<PanelVertex> vertices_;
        Array<u32> indices_;
        Array<PanelBatch> batches_;
        engine::PPtr<engine::Mesh> mesh_;
    };
}
