using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Helix.Interop;
using Helix.Scripting;

namespace HelixEngine.UI
{
    public enum LineJoin
    {
        Miter,
        Bevel,
        Round,
    }

    public enum LineCap
    {
        Butt,
        Round,
    }

    public enum FillRule
    {
        NonZero,
        OddEven,
    }

    public enum ArcDirection
    {
        Clockwise,
        CounterClockwise,
    }

    // A vertex of a mesh a script draws: its place in the element (pixels, y down), its tint, its uv.
    public struct Vertex
    {
        public static readonly float nearZ = -1, farZ = 0;
        public Vector3 position;
        public Color32 tint;
        public Vector2 uv;
    }

    // The vertices and indices of a mesh MeshGenerationContext.Allocate gave; they are drawn once the script returns.
    public class MeshWriteData
    {
        internal Vertex[] vertices;
        internal ushort[] indices;
        internal Texture2D texture;
        int m_NextVertex, m_NextIndex;

        internal MeshWriteData(int vertexCount, int indexCount, Texture2D texture)
        {
            vertices = new Vertex[vertexCount];
            indices = new ushort[indexCount];
            this.texture = texture;
        }

        public int vertexCount => vertices.Length;
        public int indexCount => indices.Length;
        public Rect uvRegion => new Rect(0, 0, 1, 1);

        public void SetNextVertex(Vertex vertex) => vertices[m_NextVertex++] = vertex;
        public void SetNextIndex(ushort index) => indices[m_NextIndex++] = index;

        public void SetAllVertices(Vertex[] vertices)
        {
            if (vertices.Length != this.vertices.Length)
                throw new ArgumentException("The vertices must be as many as allocated.");
            Array.Copy(vertices, this.vertices, vertices.Length);
            m_NextVertex = vertices.Length;
        }

        public void SetAllIndices(ushort[] indices)
        {
            if (indices.Length != this.indices.Length)
                throw new ArgumentException("The indices must be as many as allocated.");
            Array.Copy(indices, this.indices, indices.Length);
            m_NextIndex = indices.Length;
        }
    }

    // The native painting functions (the element's context during the panel's draw, or a painter of no panel).
    internal static unsafe class PaintNative
    {
        internal static delegate* unmanaged[Cdecl]<ulong> create;
        internal static delegate* unmanaged[Cdecl]<ulong, void> destroy;
        internal static delegate* unmanaged[Cdecl]<ulong, int, float*, void> state;
        internal static delegate* unmanaged[Cdecl]<ulong, float*, int, void> gradient;
        internal static delegate* unmanaged[Cdecl]<ulong, int, float*, void> path;
        internal static delegate* unmanaged[Cdecl]<ulong, float*, float*, float*, int, ushort*, int, ulong, void> mesh;
        internal static delegate* unmanaged[Cdecl]<ulong, byte*, int, float, float, float, float*, void> text;
        internal static delegate* unmanaged[Cdecl]<ulong, ulong, float, float, float, float, float, void> vectorImage;
        internal static delegate* unmanaged[Cdecl]<ulong, ulong, byte> save;

#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Start()
        {
            void* set = Host.FindExport("HelixUI.SetPaintCallback");
            if (set == null)
                return;
            create = (delegate* unmanaged[Cdecl]<ulong>)Host.FindExport("HelixUI.PaintCreate");
            destroy = (delegate* unmanaged[Cdecl]<ulong, void>)Host.FindExport("HelixUI.PaintDestroy");
            state = (delegate* unmanaged[Cdecl]<ulong, int, float*, void>)Host.FindExport("HelixUI.PaintState");
            gradient = (delegate* unmanaged[Cdecl]<ulong, float*, int, void>)Host.FindExport("HelixUI.PaintGradient");
            path = (delegate* unmanaged[Cdecl]<ulong, int, float*, void>)Host.FindExport("HelixUI.PaintPath");
            mesh = (delegate* unmanaged[Cdecl]<ulong, float*, float*, float*, int, ushort*, int, ulong, void>)Host.FindExport("HelixUI.PaintMesh");
            text = (delegate* unmanaged[Cdecl]<ulong, byte*, int, float, float, float, float*, void>)Host.FindExport("HelixUI.PaintText");
            vectorImage = (delegate* unmanaged[Cdecl]<ulong, ulong, float, float, float, float, float, void>)Host.FindExport("HelixUI.PaintVectorImage");
            save = (delegate* unmanaged[Cdecl]<ulong, ulong, byte>)Host.FindExport("HelixUI.PaintSave");
            delegate* unmanaged[Cdecl]<uint, uint, ulong, void> paint = &Paint;
            ((delegate* unmanaged[Cdecl]<void*, void>)set)(paint);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        static void Paint(uint index, uint generation, ulong context)
        {
            try
            {
                Element e = Element.FromHandle(index, generation);
                if (e?.generateVisualContent == null)
                    return;
                var mgc = new MeshGenerationContext(e, context);
                e.generateVisualContent(mgc);
                mgc.Finish();
            }
            catch (Exception ex)
            {
                Debug.LogException(ex);
            }
        }
    }

