implementing Utils;

public static const float kDirectionlLightRadius = 0.10f;

internal float PCF(const float2 uv, const float cmp, const Sampler2DShadow shadowMap)
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
            shadow += shadowMap.SampleCmpLevel(uv + offset, cmp, 0);
        }
    }

    return shadow / 25.0f;
}

public float CalculateShadow(const float4 lightSpacePos, const float3 L, const float3 worldNormal,
                             const Sampler2DShadow shadowMap)
{
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    if (projCoords.z > 1.0f)
    {
        return 0.0f;
    }

    float2 uv = projCoords.xy;
    uv = uv * 0.5f + 0.5f;

    // Slope scaled bias
    const float NdotL = saturate(dot(normalize(worldNormal), normalize(L)));
    const float bias = max(0.0003f * (1 - NdotL), 0.0007f);

    return PCF(uv, projCoords.z - bias, shadowMap);
}

public float3 GetShadowRayDir(const float3 worldPos, const float3 lightDir, const float2 randomSample, const float lightRadius) {
    const float pointRadius = lightRadius * sqrt(randomSample.x);
    const float pointAngle = randomSample.y * 2.0f * PI;

    const float2 diskPoint = float2(pointRadius * cos(pointAngle), pointRadius * sin(pointAngle));
    const float3 lightTangent = normalize(cross(lightDir, kWorldUp));
    const float3 lightBitangent = normalize(cross(lightTangent, lightDir));

    return normalize(lightDir + diskPoint.x * lightTangent + diskPoint.y * lightBitangent);
}