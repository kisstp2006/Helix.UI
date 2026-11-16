#pragma once

#include <Helix/UI/Core/Affine.h>
#include <Helix/UI/Styles/StyleEngine.h>

// The transforms of elements (translate, rotate and scale about transform-origin). Geometry is laid out untransformed
// in the root's space (LayoutEngine::WorldRect); an element's world map takes those points to where it is shown: its
// parent's world map after its own local one.
namespace helix::ui
{
    // The element's own map of the root's space, from its style and its border box there.
    Affine LocalTransform(const ComputedStyle& style, const Rect& worldRect);
    // The element's world map (its ancestors' maps, then its own).
    Affine WorldTransform(const ElementTree& tree, const LayoutEngine& layout, const StyleEngine& styles, ElementHandle e);
}
