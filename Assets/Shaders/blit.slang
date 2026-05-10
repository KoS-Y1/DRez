import ShaderIO;
import Utils;

ConstantBuffer<shader_io::BlitUniforms> pushConstants;

SamplerState g_nearest : register(s0);
SamplerState g_linear : register(s1);

[shader("compute")]
[numthreads(shader_io::kBlitThreadX, shader_io::kBlitThreadY, 1)]
void compiteMain(const uint3 threadId: SV_DispatchThreadID)
{

    const uint2 px = threadId.xy;

    const Texture2D src = (DescriptorHandle<Texture2D<float4>>)(uint2(pushConstants.srcIndex, 0));
    RWTexture2D<float4> dst = (DescriptorHandle<RWTexture2D<float4>>)(uint2(pushConstants.dstIndex, 0));

    uint width = 0;
    uint height = 0;
    dst.GetDimensions(width, height);

    if (px.x >= width || px.y >= height)
    {
        return;
    }

    const float2 uv = (float2(px) + 0.5f) / float2(width, height);

    float3 hdr;
    if (pushConstants.samplerType == shader_io::SamplerType.Linear)
    {
        hdr = src.SampleLevel(g_linear, uv, 0).rgb;
    }
    else
    {
        hdr = src.SampleLevel(g_nearest, uv, 0).rgb;
    }

    const float3 mapped = LinearToSrgb(Tonemap(hdr));

    dst[px] = float4(saturate(mapped), 1.0f);
}
