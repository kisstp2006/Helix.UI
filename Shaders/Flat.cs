using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.UI.Shaders;

/// <summary>
/// Hidden/UI/Flat: the panels' flat quads (backgrounds, borders) in their vertex colors, blended over the image without
/// depth. The vertices are in pixels; the projection the panel sets maps them to the screen.
/// </summary>
[Shader("Hidden/UI/Flat")]
public sealed class Flat : ShaderDefinition
{
    public struct Varyings
    {
        [Position] public float4 position;
        [Location(0)] public float4 color;
    }

    [Pass("UI", ZWrite = false, ZTest = CompareFunction.Always, Cull = CullMode.Off)]
    [Blend(BlendFactor.One, BlendFactor.OneMinusSrcAlpha)]
    public static class Draw
    {
        [VertexShader]
        public static Varyings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Color)] float4 color) =>
            new Varyings { position = ShaderVariables.ObjectToClip(position), color = color };

        // The colors come premultiplied by their alpha.
        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(Varyings input) => input.color;
    }
}
