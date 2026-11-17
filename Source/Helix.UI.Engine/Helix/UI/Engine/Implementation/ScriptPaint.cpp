#include <Helix/UI/Engine/ScriptPaint.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/UI/Engine/Panel.h>
#include <Helix/UI/Engine/UIAssets.h>
#include <Helix/UI/Engine/VectorImage.h>

#include <bit>
#include <cmath>

namespace helix::ui
{
    namespace
    {
        using ManagedPaint = void (*)(u32 index, u32 generation, u64 context);
        ManagedPaint managedPaint = nullptr;

        HashMap<u64, bool>& Painted()
        {
            static HashMap<u64, bool>* painted = new HashMap<u64, bool>();
            return *painted;
        }

        u64 Key(ElementHandle e) { return u64(e.index) << 32 | e.generation; }
        ScriptPaintContext* Context(u64 id) { return reinterpret_cast<ScriptPaintContext*>(uintptr_t(id)); }
        Color Rgba(const f32* c) { return Color(c[0], c[1], c[2], c[3]); }
        Color Premultiplied(Color c, f32 opacity) { return Color(c.r * c.a * opacity, c.g * c.a * opacity, c.b * c.a * opacity, c.a * opacity); }

        engine::Object* ObjectOf(u64 id) { return id ? scripting::ObjectHandles<engine::Object>::FromId(id) : nullptr; }

        // The painter's triangles into the mesh, moved to the element's box (a painter of no panel keeps them).
        void Flush(ScriptPaintContext& c)
        {
            if (!c.mesh)
                return;
            Span<const Vector2> positions = c.painter.Positions();
            if (positions.Empty())
                return;
            Array<Vector2> moved;
            moved.Reserve(positions.Size());
            for (Vector2 p : positions)
                moved.Push(Vector2(p.x + c.origin.x, p.y + c.origin.y));
            c.mesh->Triangles(Span<const Vector2>(moved.Data(), moved.Size()), c.painter.Colors(), c.painter.Indices());
            c.painter.Clear();
        }

        // -------------------------------------------------------------------------------------- the exports

        void SetPaintCallback(void* function) { managedPaint = std::bit_cast<ManagedPaint>(function); }

        u64 PaintCreate() { return u64(uintptr_t(new ScriptPaintContext())); }

        void PaintDestroy(u64 context)
        {
            ScriptPaintContext* c = Context(context);
            if (c && !c->mesh)
                delete c;
        }

        // Its state: 0 line width, 1 stroke color, 2 fill color, 3 line join, 4 line cap, 5 miter limit.
        void PaintState(u64 context, i32 which, const f32* v)
        {
            Painter& p = Context(context)->painter;
            switch (which)
            {
            case 0: p.lineWidth = v[0]; break;
            case 1: p.strokeColor = Rgba(v); break;
            case 2: p.fillColor = Rgba(v); break;
            case 3: p.lineJoin = LineJoin(u8(v[0])); break;
            case 4: p.lineCap = LineCap(u8(v[0])); break;
            case 5: p.miterLimit = v[0]; break;
            default: break;
            }
        }

        // A gradient along the stroke: count keys of (time, r, g, b, a).
        void PaintGradient(u64 context, const f32* keys, i32 count)
        {
            Painter& p = Context(context)->painter;
            p.strokeGradient.Clear();
            for (i32 i = 0; i < count; i++)
                p.strokeGradient.Push(GradientKey{keys[i * 5], Color(keys[i * 5 + 1], keys[i * 5 + 2], keys[i * 5 + 3], keys[i * 5 + 4])});
        }

        // A path operation: 0 begin, 1 close, 2 move, 3 line, 4 arc, 5 arc to, 6 quadratic, 7 bezier, 8 fill, 9 stroke.
        void PaintPath(u64 context, i32 op, const f32* a)
        {
            ScriptPaintContext& c = *Context(context);
            Painter& p = c.painter;
            switch (op)
            {
            case 0: p.BeginPath(); break;
            case 1: p.ClosePath(); break;
            case 2: p.MoveTo(Vector2(a[0], a[1])); break;
            case 3: p.LineTo(Vector2(a[0], a[1])); break;
            case 4: p.Arc(Vector2(a[0], a[1]), a[2], a[3], a[4], a[5] != 0 ? ArcDirection::CounterClockwise : ArcDirection::Clockwise); break;
            case 5: p.ArcTo(Vector2(a[0], a[1]), Vector2(a[2], a[3]), a[4]); break;
            case 6: p.QuadraticCurveTo(Vector2(a[0], a[1]), Vector2(a[2], a[3])); break;
            case 7: p.BezierCurveTo(Vector2(a[0], a[1]), Vector2(a[2], a[3]), Vector2(a[4], a[5])); break;
            case 8: p.Fill(a[0] != 0 ? FillRule::OddEven : FillRule::NonZero); break;
            case 9: p.Stroke(); break;
            default: break;
            }
        }

