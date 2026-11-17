#pragma once

#include <Helix/UI/Engine/Element.h>
#include <Helix/UI/Rendering/Painter.h>
#include <Helix/UI/Rendering/UIMesh.h>

// What scripts draw for an element (its generateVisualContent): during the panel's draw the element's C# side is
// called with a context over the panel's mesh; its Painter2D paths, its meshes, texts and vector images are added
// in the element's space (its border box from the top left), under its transform and clip. A painter of no panel
// (new Painter2D) records into its own mesh for a vector image.
namespace helix::ui
{
    class Panel;

    struct ScriptPaintContext
    {
        Painter painter;
        UIMesh* mesh = nullptr;
        UIMesh own;
        Panel* panel = nullptr;
        ElementHandle element;
        Vector2 origin = Vector2(0, 0);
        f32 opacity = 1;
    };

    // Whether an element's script draws content of its own.
    bool HasScriptPaint(ElementWorld& world, ElementHandle e);
    void SetScriptPaint(ElementWorld& world, ElementHandle e, bool on);
    // Calls the element's script with a context over the mesh (the element's box at origin).
    void RunScriptPaint(Panel& panel, UIMesh& mesh, ElementHandle e, Vector2 origin, f32 opacity);
}
