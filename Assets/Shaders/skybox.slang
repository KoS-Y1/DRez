import ShaderIO;
import Utils;

ConstantBuffer<shader_io::SkyboxUniforms> pushConstants;

SamplerState gNearest : register(s0);
SamplerState gLinear : register(s1);

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 worldPos : POSITION;
}

[shader("vertex")]
VSOut vertexMain(float3 position : POSITION)
{
    VSOut output;

    const float4 clip = mul(float4(position, 1.0f), pushConstants.viewProj);

    // Force z = w so NDC.z = 1.0 (far plane); LESS_EQUAL depth test keeps skybox behind everything
    output.sv_position = float4(clip.xy, clip.w, clip.w);
    output.worldPos = position;

    return output;
}

[shader("pixel")]
float4 fragmentMain(VSOut input) : SV_Target0
{
    const Texture2D<float4> skybox = (DescriptorHandle<Texture2D<float4>>)uint2(pushConstants.skyboxIndex, 0);

    const float2 uv = SampleSphericalMap(normalize(input.worldPos));
    return RgbmToLinear(skybox.SampleLevel(gLinear, uv, 0));
}
