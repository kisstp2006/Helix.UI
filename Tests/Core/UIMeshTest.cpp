#include <Helix/UI/Rendering/UIMesh.h>
#include <Helix/Testing/Test.h>

#include <cmath>
#include <cstdio>

using namespace helix;
using namespace helix::ui;

namespace
{
    bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.001f; }

    UIVertexKind KindOf(const UIVertex& v) { return UIVertexKind(u8(v.parameters.x + 0.5f)); }

    // The bounds of the quad starting at vertex `first`.
    Rect QuadBounds(const UIMesh& m, usize first)
    {
        Span<const UIVertex> v = m.Vertices();
        f32 x0 = v[first].position.x, y0 = v[first].position.y, x1 = x0, y1 = y0;
        for (usize k = first; k < first + 4; k++)
        {
            x0 = v[k].position.x < x0 ? v[k].position.x : x0;
            y0 = v[k].position.y < y0 ? v[k].position.y : y0;
            x1 = v[k].position.x > x1 ? v[k].position.x : x1;
            y1 = v[k].position.y > y1 ? v[k].position.y : y1;
        }
        return Rect(x0, y0, x1 - x0, y1 - y0);
    }

    bool RectIs(const Rect& r, f32 x, f32 y, f32 w, f32 h)
    {
        bool ok = Near(r.x, x) && Near(r.y, y) && Near(r.width, w) && Near(r.height, h);
        if (!ok)
            std::printf("    rect %g %g %g %g, expected %g %g %g %g\n", r.x, r.y, r.width, r.height, x, y, w, h);
        return ok;
    }

    UIShape Box()
    {
        UIShape s;
        s.rect = Rect(10, 20, 100, 50);
        s.radii = Vector4(8, 8, 8, 8);
        s.fill = Color(1, 0, 0, 1);
        return s;
    }
}

HX_TEST(UIMesh, AShapeIsItsFillAndOneBorderQuadOfOneColor)
{
    UIMesh m;
    UIShape s = Box();
    for (u32 k = 0; k < 4; k++)
    {
        s.borders[k] = 2;
        s.borderColors[k] = Color(0, 0, 1, 1);
    }
    m.Shape(s, 1);
    HX_CHECK_EQ(m.Vertices().Size(), usize(8));
    HX_CHECK_EQ(m.Indices().Size(), usize(12));
    HX_CHECK_EQ(m.DrawCalls(), 1u);
    const UIVertex& fill = m.Vertices()[0];
    const UIVertex& border = m.Vertices()[4];
    HX_CHECK(KindOf(fill) == UIVertexKind::ShapeFill);
    HX_CHECK(KindOf(border) == UIVertexKind::ShapeBorder);
    HX_CHECK(Near(border.parameters.y, -1));
    HX_CHECK(Near(fill.size.x, 100) && Near(fill.size.y, 50));
    HX_CHECK(Near(fill.radii.x, 8) && Near(fill.borders.w, 2));
    HX_CHECK(RectIs(QuadBounds(m, 0), 10, 20, 100, 50));
    // The point's place in the box.
    HX_CHECK(Near(m.Vertices()[2].uvLocal.z, 100) && Near(m.Vertices()[2].uvLocal.w, 50));
}

HX_TEST(UIMesh, BordersOfFourColorsAreTheirSidesAndClearPartsDrawNothing)
{
    UIMesh m;
    UIShape s = Box();
    s.fill = Color(0, 0, 0, 0);
    s.borders[0] = 1, s.borders[1] = 2, s.borders[2] = 0, s.borders[3] = 4;
    s.borderColors[0] = Color(1, 0, 0, 1), s.borderColors[1] = Color(0, 1, 0, 1), s.borderColors[2] = Color(0, 0, 1, 1),
    s.borderColors[3] = Color(1, 1, 0, 1);
    m.Shape(s, 1);
    // No fill, no right side.
    HX_CHECK_EQ(m.Vertices().Size(), usize(12));
    HX_CHECK(Near(m.Vertices()[0].parameters.y, 0));
    HX_CHECK(Near(m.Vertices()[4].parameters.y, 1));
    HX_CHECK(Near(m.Vertices()[8].parameters.y, 3));

    // Opacity premultiplies the colors.
    m.Clear();
    m.Shape(Box(), 0.5f);
    const Color& c = m.Vertices()[0].color;
    HX_CHECK(Near(c.r, 0.5f) && Near(c.a, 0.5f) && Near(c.g, 0));
}

