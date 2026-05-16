import ShaderIO;
import Utils;

ConstantBuffer<shader_io::ShadowUniforms> pushConstants;

struct VSOut
{
    float4 sv_position : SV_Position;
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

    const shader_io::InstanceInfo instance = instances[instanceId];
    const shader_io::MeshInfo mesh = meshes[instance.meshHandle];

    ByteAddressBuffer gltfBuffer = (DescriptorHandle<ByteAddressBuffer>)uint2(mesh.gltfBufferIndex, 0);

    const float3 pos = GetVertexAttribute<float3>(gltfBuffer, mesh.triangleMesh.positions, vertexId);

    const float4 worldPos = mul(float4(pos, 1.0f), instance.transform);
    output.sv_position = mul(worldPos, pushConstants.lightSpaceMatrix);

    return output;
}
