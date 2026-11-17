#include <Helix/UI/Styles/Transforms.h>

namespace helix::ui
{
    namespace
    {
        f32 Resolve(const Length& l, f32 size) { return l.unit == Length::Unit::Percent ? l.value * size / 100.0f : l.unit == Length::Unit::Pixel ? l.value : 0.0f; }
    }

    Affine LocalTransform(const ComputedStyle& s, const Rect& r)
    {
        Vector2 t(Resolve(s.translate[0], r.width), Resolve(s.translate[1], r.height));
        if (t.x == 0 && t.y == 0 && s.rotate == 0 && s.scale[0] == 1 && s.scale[1] == 1)
            return Affine();
        Vector2 o(r.x + Resolve(s.transformOrigin[0], r.width), r.y + Resolve(s.transformOrigin[1], r.height));
        // The translation, then the rotation and the scale about the origin.
        bool aboutZ = s.rotateAxis[0] == 0 && s.rotateAxis[1] == 0;
        Affine turn = aboutZ ? Affine::Rotation(s.rotateAxis[2] < 0 ? -s.rotate : s.rotate)
                             : Affine::Rotation(s.rotate, Vector3(s.rotateAxis[0], s.rotateAxis[1], s.rotateAxis[2]));
        return Affine::Translation(Vector2(o.x + t.x, o.y + t.y)) * turn * Affine::Scaling(Vector2(s.scale[0], s.scale[1])) *
               Affine::Translation(Vector2(-o.x, -o.y));
    }

    Affine WorldTransform(const ElementTree& tree, const LayoutEngine& layout, const StyleEngine& styles, ElementHandle e)
    {
        Affine m;
        for (ElementHandle x = e; x.IsValid() && tree.IsAlive(x); x = tree.Parent(x))
        {
            Affine local = LocalTransform(styles.Computed(x), layout.WorldRect(x));
            if (!local.IsIdentity())
                m = local * m;
        }
        return m;
    }
}
