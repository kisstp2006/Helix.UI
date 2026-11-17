#pragma once

#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Object/Object.h>
#include <Helix/UI/Engine/ImageAtlas.h>
#include <Helix/UI/Rendering/UIMesh.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

namespace helix::engine
{
    class CommandBuffer;
    class Material;
}

// What the screen's UI shares inside the module: the image atlas, the materials of the batches, a UIMesh's upload into
// an engine mesh, and the screen overlay that draws the panels and then the immediate GUI.
namespace helix::ui::overlay
{
    ImageAtlas& SharedAtlas();
    // The platform's default font (the first family the machine has) and the others after it.
    TextFont* DefaultFont(Array<TextFont*>& fallbacks);
    // The texture of a glyph's quad (its font's atlas page).
    engine::Texture2D* GlyphTexture(const TextQuad& quad);
    // The material of a batch's textures (world: depth tested with the scene's transparent objects); null without the
    // UI's shader.
    engine::Material* MaterialFor(const UIBatch& batch, bool world = false);

    // A UIMesh in an engine mesh: one interleaved stream in UIVertex's layout and 32 bit indices, its buffers only
    // growing; a sub mesh per batch.
    class MeshUpload
    {
    public:
        MeshUpload() = default;
        ~MeshUpload();
        MeshUpload(const MeshUpload&) = delete;
        MeshUpload& operator=(const MeshUpload&) = delete;
        void Upload(const UIMesh& mesh);
        engine::Mesh* mesh() const { return mesh_.Get(); }
        // Its batches in order with their materials, in the command buffer's current view.
        void Draw(const UIMesh& mesh, engine::CommandBuffer& cmd) const;

    private:
        Array<UIVertex> vertices_;
        Array<engine::SubMeshDescriptor> subMeshes_;
        u32 vertexCapacity_ = 0, indexCapacity_ = 0;
        engine::PPtr<engine::Mesh> mesh_;
    };

    // The immediate GUI's drawing over the screen after the panels (null: none). The overlay stays while there are
    // panels or a GUI drawer.
    void SetGuiDrawer(void (*draw)(engine::CommandBuffer& cmd));
}
