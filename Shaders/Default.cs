using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.UI.Shaders;

/// <summary>
/// Hidden/UI/Default: every quad and triangle of the panels, blended over the image without depth. The vertices are in
/// pixels (the panel's projection maps them to the screen); eight inputs, as many as the engine binds. What a vertex is
/// comes with it (kind in parameters.x):
/// 0 a flat color, 1 a signed distance field glyph, 2 a multi channel one, 3 a color image, 4 a coverage glyph, 5 a
/// rounded box's fill, 6 its border (parameters.y: the side, or -1 for all), 7 an image inside a rounded box. Rounded
/// boxes come as their size, corner radii and border widths, the point as its place in the box; their edges are
/// antialiased from the box's distance (the mesh scales a transformed box's distances to pixels). Every fragment is
/// clipped to a rounded rectangle (one elliptical corner for all four: its radii in size.z and size.w): the vertex gives its place in it and its size, in
/// the space of the element that clips (rotated and scaled with it). Four texture
/// slots (parameters.w names the vertex's). Colors come premultiplied by their alpha.
/// </summary>
[Shader("Hidden/UI/Default")]
public sealed class Default : ShaderDefinition
{
    [Property("Texture 0"), MainTexture, DefaultTexture("white")] public Texture2D<float4> _Tex0;
    [Property("Texture 1"), DefaultTexture("white")] public Texture2D<float4> _Tex1;
    [Property("Texture 2"), DefaultTexture("white")] public Texture2D<float4> _Tex2;
    [Property("Texture 3"), DefaultTexture("white")] public Texture2D<float4> _Tex3;

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float4 color;
        [Location(1)] public float4 uvLocal;
        [Location(2)] public float4 parameters;
        [Location(3)] public float4 size;
        [Location(4)] public float4 radii;
        [Location(5)] public float4 borders;
        // The point's place in its clip rectangle, and the rectangle's size.
        [Location(6)] public float4 clipLocal;
    }

    [Pass("UI", ZWrite = false, ZTest = CompareFunction.Always, Cull = CullMode.Off)]
    [Blend(BlendFactor.One, BlendFactor.OneMinusSrcAlpha)]
    public static class Draw
    {
        [Texture(0)] public static Texture2D<float4> _Tex0;
        [Texture(1)] public static Texture2D<float4> _Tex1;
        [Texture(2)] public static Texture2D<float4> _Tex2;
        [Texture(3)] public static Texture2D<float4> _Tex3;
        [Sampler(0)] public static SamplerState sampler0;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Color)] float4 color,
                                      [Location(VertexInputs.TexCoord0)] float4 uvLocal, [Location(VertexInputs.TexCoord1)] float4 parameters,
                                      [Location(VertexInputs.TexCoord2)] float4 size, [Location(VertexInputs.TexCoord3)] float4 radii,
                                      [Location(VertexInputs.TexCoord4)] float4 borders, [Location(VertexInputs.TexCoord5)] float4 clipRect)
        {
            Varyings o;
            o.position = ShaderVariables.ObjectToClip(position);
            o.color = color;
            o.uvLocal = uvLocal;
            o.parameters = parameters;
            o.size = size;
            o.radii = radii;
            o.borders = borders;
            // The mesh gives the place in the clip itself (the clip may turn with its element).
            o.clipLocal = clipRect;
            return o;
        }

        // The distance of p from a box of half size h with elliptical corners (horizontal radii rx, vertical ry: top left,
        // top right, bottom right, bottom left; y down), negative inside. In a corner the ellipse's implicit function over
        // its gradient (exact for circles).
        static float RoundedBox(float2 p, float2 h, float4 rx, float4 ry)
        {
            float cx = p.x > 0.0f ? (p.y > 0.0f ? rx.z : rx.y) : (p.y > 0.0f ? rx.w : rx.x);
            float cy = p.x > 0.0f ? (p.y > 0.0f ? ry.z : ry.y) : (p.y > 0.0f ? ry.w : ry.x);
            float2 a = Abs(p);
            float2 r = new float2(Max(cx, 0.0001f), Max(cy, 0.0001f));
            float2 q = a - h + r;
            float box = Max(a.x - h.x, a.y - h.y);
            float2 e = new float2(q.x / r.x, q.y / r.y);
            float k = Length(e);
            float2 g = new float2(q.x / (r.x * r.x), q.y / (r.y * r.y));
            float corner = (k - 1.0f) * k / Max(Length(g), 0.000001f);
            bool inCorner = q.x > 0.0f && q.y > 0.0f && cx > 0.0f && cy > 0.0f;
            return inCorner ? corner : box;
        }

        // Coverage of a point at `local` in a box of `size` with elliptical corners (antialiased over a pixel).
        static float Inside(float2 local, float2 size, float4 rx, float4 ry)
        {
            float2 h = size * 0.5f;
            return Saturate(0.5f - RoundedBox(local - h, h, rx, ry));
        }

        // The box inside the borders: its place, size and corners (as CSS's: each corner's horizontal radius less the
        // side's border, its vertical radius less the top's or the bottom's).
        static float InsideBorders(float2 local, float2 size, float4 radii, float4 borders)
        {
            float2 inner = new float2(Max(size.x - borders.x - borders.z, 0.0f), Max(size.y - borders.y - borders.w, 0.0f));
            float4 rx = new float4(Max(radii.x - borders.x, 0.0f), Max(radii.y - borders.z, 0.0f), Max(radii.z - borders.z, 0.0f),
                                   Max(radii.w - borders.x, 0.0f));
            float4 ry = new float4(Max(radii.x - borders.y, 0.0f), Max(radii.y - borders.y, 0.0f), Max(radii.z - borders.w, 0.0f),
                                   Max(radii.w - borders.w, 0.0f));
            if (inner.x <= 0.0f || inner.y <= 0.0f)
                return 0.0f;
            return Inside(local - new float2(borders.x, borders.y), inner, rx, ry);
        }

        // How much a point of the border belongs to a side (0 left, 1 top, 2 right, 3 bottom): the sides meet where their
        // distances relative to the widths are equal (the corners join on their diagonals), antialiased over a pixel.
        static float SideWeight(float2 local, float2 size, float4 borders, float side)
        {
            float left = local.x / Max(borders.x, 0.0001f);
            float top = local.y / Max(borders.y, 0.0001f);
            float right = (size.x - local.x) / Max(borders.z, 0.0001f);
            float bottom = (size.y - local.y) / Max(borders.w, 0.0001f);
            float mine = left;
            float other = Min(top, Min(right, bottom));
            float width = borders.x;
            if (side > 0.5f && side < 1.5f)
            {
                mine = top;
                other = Min(left, Min(right, bottom));
                width = borders.y;
            }
            else if (side > 1.5f && side < 2.5f)
            {
                mine = right;
                other = Min(left, Min(top, bottom));
                width = borders.z;
            }
            else if (side > 2.5f)
            {
                mine = bottom;
                other = Min(left, Min(top, right));
                width = borders.w;
            }
            return Saturate(0.5f + (other - mine) * width * 0.7071f);
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float kind = input.parameters.x;
            // The clip: a rounded rectangle on the screen.
            float4 clipX = new float4(input.size.z, input.size.z, input.size.z, input.size.z);
            float4 clipY = new float4(input.size.w, input.size.w, input.size.w, input.size.w);
            float clip = Inside(new float2(input.clipLocal.x, input.clipLocal.y), new float2(input.clipLocal.z, input.clipLocal.w), clipX, clipY);
            float4 t0 = _Tex0.Sample(sampler0, input.uvLocal.xy);
            float4 t1 = _Tex1.Sample(sampler0, input.uvLocal.xy);
            float4 t2 = _Tex2.Sample(sampler0, input.uvLocal.xy);
            float4 t3 = _Tex3.Sample(sampler0, input.uvLocal.xy);
            float slot = input.parameters.w;
            float4 texel = slot < 0.5f ? t0 : (slot < 1.5f ? t1 : (slot < 2.5f ? t2 : t3));
            float2 local = new float2(input.uvLocal.z, input.uvLocal.w);
            float2 size = new float2(input.size.x, input.size.y);
            float coverage = 1.0f;
            float4 color = input.color;
            if (kind < 0.5f)
                coverage = 1.0f;
            else if (kind > 4.5f && kind < 5.5f)
                coverage = InsideBorders(local, size, input.radii, input.borders);
            else if (kind > 5.5f && kind < 6.5f)
            {
                coverage = Inside(local, size, input.radii, input.radii) * (1.0f - InsideBorders(local, size, input.radii, input.borders));
                if (input.parameters.y > -0.5f)
                    coverage = coverage * SideWeight(local, size, input.borders, input.parameters.y);
            }
            else if (kind > 2.5f && kind < 3.5f || kind > 6.5f)
            {
                // Straight alpha in the image: premultiplied here, tinted by the vertex color.
                float a = texel.w * color.w;
                color = new float4(texel.x * color.x * texel.w, texel.y * color.y * texel.w, texel.z * color.z * texel.w, a);
                if (kind > 6.5f)
                    coverage = InsideBorders(local, size, input.radii, input.borders);
            }
            else if (kind > 3.5f)
                coverage = texel.x;
            else
            {
                float distance = kind < 1.5f ? texel.x : Max(Min(texel.x, texel.y), Min(Max(texel.x, texel.y), texel.z));
                coverage = Saturate((distance - input.parameters.y) * input.parameters.z + 0.5f);
            }
            return color * (coverage * clip);
        }
    }
}
