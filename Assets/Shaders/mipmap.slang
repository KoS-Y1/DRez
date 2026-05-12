import ShaderIO;
import Utils;

ConstantBuffer<shader_io::MipmapUniforms> pushConstants;

SamplerState gNearest : register(s0);
SamplerState gLinear : register(s1);

[shader("compute")]
[numthreads(shader_io::kMipmapThreadX, shader_io::kMipmapThreadY, 1)]
void computeMain(uint3 threadId: SV_DispatchThreadID)
{
    const uint2 px = threadId.xy;

    RWTexture2D<float4> dst = (DescriptorHandle<RWTexture2D<float4>>)uint2(pushConstants.dsrIndex, 0);

    uint width = 0;
    uint height = 0;
    dst.GetDimensions(width, height);

    if (px.x >= width || px.y >= height)
    {
        return;
    }

    const Texture2D<float4> src = (DescriptorHandle<Texture2D<float4>>)uint2(pushConstants.srcIndex, 0);

    const float2 uv = (float2(px) + 0.5) / float2(width, height);
    const float4 c = src.SampleLevel(gLinear, uv, pushConstants.srcMipLevel);

    dst[px] = c;
}
