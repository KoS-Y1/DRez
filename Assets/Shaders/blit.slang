import ShaderIO;
import Utils;

ConstantBuffer<shader_io::BlitUniforms> pushConstants;

[shader("compute")]
[numthreads(shader_io::kBlitThreadX, shader_io::kBlitThreadY, 1)]
void compiteMain(const uint3 threadId: SV_DispatchThreadID)
{

    const uint2 px = threadId.xy;

    const Texture2D src = (DescriptorHandle<Texture2D<float4>>)(uint2(pushConstants.srcIndex, 0));
    // TODO: sampler
    RWTexture2D<float4> dst = (DescriptorHandle<RWTexture2D<float4>>)(uint2(pushConstants.dstIndex, 0));

    uint width = 0;
    uint height = 0;
    dst.GetDimensions(width, height);

    if (px.x >= width || px.y >= height)
    {
        return;
    }

    const float3 hdr = src[px].rgb;
    const float3 mapped = LinearToSrgb(Tonemap(hdr));

    dst[px] = float4(saturate(mapped), 1.0f);
}