    // What an element's generateVisualContent draws with: its painter, meshes, texts and vector images, in the
    // element's space.
    public class MeshGenerationContext
    {
        readonly ulong m_Context;
        Painter2D m_Painter;
        readonly List<MeshWriteData> m_Meshes = new List<MeshWriteData>();

        internal MeshGenerationContext(Element element, ulong context)
        {
            this.element = element;
            m_Context = context;
        }

        public Element element { get; }
        // The native paint context (what ImguiContainer's Repaint draws into).
        internal ulong nativeContext => m_Context;
        public Painter2D painter2D => m_Painter ??= new Painter2D(m_Context);

        public MeshWriteData Allocate(int vertexCount, int indexCount, Texture2D texture = null)
        {
            var data = new MeshWriteData(vertexCount, indexCount, texture);
            m_Meshes.Add(data);
            return data;
        }

        public unsafe void DrawText(string text, Vector2 pos, float fontSize, Color color)
        {
            Flush();
            byte[] bytes = Encoding.UTF8.GetBytes(text ?? "");
            float* c = stackalloc float[4] { color.r, color.g, color.b, color.a };
            fixed (byte* b = bytes)
                PaintNative.text(m_Context, b, bytes.Length, pos.x, pos.y, fontSize, c);
        }

        public unsafe void DrawVectorImage(VectorImage vectorImage, Vector2 offset, Angle rotationAngle, Vector2 scale)
        {
            if (vectorImage == null)
                return;
            Flush();
            PaintNative.vectorImage(m_Context, ((INativeObject)vectorImage).HxId, offset.x, offset.y, rotationAngle.ToDegrees(), scale.x, scale.y);
        }

        // The meshes allocated so far, in their order.
        internal unsafe void Flush()
        {
            foreach (MeshWriteData d in m_Meshes)
            {
                int n = d.vertices.Length;
                var positions = new float[n * 3];
                var colors = new float[n * 4];
                var uvs = new float[n * 2];
                for (int i = 0; i < n; i++)
                {
                    Vertex v = d.vertices[i];
                    positions[i * 3] = v.position.x;
                    positions[i * 3 + 1] = v.position.y;
                    positions[i * 3 + 2] = v.position.z;
                    colors[i * 4] = v.tint.r / 255f;
                    colors[i * 4 + 1] = v.tint.g / 255f;
                    colors[i * 4 + 2] = v.tint.b / 255f;
                    colors[i * 4 + 3] = v.tint.a / 255f;
                    uvs[i * 2] = v.uv.x;
                    uvs[i * 2 + 1] = v.uv.y;
                }
                ulong texture = d.texture != null ? ((INativeObject)d.texture).HxId : 0;
                fixed (float* p = positions, c = colors, u = uvs)
                fixed (ushort* x = d.indices)
                    PaintNative.mesh(m_Context, p, c, u, n, x, d.indices.Length, texture);
            }
            m_Meshes.Clear();
        }

        internal void Finish() => Flush();
    }

    // Paths of lines, arcs and curves, filled and stroked: an element's painter during its generateVisualContent, or
    // a painter of its own (new Painter2D) whose paths SaveToVectorImage keeps.
    public class Painter2D : IDisposable
    {
        ulong m_Context;
        readonly bool m_Own;
        float m_LineWidth = 1, m_MiterLimit = 10;
        Color m_StrokeColor = Color.black, m_FillColor = Color.white;
        LineJoin m_LineJoin = LineJoin.Miter;
        LineCap m_LineCap = LineCap.Butt;
        Gradient m_StrokeGradient;
        float[] m_Dashes = Array.Empty<float>();

        internal Painter2D(ulong context) { m_Context = context; }

        public unsafe Painter2D()
        {
            m_Own = true;
            m_Context = PaintNative.create != null ? PaintNative.create() : 0;
        }

        public void Dispose()
        {
            if (m_Own && m_Context != 0)
                unsafe
                {
                    PaintNative.destroy(m_Context);
                }
            m_Context = 0;
        }

        unsafe void State(int which, float a, float b = 0, float c = 0, float d = 0)
        {
            if (m_Context == 0)
                return;
            float* v = stackalloc float[4] { a, b, c, d };
            PaintNative.state(m_Context, which, v);
        }

