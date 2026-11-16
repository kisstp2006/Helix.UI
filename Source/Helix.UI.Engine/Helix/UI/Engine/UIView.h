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

// A view: a panel built from a markup asset, laid out in the screen's size and drawn over the screen in the engine's
// screen overlay (after every camera, with any pipeline or none), in the order of the views' sorting orders. With
// watchFiles the markup and its sheets are read again from their source while the game runs and the panel built again
// when they change. The views read the engine's input each frame (InputFeed): pointers go to the top-most view with an
// element under them, keys to the view last pressed in; a focused text element keeps the input method on.
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
        // The top-most element of the view under a point of the screen (pixels from the top left), none when nothing
        // of it is there: a game asks its views whether the pointer is over them (the UI reads the input, it does not
        // take it from the game).
        ElementHandle Pick(Vector2 screenPosition) const { return panel_ ? panel_->events().Pick(screenPosition) : ElementHandle{}; }
        // The errors of the last build.
        Span<const HxmlError> errors() const { return Span<const HxmlError>(errors_.Data(), errors_.Size()); }
        // The panel is built again from the source (at the next Update).
        void Rebuild() { built_ = false; }

        // The views that draw, in drawing order (for the drawer).
        static Span<UIView* const> Views();
        // The geometry of the last Update in a mesh (null before the first), a submesh per batch.
        engine::Mesh* mesh() const;
        const UIMesh& uiMesh() const { return uiMesh_; }

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
        UIMesh uiMesh_;
        engine::PPtr<engine::Mesh> mesh_;
    };
}
