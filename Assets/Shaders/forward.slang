import ShaderIO;
import Utils;

ConstantBuffer<shader_io::GlobalUniforms> pushConstants;

struct VSOut
{
    float4 sv_position : SV_Position;
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD0;
    float3 T : TANGENT;
    float3 B : BINORMAL;
    float3 N : NORMAL;
    nointerpolation uint instanceId : TEXCOORD1;
}

[shader("vertex")]
VSOut vertexMain(uint vertexId: SV_VertexID, uint instanceIndex: SV_InstanceID,
                 uint baseInstance: SV_StartInstanceLocation)
{
    VSOut output;

    uint instanceId = instanceIndex + baseInstance;

    StructuredBuffer<shader_io::InstanceInfo> instances =
        (DescriptorHandle<StructuredBuffer<shader_io::InstanceInfo>>)uint2(pushConstants.instancesIndex, 0);
    StructuredBuffer<shader_io::MeshInfo> meshes =
        (DescriptorHandle<StructuredBuffer<shader_io::MeshInfo>>)uint2(pushConstants.meshesIndex, 0);

    const shader_io::InstanceInfo instance = instances[instanceId];
    const shader_io::MeshInfo mesh = meshes[instance.meshHandle];

    ByteAddressBuffer gltfBuffer = (DescriptorHandle<ByteAddressBuffer>)uint2(mesh.gltfBufferIndex, 0);

    const float3 pos = GetVertexAttribute<float3>(gltfBuffer, mesh.triangleMesh.positions, vertexId);
    const float4 tangent = GetVertexAttribute<float4>(gltfBuffer, mesh.triangleMesh.tangents, vertexId);

    const float3 normal = GetVertexAttribute<float3>(gltfBuffer, mesh.triangleMesh.normals, vertexId);

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

    output.uv = GetVertexAttribute<float2>(gltfBuffer, mesh.triangleMesh.texCoords, vertexId);
    output.worldPos = mul(float4(pos, 1.0f), pushConstants.transform).xyz;
    output.sv_position = mul(float4(output.worldPos, 1.0f), pushConstants.viewProj);
    output.T = T;
    output.B = B;
    output.N = N;
    output.instanceId = instanceId;

    return output;
}

[shader("pixel")]
float4 pixelMain(VSOut input) : SV_Target
{
    return float4(input.uv, 0.0f, 1.0f);
}
