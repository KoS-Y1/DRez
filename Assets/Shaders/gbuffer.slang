import ShaderIO;
import Utils;

ConstantBuffer<shader_io::GbufferUniforms> pushConstants;

SamplerState gNearest : register(s0);
SamplerState gLinear : register(s1);
struct VSOut

{
    float4 sv_position : SV_Position;
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD0;
    float3 T : TANGENT;
    float3 B : BINORMAL;
    float3 N : NORMAL;

    float4 prevClipPos : TEXCOORD1;
    float4 curClipPos : TEXCOORD2;

    nointerpolation uint instanceId : TEXCOORD3;
}

struct FSOut
{
    float4 worldPosInstanceId : SV_Target0;
    float4 normalRoughness : SV_Target1;
    float4 albedoOcculusion : SV_Target2;
    float4 emissiveMetallic : SV_Target3;
    float2 motionVector : SV_Target4;
}

internal float3 Sample(const float2 uv, const Texture2D texture, const shader_io::SamplerType sampler)
{
    if (sampler == shader_io::SamplerType::Linear)
    {
        return texture.Sample(gLinear, uv).rgb;
    }

    return texture.Sample(gNearest, uv).rgb;
}

internal float2 CalculateMotionVector(float4 prevPos, float4 curPos)
{
    // Convert to NDC
    float2 prev = prevPos.xy / prevPos.w;
    float2 cur = curPos.xy / curPos.w;

    // Remap to [0, 1]
    prev = prev * 0.5f + 0.5f;
    cur = cur * 0.5f + 0.5f;

    // Flip Y
    return float2(prev.x - cur.x, cur.y - prev.y);
}

[shader("vertex")]
VSOut vertexMain(uint vertexId: SV_VertexID, uint instanceIndex: SV_InstanceID,
                 uint baseInstance: SV_StartInstanceLocation)
{
    VSOut output;

    const uint instanceId = instanceIndex + baseInstance;

    const StructuredBuffer<shader_io::InstanceInfo> instances =
        (DescriptorHandle<StructuredBuffer<shader_io::InstanceInfo>>)uint2(pushConstants.instancesIndex, 0);
    const StructuredBuffer<shader_io::MeshInfo> meshes =
        (DescriptorHandle<StructuredBuffer<shader_io::MeshInfo>>)uint2(pushConstants.meshesIndex, 0);
    const shader_io::GbufferInfo gbufferInfo =
        ((DescriptorHandle<StructuredBuffer<shader_io::GbufferInfo>>)uint2(pushConstants.gbufferInfoIndex, 0))[0];

    const shader_io::InstanceInfo instance = instances[instanceId];
    const shader_io::MeshInfo mesh = meshes[instance.meshHandle];

    const float4x4 viewProj = gbufferInfo.viewProj;
    const float4x4 prevViewProj = gbufferInfo.prevViewProj;
    const float4x4 viewProjJitter = gbufferInfo.viewProjJitter;

    ByteAddressBuffer gltfBuffer = (DescriptorHandle<ByteAddressBuffer>)uint2(mesh.gltfBufferIndex, 0);

    const float3 pos = GetVertexAttribute<float3>(gltfBuffer, mesh.triangleMesh.positions, vertexId);
    const float4 tangent = GetVertexAttribute<float4>(gltfBuffer, mesh.triangleMesh.tangents, vertexId);

    const float3 normal = GetVertexAttribute<float3>(gltfBuffer, mesh.triangleMesh.normals, vertexId);

    const float3x3 localToWorld = float3x3(instance.transform);
    const float3x3 normalMatrix = float3x3(instance.normalMatrix);

    float3 T, B;
    const float3 N = normalize(mul(normal, normalMatrix));

    if (length(tangent) > 0.0001f)
    {
        T = normalize(mul(tangent.xyz, normalMatrix));
        B = normalize(cross(N, T)) * tangent.w;
    }
    else
    {
        float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
        T = normalize(cross(up, N));
        B = cross(N, T);
    }

    output.uv = GetVertexAttribute<float2>(gltfBuffer, mesh.triangleMesh.texCoords, vertexId);
    output.worldPos = mul(float4(pos, 1.0f), instance.transform).xyz;
    output.sv_position = mul(float4(output.worldPos, 1.0f), viewProjJitter);
    output.T = T;
    output.B = B;
    output.N = N;
    output.prevClipPos = mul(float4(output.worldPos, 1.0f), prevViewProj);
    output.curClipPos = mul(float4(output.worldPos, 1.0f), viewProj);
    output.instanceId = instanceId;

    return output;
}

[shader("pixel")]
FSOut fragmentMain(VSOut input)
{
    FSOut output;

    const StructuredBuffer<shader_io::InstanceInfo> instances =
        (DescriptorHandle<StructuredBuffer<shader_io::InstanceInfo>>)uint2(pushConstants.instancesIndex, 0);
    const StructuredBuffer<shader_io::MaterialInfo> materials =
        (DescriptorHandle<StructuredBuffer<shader_io::MaterialInfo>>)uint2(pushConstants.materialsIndex, 0);

    const shader_io::InstanceInfo instance = instances[input.instanceId];
    const shader_io::MaterialInfo material = materials[instance.materialHandle];

    float3 albedo;
    float3 emissive;
    float3 normal;
    float roughness;
    float metallic;
    float ao = 0.0f;

    // Fecth texture if exists, otherwise, using the color
    if (material.albedoTextureIndex > -1)
    {
        const Texture2D texture = (DescriptorHandle<Texture2D<float4>>)uint2(material.albedoTextureIndex, 0);
        albedo = Sample(input.uv, texture, material.albedoSamplerType);
    }
    else
    {
        albedo = material.albedoFactor;
    }
    if (material.ormTextureIndex > -1)
    {
        const Texture2D texture = (DescriptorHandle<Texture2D<float4>>)uint2(material.ormTextureIndex, 0);
        const float3 orm = Sample(input.uv, texture, material.ormSamplerType);
        roughness = orm.y;
        metallic = orm.z;
        ao = orm.x;
    }
    else
    {
        roughness = material.roughnessFactor;
        metallic = material.metallicFactor;
    }
    if (material.emissiveTextureIndex > -1)
    {
        const Texture2D texture = (DescriptorHandle<Texture2D<float4>>)uint2(material.emissiveTextureIndex, 0);
        emissive = Sample(input.uv, texture, material.emissiveSamplerType);
    }
    else
    {
        emissive = material.emissiveFactor;
    }
    if (material.normalTextureIndex > -1)
    {
        const Texture2D texture = (DescriptorHandle<Texture2D<float4>>)uint2(material.normalTextureIndex, 0);
        normal = Sample(input.uv, texture, material.normalSamplerType);
        normal = normal * 2.0f - 1.0f;
    }
    else
    {
        normal = float3(0.0f, 0.0f, 1.0f);
    }

    const float3x3 TBN = float3x3(input.T, input.B, input.N);
    normal = normalize(mul(normal, TBN));

    output.worldPosInstanceId = float4(input.worldPos, float(input.instanceId));
    output.albedoOcculusion = float4(albedo, ao);
    output.normalRoughness = float4(normal, roughness);
    output.emissiveMetallic = float4(emissive, metallic);
    output.motionVector = CalculateMotionVector(input.prevClipPos, input.curClipPos);

    return output;
}
