#pragma once

#include <Helix/Engine/Object/Object.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Engine/Texture/RenderTexture.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Vector.h>
#include <Helix/UI/Engine/UIAssets.h>

// The settings of a runtime panel: how its pixels scale to the screen's, where it draws (the screen or a render
// texture) and in what order, its theme. The views of one settings asset share one panel.
namespace helix::ui
{
    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class PanelScaleMode : i32
    {
        // A panel pixel is `scale` screen pixels.
        ConstantPixelSize = 0,
        // Elements keep their size on screens of any density (the screen's dpi against referenceDpi).
        ConstantPhysicalSize = 1,
        // Elements grow and shrink with the screen against referenceResolution.
        ScaleWithScreenSize = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class PanelScreenMatchMode : i32
    {
        // Between matching the width (0) and the height (1), by match.
        MatchWidthOrHeight = 0,
        // The panel is no smaller than the reference resolution.
        Shrink = 1,
        // The panel is no larger than the reference resolution.
        Expand = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.UI")
    enum class PanelRenderMode : i32
    {
        // Over the screen (or into the target texture).
        ScreenSpaceOverlay = 0,
        // In the scene, each view placed by its GameObject's transform and drawn by the cameras.
        WorldSpace = 1,
    };

    HX_CLASS(Namespace = "HelixEngine.UI")
    class UIPanelSettings : public engine::ScriptableObject
    {
    public:
        HX_PROPERTY() PanelRenderMode renderMode() const { return renderMode_; }
        void SetRenderMode(PanelRenderMode value) { renderMode_ = value; }
        HX_PROPERTY() ThemeStyleSheet* themeStyleSheet() const { return ScriptObjectAs<ThemeStyleSheet>(theme_.Get()); }
        void SetThemeStyleSheet(ThemeStyleSheet* value) { theme_ = engine::PPtr<engine::ScriptableObject>(value); }
        HX_PROPERTY() engine::RenderTexture* targetTexture() const { return targetTexture_.Get(); }
        void SetTargetTexture(engine::RenderTexture* value) { targetTexture_ = engine::PPtr<engine::RenderTexture>(value); }
        HX_PROPERTY() i32 targetDisplay() const { return targetDisplay_; }
        void SetTargetDisplay(i32 value) { targetDisplay_ = value; }
        HX_PROPERTY() PanelScaleMode scaleMode() const { return scaleMode_; }
        void SetScaleMode(PanelScaleMode value) { scaleMode_ = value; }
        HX_PROPERTY() f32 scale() const { return scale_; }
        void SetScale(f32 value) { scale_ = value; }
        HX_PROPERTY() f32 referenceDpi() const { return referenceDpi_; }
        void SetReferenceDpi(f32 value) { referenceDpi_ = value; }
        HX_PROPERTY() f32 fallbackDpi() const { return fallbackDpi_; }
        void SetFallbackDpi(f32 value) { fallbackDpi_ = value; }
        HX_PROPERTY() Vector2Int referenceResolution() const { return referenceResolution_; }
        void SetReferenceResolution(Vector2Int value) { referenceResolution_ = value; }
        HX_PROPERTY() PanelScreenMatchMode screenMatchMode() const { return screenMatchMode_; }
        void SetScreenMatchMode(PanelScreenMatchMode value) { screenMatchMode_ = value; }
        HX_PROPERTY() f32 match() const { return match_; }
        void SetMatch(f32 value) { match_ = value; }
        HX_PROPERTY() f32 sortingOrder() const { return sortingOrder_; }
        void SetSortingOrder(f32 value) { sortingOrder_ = value; }
        HX_PROPERTY() bool clearColor() const { return clearColor_; }
        void SetClearColor(bool value) { clearColor_ = value; }
        HX_PROPERTY() Color colorClearValue() const { return colorClearValue_; }
        void SetColorClearValue(Color value) { colorClearValue_ = value; }
        HX_PROPERTY() bool clearDepthStencil() const { return clearDepthStencil_; }
        void SetClearDepthStencil(bool value) { clearDepthStencil_ = value; }
        HX_PROPERTY(ReadOnly) f32 depthClearValue() const { return 1.0f; }

        // Screen pixels per panel pixel for a target of the size (and the screen's dpi, 0 when unknown).
        f32 ResolveScale(f32 width, f32 height, f32 dpi) const;

        // A script's function maps screen points to the panel (C#'s SetScreenToPanelSpaceFunction): 0 none, 1 a
        // point, 2 a point and its distance from the screen (against the world space panels).
        HX_FUNCTION(Internal) void SetScreenToPanelKindInternal(i32 kind) { screenToPanel_ = kind; }
        i32 screenToPanelKind() const { return screenToPanel_; }
        // The function's panel point (and distance in z) of a screen point (pixels from the top left); false when
        // it is off the panel.
        bool ScreenToPanel(Vector2 screen, Vector3& out);

    private:
        HX_FIELD(SerializeField, Name = "m_RenderMode") PanelRenderMode renderMode_ = PanelRenderMode::ScreenSpaceOverlay;
        HX_FIELD(SerializeField, Name = "themeUss") engine::PPtr<engine::ScriptableObject> theme_;
        HX_FIELD(SerializeField, Name = "m_TargetTexture") engine::PPtr<engine::RenderTexture> targetTexture_;
        HX_FIELD(SerializeField, Name = "m_TargetDisplay") i32 targetDisplay_ = 0;
        HX_FIELD(SerializeField, Name = "m_ScaleMode") PanelScaleMode scaleMode_ = PanelScaleMode::ConstantPixelSize;
        HX_FIELD(SerializeField, Name = "m_Scale") f32 scale_ = 1.0f;
        HX_FIELD(SerializeField, Name = "m_ReferenceDpi") f32 referenceDpi_ = 96.0f;
        HX_FIELD(SerializeField, Name = "m_FallbackDpi") f32 fallbackDpi_ = 96.0f;
        HX_FIELD(SerializeField, Name = "m_ReferenceResolution") Vector2Int referenceResolution_ = Vector2Int(1200, 800);
        HX_FIELD(SerializeField, Name = "m_ScreenMatchMode") PanelScreenMatchMode screenMatchMode_ = PanelScreenMatchMode::MatchWidthOrHeight;
        HX_FIELD(SerializeField, Name = "m_Match") f32 match_ = 0.0f;
        HX_FIELD(SerializeField, Name = "m_SortingOrder") f32 sortingOrder_ = 0.0f;
        HX_FIELD(SerializeField, Name = "m_ClearColor") bool clearColor_ = false;
        HX_FIELD(SerializeField, Name = "m_ColorClearValue") Color colorClearValue_ = Color(0, 0, 0, 0);
        HX_FIELD(SerializeField, Name = "m_ClearDepthStencil") bool clearDepthStencil_ = true;
        i32 screenToPanel_ = 0;
    };
}