HX_TEST(UIMesh, ABatchTakesFourTexturesThenANewOneStarts)
{
    UIMesh m;
    int textures[5];
    const f32 none[4] = {0, 0, 0, 0};
    for (int& t : textures)
    {
        UIImage image;
        image.texture = &t;
        m.Image(Rect(0, 0, 10, 10), image, Vector4(), none, 1);
    }
    HX_CHECK_EQ(m.DrawCalls(), 2u);
    Span<const UIBatch> b = m.Batches();
    for (u32 k = 0; k < 4; k++)
        HX_CHECK(b[0].textures[k] == &textures[k]);
    HX_CHECK(b[1].textures[0] == &textures[4]);
    HX_CHECK_EQ(b[0].indexCount, 24u);
    HX_CHECK_EQ(b[1].firstIndex, 24u);
    // Each vertex names its slot.
    HX_CHECK(Near(m.Vertices()[12].parameters.w, 3));
    HX_CHECK(Near(m.Vertices()[16].parameters.w, 0));

    // The same texture again, and shapes without one, stay in the batch.
    UIImage again;
    again.texture = &textures[4];
    m.Image(Rect(0, 0, 10, 10), again, Vector4(), none, 1);
    m.Shape(Box(), 1);
    HX_CHECK_EQ(m.DrawCalls(), 2u);
    HX_CHECK(Near(m.Vertices()[20].parameters.w, 0));
}

HX_TEST(UIMesh, ImagesFitCropStretchOrSlice)
{
    UIImage image;
    int texture = 0;
    image.texture = &texture;
    image.textureWidth = 200;
    image.textureHeight = 100;
    const f32 none[4] = {0, 0, 0, 0};

    UIMesh m;
    image.sizeMode = UISizeMode::Contain;
    m.Image(Rect(0, 0, 100, 100), image, Vector4(), none, 1);
    HX_CHECK(RectIs(QuadBounds(m, 0), 0, 25, 100, 50));

    m.Clear();
    image.sizeMode = UISizeMode::Cover;
    m.Image(Rect(0, 0, 100, 100), image, Vector4(), none, 1);
    HX_CHECK(RectIs(QuadBounds(m, 0), 0, 0, 100, 100));
    // The middle half of the width.
    HX_CHECK(Near(m.Vertices()[0].uvLocal.x, 0.25f) && Near(m.Vertices()[1].uvLocal.x, 0.75f));

    m.Clear();
    image.sizeMode = UISizeMode::Length;
    const f32 borders[4] = {5, 5, 5, 5};
    m.Image(Rect(0, 0, 100, 100), image, Vector4(10, 10, 10, 10), borders, 1);
    // Inside the borders, its place in the whole box.
    HX_CHECK(RectIs(QuadBounds(m, 0), 5, 5, 90, 90));
    HX_CHECK(Near(m.Vertices()[0].uvLocal.z, 5));
    HX_CHECK(KindOf(m.Vertices()[0]) == UIVertexKind::ShapeImage);

    m.Clear();
    image.slices[0] = image.slices[1] = image.slices[2] = image.slices[3] = 10;
    image.sliceScale = 2;
    m.Image(Rect(0, 0, 100, 60), image, Vector4(), none, 1);
    HX_CHECK_EQ(m.Vertices().Size(), usize(36));
    // The corners keep their size (times the scale), the middle stretches.
    HX_CHECK(RectIs(QuadBounds(m, 0), 0, 0, 20, 20));
    HX_CHECK(RectIs(QuadBounds(m, 16), 20, 20, 60, 20));
    HX_CHECK(RectIs(QuadBounds(m, 32), 80, 40, 20, 20));
    HX_CHECK(Near(m.Vertices()[1].uvLocal.x, 10.0f / 200) && Near(m.Vertices()[2].uvLocal.y, 10.0f / 100));

    // A texture of its own has its rows up: v turns over.
    UIMesh up;
    UIImage own = image;
    own.slices[0] = own.slices[1] = own.slices[2] = own.slices[3] = 0;
    own.rowsUp = true;
    up.Image(Rect(0, 0, 10, 10), own, Vector4(), none, 1);
    HX_CHECK(Near(up.Vertices()[0].uvLocal.y, 1) && Near(up.Vertices()[2].uvLocal.y, 0));

    // Slices larger than the area shrink together.
    m.Clear();
    m.Image(Rect(0, 0, 20, 60), image, Vector4(), none, 1);
    HX_CHECK(RectIs(QuadBounds(m, 0), 0, 0, 10, 20));
}