        unsafe void Path(int op, float a = 0, float b = 0, float c = 0, float d = 0, float e = 0, float f = 0)
        {
            if (m_Context == 0)
                return;
            float* v = stackalloc float[6] { a, b, c, d, e, f };
            PaintNative.path(m_Context, op, v);
        }

        public float lineWidth
        {
            get => m_LineWidth;
            set
            {
                m_LineWidth = value;
                State(0, value);
            }
        }

        public Color strokeColor
        {
            get => m_StrokeColor;
            set
            {
                m_StrokeColor = value;
                State(1, value.r, value.g, value.b, value.a);
            }
        }

        public Color fillColor
        {
            get => m_FillColor;
            set
            {
                m_FillColor = value;
                State(2, value.r, value.g, value.b, value.a);
            }
        }

        public LineJoin lineJoin
        {
            get => m_LineJoin;
            set
            {
                m_LineJoin = value;
                State(3, (int)value);
            }
        }

        public LineCap lineCap
        {
            get => m_LineCap;
            set
            {
                m_LineCap = value;
                State(4, (int)value);
            }
        }

        public float miterLimit
        {
            get => m_MiterLimit;
            set
            {
                m_MiterLimit = value;
                State(5, value);
            }
        }

        // A gradient along the stroke (null: strokeColor).
        public unsafe Gradient strokeGradient
        {
            get => m_StrokeGradient;
            set
            {
                m_StrokeGradient = value;
                if (m_Context == 0)
                    return;
                // Its colors sampled at its keys' times (and between them) as the painter's keys.
                var times = new SortedSet<float> { 0, 1 };
                if (value != null)
                {
                    foreach (GradientColorKey k in value.colorKeys)
                        times.Add(k.time);
                    foreach (GradientAlphaKey k in value.alphaKeys)
                        times.Add(k.time);
                }
                var keys = new float[value == null ? 0 : times.Count * 5];
                int i = 0;
                if (value != null)
                    foreach (float t in times)
                    {
                        Color c = value.Evaluate(t);
                        keys[i++] = t;
                        keys[i++] = c.r;
                        keys[i++] = c.g;
                        keys[i++] = c.b;
                        keys[i++] = c.a;
                    }
                fixed (float* k = keys)
                    PaintNative.gradient(m_Context, k, keys.Length / 5);
            }
        }

        public float dashOffset { get; set; }
        public void SetDashPattern(float[] pattern) => m_Dashes = pattern ?? Array.Empty<float>();
        public float[] GetDashPattern() => (float[])m_Dashes.Clone();

        public void BeginPath() => Path(0);
        public void ClosePath() => Path(1);
        public void MoveTo(Vector2 pos) => Path(2, pos.x, pos.y);
        public void LineTo(Vector2 pos) => Path(3, pos.x, pos.y);

        public void Arc(Vector2 center, float radius, Angle startAngle, Angle endAngle, ArcDirection direction = ArcDirection.Clockwise) =>
            Path(4, center.x, center.y, radius, startAngle.ToDegrees(), endAngle.ToDegrees(), direction == ArcDirection.CounterClockwise ? 1 : 0);

        public void ArcTo(Vector2 p1, Vector2 p2, float radius) => Path(5, p1.x, p1.y, p2.x, p2.y, radius);
        public void QuadraticCurveTo(Vector2 p1, Vector2 p2) => Path(6, p1.x, p1.y, p2.x, p2.y);
        public void BezierCurveTo(Vector2 p1, Vector2 p2, Vector2 p3) => Path(7, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
        public void Fill(FillRule fillRule = FillRule.NonZero) => Path(8, fillRule == FillRule.OddEven ? 1 : 0);
        public void Stroke() => Path(9);

        // A painter of its own starts over.
        public void Clear()
        {
            if (!m_Own)
                return;
            Dispose();
            unsafe
            {
                m_Context = PaintNative.create != null ? PaintNative.create() : 0;
            }
            lineWidth = m_LineWidth;
            strokeColor = m_StrokeColor;
            fillColor = m_FillColor;
            lineJoin = m_LineJoin;
            lineCap = m_LineCap;
            miterLimit = m_MiterLimit;
        }

        public unsafe bool SaveToVectorImage(VectorImage vectorImage)
        {
            if (vectorImage == null || m_Context == 0)
                return false;
            return PaintNative.save(m_Context, ((INativeObject)vectorImage).HxId) != 0;
        }
    }

    public partial class Element
    {
        Action<MeshGenerationContext> m_GenerateVisualContent;

        // Draws the element's content of its own (the panel calls it when it draws the element).
        public Action<MeshGenerationContext> generateVisualContent
        {
            get => m_GenerateVisualContent;
            set
            {
                m_GenerateVisualContent = value;
                if (value != null)
                    KeepForCallbacks();
                SetScriptPaintInternal(value != null);
            }
        }
    }
}
