implementing Utils;

public static const float kDirectionlLightRadius = 0.10f;

internal float PCF(const float2 uv, const float cmp, const Texture2D<float> shadowMap,
                   const SamplerComparisonState shadowSampler)
{
    float shadow = 0.0f;

    uint width = 0;
    uint height = 0;
    shadowMap.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            const float2 offset = float2(x, y) * texelSize;
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uv + offset, cmp);
        }
    }

    return shadow / 25.0f;
}

public float CalculateShadow(const float4 lightSpacePos, const float3 L, const float3 worldNormal,
                             const Texture2D<float> shadowMap, const SamplerComparisonState shadowSampler)
{
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    if (projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    float2 uv = projCoords.xy;
    uv = uv * 0.5f + 0.5f;
    // D3D UV: y=0 is top of texture; flip from NDC y-up
    uv.y = 1.0f - uv.y;

    // Slope scaled bias
    const float NdotL = saturate(dot(normalize(worldNormal), normalize(L)));
    const float bias = max(0.0001f * (1 - NdotL), 0.0007f);

    return PCF(uv, projCoords.z - bias, shadowMap, shadowSampler);
}