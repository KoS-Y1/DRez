implementing Utils;


internal float3 FresnelSchlick(const float cosTheta, const float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

internal float3 FresnelSchlickRoughness(const float cosTheta, const float3 F0, const float roughness)
{
    return F0 + (max(float3(1.0f - roughness), F0) - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

internal float DistributionGGX(const float3 N, const float3 H, const float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    return a2 / (PI * denom * denom);
}

internal float GeometrySchlickGGX(const float NdotV, const float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float denom = NdotV * (1.0f - k) + k;
    return NdotV / denom;
}

internal float GeometrySmith(const float3 N, const float3 V, const float3 L, const float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

public float3 CalculateBRDF(const float3 N, const float3 V, const float3 L, const float3 albedo, const float roughness,
                           const float metallic, const float3 lightColor, const float lightIntensity)
{
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotV = max(dot(N, V), 0.0f);

    // Fresnel reflectance at normal incidence
    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    float G = GeometrySmith(N, V, L, roughness);
    float NDF = DistributionGGX(N, H, roughness);

    float3 kS = F;
    float3 kD = (float3(1.0f) - kS) * (1.0f - metallic);

    float3 numerator = NDF * G * F;
    float denominator = 4.0f * NdotV * NdotL + 0.001f; // Prevent division by zero
    float3 specular = numerator / denominator;

    float3 irradiance = lightColor * lightIntensity;

    return (kD * albedo / PI + specular) * irradiance * NdotL;
}

// From https://learnopengl.com/PBR/IBL/Specular-IBL
public float2 SampleSphericalMap(float3 input)
{
    const float2 invAtan = float2(0.1591, 0.3183);

    float2 uv = float2(atan2(input.z, input.x), asin(input.y));
    uv *= invAtan;

    return uv + 0.5;
}

internal float3 SampleIBLReflection(const float3 R, const float roughness,
                                    const Texture2D<float4> specularTex, const SamplerState samp)
{
    // based on https://github.com/oframe/ibl-converter/blob/master/src/shaders/PBRShader.js
    const float MAX_REFLECTION_LOD = 6.0;
    float blend = roughness * MAX_REFLECTION_LOD;
    const float level0 = floor(blend);
    const float level1 = min(MAX_REFLECTION_LOD, level0 + 1.0);
    blend -= level0;

    float2 uvSpec = SampleSphericalMap(R);
    uvSpec.y /= 2.0f;

    float2 uv0 = uvSpec;
    float2 uv1 = uvSpec;

    uv0 /= pow(2.0, level0);
    uv0.y += 1.0 - exp(-LN2 * level0);

    uv1 /= pow(2.0, level1);
    uv1.y += 1.0 - exp(-LN2 * level1);

    float3 specular0 = RgbmToLinear(specularTex.SampleLevel(samp, uv0, 0)).rgb;
    float3 specular1 = RgbmToLinear(specularTex.SampleLevel(samp, uv1, 0)).rgb;
    float3 reflection = lerp(specular0, specular1, blend);

    return reflection;
}

public float3 CalculateIBL(const float3 N, const float NdotV, const float3 R, const float3 F0, const float3 albedo,
                  const float roughness, const float metallic, const float ao,
                  const Texture2D<float4> irradianceTex, const Texture2D<float4> specularTex,
                  const Texture2D<float4> brdfLutTex, const SamplerState samp)
{
    const float3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    const float3 kD = (float3(1.0f) - kS) * (1.0f - metallic);

    const float3 irradiance = irradianceTex.SampleLevel(samp, SampleSphericalMap(N), 0).xyz;
    const float3 reflection = SampleIBLReflection(R, roughness, specularTex, samp);
    const float2 brdf = brdfLutTex.SampleLevel(samp, float2(NdotV, roughness), 0).rg;
    const float3 specular = reflection * (kS * brdf.x + brdf.y);

    return (kD * irradiance * albedo + specular) * ao;
}
