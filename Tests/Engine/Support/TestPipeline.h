#pragma once

#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Pipeline/CommandBuffer.h>
#include <Helix/Engine/Rendering/Pipeline/RenderPipeline.h>
#include <Helix/Engine/Rendering/Pipeline/ScriptableRenderContext.h>
#include <Helix/Engine/Script/ScriptableObject.h>

// The tests' pipeline (the game module UIEngineTests): each camera's target cleared to its background color, then the
// end of the camera, where the views draw.
namespace helix::ui::testing
{
    class ClearPipeline final : public engine::RenderPipeline
    {
    public:
        void Render(engine::ScriptableRenderContext& context, Span<engine::Camera* const> cameras) override
        {
            BeginContextRendering(context, cameras);
            for (engine::Camera* camera : cameras)
            {
                BeginCameraRendering(context, camera);
                context.SetupCameraProperties(camera);
                engine::CommandBuffer clear;
                clear.SetName("Clear");
                clear.ClearRenderTarget(true, true, camera->backgroundColor());
                context.ExecuteCommandBuffer(clear);
                context.Submit();
                EndCameraRendering(context, camera);
            }
            EndContextRendering(context, cameras);
        }
    };

    HX_CLASS(Namespace = "UIEngineTests")
    class ClearPipelineAsset : public engine::ScriptableObject
    {
    protected:
        engine::RenderPipeline* CreatePipeline() { return new ClearPipeline(); }
    };
}
