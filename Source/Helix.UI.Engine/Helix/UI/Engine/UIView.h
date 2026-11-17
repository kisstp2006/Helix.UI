#pragma once

#include <Helix/Engine/Script/GameBehaviour.h>
#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/UIAssets.h>
#include <Helix/UI/Engine/UIPanelSettings.h>
#include <Helix/Math/Matrix4x4.h>

#include <memory>

namespace helix::engine
{
    class Material;
    class Mesh;
}

// A view: a markup asset built into a panel when the view is enabled (or its source set). The views of one
// UIPanelSettings share its panel (a view without settings has one of its own), and a view under another view's
// GameObject joins that view's panel (parentUI): each view's root is a child of the panel's root, ordered by the views'
// sorting orders; the panels draw by their settings' sorting order over the screen in the engine's screen overlay
// (after every camera, with any pipeline or none), or into their settings' target texture. A panel is laid out in the
// screen's size divided by its scale (UIPanelSettings's scale mode). A view of world space settings has a panel of its
// own in the scene: a panel pixel is a unit of its GameObject's space, the panel's pivot point at its origin, x right
// and y up; the cameras draw it with the scene's transparent objects. With watchFiles the markup and its sheets are
// read again from their source while the game runs and the view built again when they change. The panels read the
// engine's input each frame (InputFeed): pointers go to the top-most screen panel with an element under them, else to
// the nearest world space panel their ray hits (PanelInputConfiguration), keys to the panel last pressed in; a focused
// text element keeps the input method on.
namespace helix::ui
{
    class RuntimePanel;

    // The point of a world space panel at its GameObject's origin.
    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class Pivot : i32
    {
        Center = 0,
        TopLeft = 1,
        TopCenter = 2,
        TopRight = 3,
        LeftCenter = 4,
        RightCenter = 5,
        BottomLeft = 6,
        BottomCenter = 7,
        BottomRight = 8,
    };

    // What the pivot is a point of: the bounds of the elements, or the root's layout box.
    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class PivotReferenceSize : i32
    {
        BoundingBox = 0,
        Layout = 1,
    };

    // A world space panel's size: its elements' (Dynamic) or worldSpaceSize (Fixed).
    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class WorldSpaceSizeMode : i32
    {
        Dynamic = 0,
        Fixed = 1,
    };

    HX_CLASS(Namespace = "HelixEngine.UI")
    class UIView : public engine::GameBehaviour
    {
    public:
        UIView() = default;
        // A copy (Instantiate) takes the serialized fields; it is built when it is enabled.
        UIView(const UIView& other);
        UIView& operator=(const UIView&) = delete;
        ~UIView() override;

        HX_PROPERTY() UITemplate* source() const { return source_.Get(); }
        void SetSource(UITemplate* value);
        HX_PROPERTY() UIPanelSettings* panelSettings() const { return ScriptObjectAs<UIPanelSettings>(settings_.Get()); }
        void SetPanelSettings(UIPanelSettings* value);
        // Views with a higher order are over the others of their panel.
        HX_PROPERTY() f32 sortingOrder() const { return sortingOrder_; }
        void SetSortingOrder(f32 value);
        HX_PROPERTY() bool watchFiles() const { return watchFiles_; }
        void SetWatchFiles(bool value) { watchFiles_ = value; }
        HX_PROPERTY() Pivot pivot() const { return pivot_; }
        void SetPivot(Pivot value) { pivot_ = value; }
        HX_PROPERTY() PivotReferenceSize pivotReferenceSize() const { return pivotReferenceSize_; }
        void SetPivotReferenceSize(PivotReferenceSize value) { pivotReferenceSize_ = value; }
        HX_PROPERTY() WorldSpaceSizeMode worldSpaceSizeMode() const { return worldSpaceSizeMode_; }
        void SetWorldSpaceSizeMode(WorldSpaceSizeMode value) { worldSpaceSizeMode_ = value; }
        HX_PROPERTY() Vector2 worldSpaceSize() const { return worldSpaceSize_; }
        void SetWorldSpaceSize(Vector2 value) { worldSpaceSize_ = value; }
        // A nested view's root in its parent's panel: 0 relative (in the flow), 1 absolute (over the panel; C#'s
        // position).
        HX_FUNCTION(Internal) i32 PositionInternal() const { return position_; }
        HX_FUNCTION(Internal) void SetPositionInternal(i32 value);
        // The view of the nearest GameObject above this one's that has an enabled view (null when none).
        HX_PROPERTY(ReadOnly) UIView* parentUI() const;
        // The map from the panel's pixels to the world (a world space view; the identity for others).
        Matrix4x4 PanelToWorld() const;

        void OnEnable();
        void OnDisable();
        void Update();

        // The panel (null while the view is disabled).
        Panel* panel() const;
        // The root of the view's elements (null while the view is disabled).
        HX_PROPERTY() Element* rootElement() const;
        // The view an element is in (null when none).
        HX_FUNCTION() static UIView* FindUIView(Element* element);
        // The top-most element of the view's panel under a point of the screen (pixels from the top left), none when
        // nothing of it is there: a game asks its views whether the pointer is over them (the UI reads the input, it
        // does not take it from the game).
        ElementHandle Pick(Vector2 screenPosition) const;
        // The errors of the last build.
        Span<const HxmlError> errors() const { return Span<const HxmlError>(errors_.Data(), errors_.Size()); }
        // The view is built again from the source (at the next Update).
        void Rebuild() { built_ = false; }

        // The views enabled, in the order they were enabled.
        static Span<UIView* const> Views();
        // The geometry of its panel's last Update in a mesh (null before the first), a submesh per batch.
        engine::Mesh* mesh() const;
        const UIMesh& uiMesh() const;
        // The texture at a path of the view's styles (a cursor image; null when there is none).
        engine::Texture2D* CursorTexture(Atom path);
        // Its panel (null while the view is disabled).
        RuntimePanel* runtime() const { return runtime_; }
        // Screen pixels per pixel of the panel whose root this is (1 for any other element).
        HX_FUNCTION(Internal) static f32 PanelScaleInternal(Element* panelRoot);

    private:
        friend class RuntimePanel;
        void Build();
        bool SourcesChanged();
        void Attach();
        void Detach();

        HX_FIELD(SerializeField, Name = "m_PanelSettings") engine::PPtr<engine::ScriptableObject> settings_;
        HX_FIELD(SerializeField, Name = "m_Source") engine::PPtr<UITemplate> source_;
        HX_FIELD(SerializeField, Name = "m_SortingOrder") f32 sortingOrder_ = 0;
        HX_FIELD(SerializeField, Name = "m_WatchFiles") bool watchFiles_ = false;
        HX_FIELD(SerializeField, Name = "m_Position") i32 position_ = 1;
        HX_FIELD(SerializeField, Name = "m_Pivot") Pivot pivot_ = Pivot::Center;
        HX_FIELD(SerializeField, Name = "m_PivotReferenceSize") PivotReferenceSize pivotReferenceSize_ = PivotReferenceSize::BoundingBox;
        HX_FIELD(SerializeField, Name = "m_WorldSpaceSizeMode") WorldSpaceSizeMode worldSpaceSizeMode_ = WorldSpaceSizeMode::Dynamic;
        HX_FIELD(SerializeField, Name = "m_WorldSpaceSize") Vector2 worldSpaceSize_ = Vector2(1920, 1080);

        RuntimePanel* runtime_ = nullptr;
        ElementHandle root_;
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
    };
}
