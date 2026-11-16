using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.UI.Shaders;

/// <summary>
/// Hidden/UI/Default: the panels' quads blended over the image without depth, the vertices in pixels (the panel's
/// projection maps them to the screen). The kind of each quad comes with its vertices (params.x): 0 a flat color, 1 a
/// signed distance field glyph, 2 a multi channel one, 3 a color glyph or image, 4 a coverage glyph. Distance field
/// glyphs reach full coverage params.z field units past the edge params.y (narrower for small text, lower for bold).
/// Colors come premultiplied by their alpha.
/// </summary>
[Shader("Hidden/UI/Default")]
public sealed class Default : ShaderDefinition
{
    [Property("Texture"), MainTexture, DefaultTexture("white")] public Texture2D<float4> _MainTex;

    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float4 color;
        [Location(1)] public float2 uv;
        [Location(2)] public float4 parameters;
    }

    [Pass("UI", ZWrite = false, ZTest = CompareFunction.Always, Cull = CullMode.Off)]
    [Blend(BlendFactor.One, BlendFactor.OneMinusSrcAlpha)]
    public static class Draw
    {
        [Texture(0)] public static Texture2D<float4> _MainTex;
        [Sampler(0)] public static SamplerState mainSampler;

        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Color)] float4 color,
                                      [Location(VertexInputs.TexCoord0)] float2 uv, [Location(VertexInputs.TexCoord1)] float4 parameters) =>
            new Varyings { position = ShaderVariables.ObjectToClip(position), color = color, uv = uv, parameters = parameters };

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input)
        {
            float kind = input.parameters.x;
            if (kind < 0.5f)
                return input.color;
            float4 texel = _MainTex.Sample(mainSampler, input.uv);
            if (kind > 2.5f && kind < 3.5f)
            {
                // Straight alpha in the image: premultiplied here, the vertex alpha fading it.
                float a = texel.w * input.color.w;
                return new float4(texel.x * a, texel.y * a, texel.z * a, a);
            }
            float coverage;
            if (kind > 3.5f)
                coverage = texel.x;
            else
            {
                float distance = kind < 1.5f ? texel.x : Max(Min(texel.x, texel.y), Min(Max(texel.x, texel.y), texel.z));
                coverage = Saturate((distance - input.parameters.y) * input.parameters.z + 0.5f);
            }
            return input.color * coverage;
        }
    }
}
