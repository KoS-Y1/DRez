#if defined(__cplusplus)
#pragma once

#include "shader_io.h"

#elif defined(__SLANG__)

implementing ShaderIO;

#endif

namespace shader_io
{
SLANG_PUBLIC enum class SamplerType : uint32_t
{
    Nearest = 0,
    Linear = 1,
};

SLANG_PUBLIC struct BufferView
{
    SLANG_PUBLIC uint32_t offset;
    SLANG_PUBLIC uint32_t count;
    SLANG_PUBLIC uint32_t byteStride; // 0 if tightly packed
};

SLANG_PUBLIC struct TriangleMesh
{
    SLANG_PUBLIC BufferView indices;
    SLANG_PUBLIC BufferView positions;
    SLANG_PUBLIC BufferView normals;
    SLANG_PUBLIC BufferView tangents;
    SLANG_PUBLIC BufferView texCoords;
};

SLANG_PUBLIC struct MaterialInfo
{
    SLANG_PUBLIC int albedoTextureIndex = -1;
    SLANG_PUBLIC int ormTextureIndex = -1;
    SLANG_PUBLIC int emissiveTextureIndex = -1;
    SLANG_PUBLIC int normalTextureIndex = -1;

    SLANG_PUBLIC SamplerType albedoSamplerType;
    SLANG_PUBLIC SamplerType ormSamplerType;
    SLANG_PUBLIC SamplerType emissiveSamplerType;
    SLANG_PUBLIC SamplerType normalSamplerType;

    SLANG_PUBLIC float3 albedoFactor;
    SLANG_PUBLIC float metallicFactor;
    SLANG_PUBLIC float3 emissiveFactor;
    SLANG_PUBLIC float roughnessFactor;
};

SLANG_PUBLIC struct MeshInfo
{
    SLANG_PUBLIC uint32_t gltfBufferIndex;
    SLANG_PUBLIC TriangleMesh triangleMesh;
    SLANG_PUBLIC int indexType;
};

SLANG_PUBLIC struct InstanceInfo
{
    SLANG_PUBLIC float4x4 transform;
    SLANG_PUBLIC float4x4 normalMatrix;
    SLANG_PUBLIC uint32_t meshHandle;
    SLANG_PUBLIC uint32_t materialHandle;
    uint32_t padding[2];
};

SLANG_PUBLIC struct GbufferInfo
{
    SLANG_PUBLIC float4x4 viewProj;
    SLANG_PUBLIC float4x4 prevViewProj;
    SLANG_PUBLIC float4x4 viewProjJitter;
};

SLANG_PUBLIC struct DeferredInfo
{
    SLANG_PUBLIC uint32_t gbuffer0Index; // worldPos + instanceId
    SLANG_PUBLIC uint32_t gbuffer1Index; // normal + roughness
    SLANG_PUBLIC uint32_t gbuffer2Index; // albedo + ao
    SLANG_PUBLIC uint32_t gbuffer3Index; // emissive + metallic

    SLANG_PUBLIC uint32_t irradianceIndex;
    SLANG_PUBLIC uint32_t specularIndex;
    SLANG_PUBLIC uint32_t brdfLutIndex;

    SLANG_PUBLIC uint32_t shadowMapIndex;
};

};
