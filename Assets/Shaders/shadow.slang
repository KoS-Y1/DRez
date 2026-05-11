import ShaderIO;
import Utils;

[[vk::push_constant]]
ConstantBuffer<shader_io::GlobalUniforms> pushConstant;

struct VSOut
{
    float4 sv_position : SV_Position;
}

[shader("vertex")]
VSOut vertMain(uint vertexId: SV_VertexID, uint instanceIndex: SV_InstanceID,
               uint baseInstance: SV_StartInstanceLocation)
{
    VSOut output;

    uint instanceId = instanceIndex + baseInstance;

    shader_io::GlobalSceneInfo *sceneInfo = pushConstant.sceneInfo;
    const shader_io::InstanceInfo instance = sceneInfo->instances[instanceId];
    const shader_io::MeshInfo mesh = sceneInfo->meshes[instance.meshHandle];

    const float3 pos = GetVertexAttribute<float3>(mesh.buffer, mesh.triangleMesh.positions, vertexId);

    output.sv_position = mul(mul(float4(pos, 1.0f), pushConstant.transform), sceneInfo->lightSpace);

    return output;
}