        // A mesh of the script's vertices (x y z, r g b a, u v) and indices, textured or flat.
        void PaintMesh(u64 context, const f32* positions, const f32* colors, const f32* uvs, i32 count, const u16* indices, i32 indexCount, u64 texture)
        {
            ScriptPaintContext& c = *Context(context);
            if (!c.mesh || count <= 0 || indexCount <= 0)
                return;
            Flush(c);
            engine::Object* o = ObjectOf(texture);
            auto* t = o && o->Is<engine::Texture2D>() ? static_cast<engine::Texture2D*>(o) : nullptr;
            Array<Vector2> p;
            Array<Color> k;
            Array<Vector2> uv;
            Array<u32> idx;
            for (i32 i = 0; i < count; i++)
            {
                p.Push(Vector2(positions[i * 3] + c.origin.x, positions[i * 3 + 1] + c.origin.y));
                k.Push(Premultiplied(Rgba(colors + i * 4), c.opacity));
                uv.Push(Vector2(uvs[i * 2], uvs[i * 2 + 1]));
            }
            for (i32 i = 0; i < indexCount; i++)
                if (indices[i] < u32(count))
                    idx.Push(indices[i]);
            c.mesh->TexturedTriangles(Span<const Vector2>(p.Data(), p.Size()), Span<const Color>(k.Data(), k.Size()), Span<const Vector2>(uv.Data(), uv.Size()),
                                      Span<const u32>(idx.Data(), idx.Size()), t);
        }

        // A text in the element's font at a point (its top left) in a size and color.
        void PaintText(u64 context, const char* text, i32 length, f32 x, f32 y, f32 size, const f32* color)
        {
            ScriptPaintContext& c = *Context(context);
            if (!c.mesh || !c.panel)
                return;
            Flush(c);
            c.panel->DrawScriptText(*c.mesh, c.element, StringView(text, usize(length)), Vector2(c.origin.x + x, c.origin.y + y), size, Rgba(color),
                                    c.opacity);
        }

        // A vector image moved, turned (degrees) and scaled.
        void PaintVectorImage(u64 context, u64 image, f32 x, f32 y, f32 angle, f32 sx, f32 sy)
        {
            ScriptPaintContext& c = *Context(context);
            auto* vi = ScriptObjectAs<VectorImage>(ObjectOf(image));
            if (!vi)
                return;
            Flush(c);
            f32 r = angle * 0.017453292519943295f, cs = std::cos(r), sn = std::sin(r);
            Array<Vector2> p;
            Array<Color> k;
            for (Vector2 v : vi->positions())
            {
                f32 px = v.x * sx, py = v.y * sy;
                p.Push(Vector2(c.origin.x + x + px * cs - py * sn, c.origin.y + y + px * sn + py * cs));
            }
            for (Color col : vi->colors())
                k.Push(Color(col.r * c.opacity, col.g * c.opacity, col.b * c.opacity, col.a * c.opacity));
            if (c.mesh)
                c.mesh->Triangles(Span<const Vector2>(p.Data(), p.Size()), Span<const Color>(k.Data(), k.Size()), vi->indices());
        }

        // A painter's triangles kept in a vector image.
        bool PaintSave(u64 context, u64 image)
        {
            ScriptPaintContext& c = *Context(context);
            auto* vi = ScriptObjectAs<VectorImage>(ObjectOf(image));
            if (!vi)
                return false;
            vi->Set(c.painter.Positions(), c.painter.Colors(), c.painter.Indices());
            return true;
        }

        struct ExportRegistrar
        {
            ExportRegistrar()
            {
                scripting::RegisterNativeExport("HelixUI.SetPaintCallback", std::bit_cast<const void*>(&SetPaintCallback));
                scripting::RegisterNativeExport("HelixUI.PaintCreate", std::bit_cast<const void*>(&PaintCreate));
                scripting::RegisterNativeExport("HelixUI.PaintDestroy", std::bit_cast<const void*>(&PaintDestroy));
                scripting::RegisterNativeExport("HelixUI.PaintState", std::bit_cast<const void*>(&PaintState));
                scripting::RegisterNativeExport("HelixUI.PaintGradient", std::bit_cast<const void*>(&PaintGradient));
                scripting::RegisterNativeExport("HelixUI.PaintPath", std::bit_cast<const void*>(&PaintPath));
                scripting::RegisterNativeExport("HelixUI.PaintMesh", std::bit_cast<const void*>(&PaintMesh));
                scripting::RegisterNativeExport("HelixUI.PaintText", std::bit_cast<const void*>(&PaintText));
                scripting::RegisterNativeExport("HelixUI.PaintVectorImage", std::bit_cast<const void*>(&PaintVectorImage));
                scripting::RegisterNativeExport("HelixUI.PaintSave", std::bit_cast<const void*>(&PaintSave));
            }
        } registrar;
    }

    bool HasScriptPaint(ElementWorld& world, ElementHandle e) { return &world == &ElementWorld::Main() && Painted().Find(Key(e)); }

    void SetScriptPaint(ElementWorld&, ElementHandle e, bool on)
    {
        if (on)
            Painted().Set(Key(e), true);
        else
            Painted().Remove(Key(e));
    }

    void RunScriptPaint(Panel& panel, UIMesh& mesh, ElementHandle e, Vector2 origin, f32 opacity)
    {
        if (!managedPaint)
            return;
        ScriptPaintContext c;
        c.mesh = &mesh;
        c.panel = &panel;
        c.element = e;
        c.origin = origin;
        c.opacity = opacity;
        c.painter.opacity = opacity;
        managedPaint(e.index, e.generation, u64(uintptr_t(&c)));
        Flush(c);
    }

    void Element::SetScriptPaintInternal(bool on)
    {
        SetScriptPaint(world(), handle(), on);
        MarkDirtyRepaint();
    }

    void VectorImage::Set(Span<const Vector2> positions, Span<const Color> colors, Span<const u32> indices)
    {
        positions_.Clear();
        colors_.Clear();
        indices_.Clear();
        f32 w = 0, h = 0;
        for (Vector2 p : positions)
        {
            positions_.Push(p);
            w = p.x > w ? p.x : w;
            h = p.y > h ? p.y : h;
        }
        for (Color c : colors)
            colors_.Push(c);
        for (u32 i : indices)
            indices_.Push(i);
        size_ = Vector2(w, h);
    }
}
