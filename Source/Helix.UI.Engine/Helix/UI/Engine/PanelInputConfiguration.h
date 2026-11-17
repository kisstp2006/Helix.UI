#pragma once

#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Script/GameBehaviour.h>

#include <limits>

// How screen pointers reach the world space panels: rays from the event cameras (the main camera by default) through
// the pointer, the nearest panel of the interaction layers hit within the distance gets the pointer at the point it
// was hit. The first enabled configuration applies; without one world space input is on with the defaults.
namespace helix::ui
{
    HX_CLASS(Namespace = "HelixEngine.UI")
    class PanelInputConfiguration : public engine::GameBehaviour
    {
    public:
        HX_PROPERTY() bool processWorldSpaceInput() const { return processWorldSpaceInput_; }
        void SetProcessWorldSpaceInput(bool value) { processWorldSpaceInput_ = value; }
        HX_PROPERTY() i32 interactionLayers() const { return interactionLayers_; }
        void SetInteractionLayers(i32 value) { interactionLayers_ = value; }
        HX_PROPERTY() f32 maxInteractionDistance() const { return maxInteractionDistance_; }
        void SetMaxInteractionDistance(f32 value) { maxInteractionDistance_ = value; }
        HX_PROPERTY() bool defaultEventCameraIsMainCamera() const { return defaultEventCameraIsMainCamera_; }
        void SetDefaultEventCameraIsMainCamera(bool value) { defaultEventCameraIsMainCamera_ = value; }
        HX_PROPERTY(Setter = "SetEventCameras") Array<engine::Camera*> eventCameras() const;
        // Whether the panels' input comes through the canvas UI's event system (C#'s PanelInputRedirection: 0
        // automatic, 1 never, 2 always) and makes its components; they take effect with that event system.
        HX_PROPERTY() bool autoCreatePanelComponents() const { return autoCreatePanelComponents_; }
        void SetAutoCreatePanelComponents(bool value) { autoCreatePanelComponents_ = value; }
        HX_FUNCTION(Internal) i32 PanelInputRedirectionInternal() const { return redirection_; }
        HX_FUNCTION(Internal) void SetPanelInputRedirectionInternal(i32 value) { redirection_ = value; }
        void SetEventCameras(Span<engine::Camera* const> cameras);

        void OnEnable();
        void OnDisable();

        // The configuration in use (null: the defaults).
        static PanelInputConfiguration* current();

    private:
        HX_FIELD(SerializeField, Name = "m_ProcessWorldSpaceInput") bool processWorldSpaceInput_ = true;
        HX_FIELD(SerializeField, Name = "m_InteractionLayers") i32 interactionLayers_ = -1;
        HX_FIELD(SerializeField, Name = "m_MaxInteractionDistance") f32 maxInteractionDistance_ = std::numeric_limits<f32>::infinity();
        HX_FIELD(SerializeField, Name = "m_DefaultEventCameraIsMainCamera") bool defaultEventCameraIsMainCamera_ = true;
        HX_FIELD(SerializeField, Name = "m_EventCameras") Array<engine::PPtr<engine::Camera>> eventCameras_;
        HX_FIELD(SerializeField, Name = "m_AutoCreatePanelComponents") bool autoCreatePanelComponents_ = true;
        HX_FIELD(SerializeField, Name = "m_PanelInputRedirection") i32 redirection_ = 0;
    };
}
