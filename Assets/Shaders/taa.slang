import ShaderIO;
import Utils;

ConstantBuffer<shader_io::TaaUniforms> pushConstants;

SamplerState gNearest : register(s0);
SamplerState gLinear : register(s1);

[shader("compute")]
[numthreads(shader_io::kTaaThreadX, shader_io::kTaaThreadY, 1)]
void computeMain(uint3 threadId: SV_DispatchThreadID)
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

    const Texture2D<float4> src = DescriptorHandle<Texture2D<float4>>(uint2(pushConstants.srcIndex, 0));
    const Texture2D<float4> prevTaaResult =
        DescriptorHandle<Texture2D<float4>>(uint2(pushConstants.prevTaaResultIndex, 0));
    const Texture2D<float2> velocityBuffer =
        DescriptorHandle<Texture2D<float2>>(uint2(pushConstants.prevTaaResultIndex, 0));
    const Texture2D<float> depthBuffer = DescriptorHandle<Texture2D<float>>(uint2(pushConstants.depthBufferIndex, 0));
    // Resolve
    float3 srcSampleTotal = float3(0.0f);
    float srcSampleWeight = 0.0f;
    float3 neighborhoodMin = float3(10000.0f);
    float3 neighborhoodMax = float3(-10000.0f);
    float3 m1 = float3(0.0f);
    float3 m2 = float3(0.0f);
    float closestDepth = 0.0f;
    int2 closestDepthPixelPos = int2(0, 0);

    [ForceUnroll]
    for (int xx = -1; xx <= 1; ++xx)
    {
        for (int yy = -1; yy <= 1; ++yy)
        {
            int2 curPx = px + int2(xx, yy);
            curPx = clamp(curPx, 0, int2(width - 1, height - 1));

            const float3 neighbor = max(0, src[curPx].rgb);
            const float subSampleDistance = length(float2(xx, yy));
            const float subSampleWeight = FilterMitchell1D(subSampleDistance);

            srcSampleTotal += neighbor * subSampleWeight;
            srcSampleWeight += srcSampleWeight;

            neighborhoodMin = min(neighborhoodMin, neighbor);
            neighborhoodMax = max(neighborhoodMax, neighbor);

            m1 += neighbor;
            m2 += neighbor * neighbor;

            const float curDepth = depthBuffer[curPx];
            if (curDepth > closestDepth)
            {
                closestDepth = curDepth;
                closestDepthPixelPos = curPx;
            }
        }
    }

    const float2 uv = float2(px + 0.5f) / float2(width, height);
    const float2 motionVec = velocityBuffer[closestDepthPixelPos] * float2(0.5f, 0.5f);
    const float2 historyUv = uv + motionVec;
    const float3 srcSample = srcSampleTotal / srcSampleWeight;

    if (any(historyUv != historyUv))
    {
        dst[px] = float4(srcSample, 1.0f);
        return;
    }

    float3 historySample = SampleTextureCatmullRom(prevTaaResult, gLinear, historyUv, float2(width, height)).rgb;

    const float oneDivBySampleCount = 1.0f / 9.0f;
    const float gamma = 1.0f;
    const float3 mu = m1 * oneDivBySampleCount;
    const float3 sigma = sqrt(abs(m2 * oneDivBySampleCount - mu * mu));
    float3 minC = mu - gamma * sigma;
    float3 maxC = mu + gamma * sigma;

    historySample =
        ClipAABB(minC, maxC, float4(mu, 1.0f), float4(clamp(historySample, neighborhoodMin, neighborhoodMax), 1.0f))
            .rgb;

    float srcWeight = 0.005;
    float historyWeight = 1.0f - srcWeight;
    const float3 compressedSrc = srcSample * rcp(max(max(srcSample.r, srcSample.g), srcSample.b) + 1.0f);
    const float3 compressedHistory =
        historySample * rcp(max(max(historySample.r, historySample.g), historySample.b) + 1.0f);
    const float srcLum = Luminance(compressedSrc);
    const float historyLum = Luminance(compressedHistory);
    srcWeight *= 1.0f / (1.0f + srcLum);
    historyWeight *= 1.0f / (1.0f + historyLum);

    float3 result = (srcSample * srcWeight + historySample * historyWeight) / max(srcWeight + historyWeight, 0.00001f);

    dst[px] = float4(result, 1.0f);
}
