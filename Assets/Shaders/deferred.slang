import ShaderIO;
import Utils;

ConstantBuffer<shader_io::DeferredUniforms> pushConstants;

SamplerState gNearest : register(s0);
SamplerState gLinear : register(s1);

[shader("compute")]
[numthreads(shader_io::kDeferredThreadX, shader_io::kDeferredThreadY, 1)]
void computeMain(const uint3 threadId: SV_DispatchThreadID)
{
    const uint2 px = threadId.xy;

    RWTexture2D<float4> dst = DescriptorHandle<RWTexture2D<float4>>(uint2(pushConstants.dstIndex, 0));

    uint width = 0;
    uint height = 0;
    dst.GetDimensions(width, height);

    if (px.x >= width || px.y >= height)
    {
        return;
    }

    const Texture2D gbuffer0 = DescriptorHandle<Texture2D<float4>>(uint2(pushConstants.gbuffer0Index, 0));
    const Texture2D gbuffer1 = DescriptorHandle<Texture2D<float4>>(uint2(pushConstants.gbuffer1Index, 0));
    const Texture2D gbuffer2 = DescriptorHandle<Texture2D<float4>>(uint2(pushConstants.gbuffer2Index, 0));
    const Texture2D gbuffer3 = DescriptorHandle<Texture2D<float4>>(uint2(pushConstants.gbuffer3Index, 0));

    
}
