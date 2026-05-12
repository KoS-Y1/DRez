import ShaderIO;
import Utils;

ConstantBuffer<shader_io::DeferredUniforms> pushConstants;

SamplerState gNearest : register(s0);
SamplerState gLinear : register(s1);
SamplerComparisonState gShadow : register(s2);

[shader("compute")]
[numthreads(shader_io::kDeferredThreadX, shader_io::kDeferredThreadY, 1)]
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

    const shader_io::DeferredInfo info =
        ((DescriptorHandle<StructuredBuffer<shader_io::DeferredInfo>>)uint2(pushConstants.deferredInfoIndex, 0))[0];

    const Texture2D gbuffer0 = (DescriptorHandle<Texture2D<float4>>)uint2(info.gbuffer0Index, 0);
    const Texture2D gbuffer1 = (DescriptorHandle<Texture2D<float4>>)uint2(info.gbuffer1Index, 0);
    const Texture2D gbuffer2 = (DescriptorHandle<Texture2D<float4>>)uint2(info.gbuffer2Index, 0);
    const Texture2D gbuffer3 = (DescriptorHandle<Texture2D<float4>>)uint2(info.gbuffer3Index, 0);

    const float4 sample0 = gbuffer0[px];
    const float4 sample1 = gbuffer1[px];
    const float4 sample2 = gbuffer2[px];
    const float4 sample3 = gbuffer3[px];

    // Background pixels have an unwritten (zero-length) normal
    if (dot(sample1.xyz, sample1.xyz) < kSmallValue)
    {
        dst[px] = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    const float3 worldPos = sample0.xyz;
    const float3 N = normalize(sample1.xyz);
    const float roughness = sample1.w;
    const float3 albedo = sample2.xyz;
    const float ao = sample2.w;
    const float3 emissive = sample3.xyz;
    const float metallic = sample3.w;

    const float3 V = normalize(pushConstants.cameraPos - worldPos);
    const float3 R = reflect(-V, N);
    const float NdotV = max(dot(N, V), 0.0f);
    const float3 F0 = lerp(float3(0.04f), albedo, metallic);

    const float3 L = normalize(pushConstants.lightDir);
    const float3 brdf =
        CalculateBRDF(N, V, L, albedo, roughness, metallic, pushConstants.lightColor, pushConstants.lightIntensity);

    const Texture2D<float4> irradianceTex = (DescriptorHandle<Texture2D<float4>>)uint2(info.irradianceIndex, 0);
    const Texture2D<float4> specularTex = (DescriptorHandle<Texture2D<float4>>)uint2(info.specularIndex, 0);
    const Texture2D<float4> brdfLutTex = (DescriptorHandle<Texture2D<float4>>)uint2(info.brdfLutIndex, 0);

    const float3 ibl = CalculateIBL(N, NdotV, R, F0, albedo, roughness, metallic, ao, irradianceTex, specularTex,
                                    brdfLutTex, gNearest);

    // Shadow
    const Texture2D<float> shadowMap = (DescriptorHandle<Texture2D<float>>)uint2(info.shadowMapIndex, 0);
    const float4 lightSpacePos = mul(float4(worldPos, 1.0f), pushConstants.lightSpaceMatrix);
    const float shadow = CalculateShadow(lightSpacePos, L, N, shadowMap, gShadow);

    // const float3 color = ibl + emissive + brdf * shadow;
    const float3 color = ibl + emissive + brdf;

    const uint id = uint(sample0.w);
    const float3 dbg =
        float3(frac(float(id) * 0.61803398f), frac(float(id) * 0.41421356f), frac(float(id) * 0.31830988f));
    dst[px] = float4(dbg, 1.0f);
    // dst[px] = float4(color, 1.0f);
}
