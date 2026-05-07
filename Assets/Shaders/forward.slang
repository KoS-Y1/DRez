import ShaderIO;
import Utils;

ConstantBuffer<shader_io::GlobalUniforms> pushConstants;

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 worldPos;
    float2 uv;
    float3x3 TBN;
    nointerpolation uint instanceId;
}

[shader("vertex")]
VSOut vertexMain(uint vertexId: SV_VertexID, uint instanceIndex: SV_InstanceID,
                 uint baseInstance: SV_StartInstanceLocation)
{
    VSOut output;

    uint instanceId = instanceIndex + baseInstance;

    shader_io::GlobalSceneInfo *sceneInfo = pushConstants.sceneInfo;
    const shader_io::InstanceInfo instance = sceneInfo->instances[instanceId];
    const shader_io::MeshInfo mesh = sceneInfo->meshes[instance.meshHandle];

    const float3 pos = GetVertexAttribute<float3>(mesh.buffer, mesh.triangleMesh.positions, vertexId);
    const float4 tangent = GetVertexAttribute<float4>(mesh.buffer, mesh.triangleMesh.tangents, vertexId);

    const float3 normal = GetVertexAttribute<float3>(mesh.buffer, mesh.triangleMesh.normals, vertexId);

    const float3x3 localToWorld = float3x3(pushConstants.transform);
    const float3x3 normalMatrix = float3x3(pushConstants.normalMatrix);

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

    output.uv = GetVertexAttribute<float2>(mesh.buffer, mesh.triangleMesh.texCoords, vertexId);
    output.worldPos = mul(float4(pos, 1.0f), pushConstants.transform).xyz;
    output.sv_position = mul(float4(output.worldPos, 1.0f), sceneInfo->viewProj);
    output.TBN = float3x3(T, B, N);
    output.instanceId = instanceId;

    return output;
}

[shader("pixel")]
float4 pixelMain(VSOut input) : SV_Target
{
    return float4(input.uv, 0.0f, 1.0f);
}