HX_TEST(UIMesh, VerticesCarryTheirClip)
{
    UIMesh m;
    UIClip outer;
    outer.rect = Rect(0, 0, 100, 100);
    UIClip inner = outer.Within(Rect(50, -10, 100, 40), Vector4(2, 6, 4, 0));
    HX_CHECK(RectIs(inner.rect, 50, 0, 50, 30));
    HX_CHECK(Near(inner.radius, 6));
    m.SetClip(inner);
    m.Rectangle(Rect(0, 0, 10, 10), Color(1, 1, 1, 1));
    m.Shape(Box(), 1);
    const UIVertex& flat = m.Vertices()[0];
    HX_CHECK(KindOf(flat) == UIVertexKind::Flat);
    // Its place in the clip and the clip's size.
    HX_CHECK(Near(flat.clipRect.x, -50) && Near(flat.clipRect.y, 0) && Near(flat.clipRect.z, 50) && Near(flat.clipRect.w, 30));
    HX_CHECK(Near(flat.size.z, 6));
    HX_CHECK(Near(m.Vertices()[4].size.z, 6) && Near(m.Vertices()[4].size.x, 100));

    // Elliptical corners: the largest of each axis, both on the vertices.
    UIClip oval = outer.Within(Rect(10, 10, 50, 50), Vector4(8, 2, 0, 0), Vector4(3, 5, 0, 0), Affine());
    HX_CHECK(Near(oval.radius, 8) && Near(oval.radiusY, 5));
    UIMesh ovalMesh;
    ovalMesh.SetClip(oval);
    ovalMesh.Rectangle(Rect(20, 20, 4, 4), Color(1, 1, 1, 1));
    HX_CHECK(Near(ovalMesh.Vertices()[0].size.z, 8) && Near(ovalMesh.Vertices()[0].size.w, 5));

    // Disjoint clips leave nothing.
    UIClip empty = outer.Within(Rect(200, 200, 10, 10), Vector4());
    HX_CHECK(Near(empty.rect.width, 0) && Near(empty.rect.height, 0));

    // Painted triangles are flat vertices with their indices.
    m.Clear();
    const Vector2 positions[3] = {Vector2(0, 0), Vector2(10, 0), Vector2(0, 10)};
    const Color colors[3] = {Color(1, 0, 0, 1), Color(0, 1, 0, 1), Color(0, 0, 1, 1)};
    const u32 indices[3] = {0, 1, 2};
    m.Rectangle(Rect(0, 0, 1, 1), Color(1, 1, 1, 1));
    m.Triangles(Span<const Vector2>(positions, 3), Span<const Color>(colors, 3), Span<const u32>(indices, 3));
    HX_CHECK_EQ(m.Vertices().Size(), usize(7));
    HX_CHECK_EQ(m.Indices()[8], 6u);
    HX_CHECK(Near(m.Vertices()[5].color.g, 1));
    HX_CHECK_EQ(m.DrawCalls(), 1u);
}
