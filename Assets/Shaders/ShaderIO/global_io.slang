#if defined(__cplusplus)

#pragma once

#include "resources_io.slang"

#elif defined(__SLANG__)

implementing ShaderIO;

#endif

namespace shader_io
{
SLANG_PUBLIC struct GlobalUniforms
{
    SLANG_PUBLIC float4x4 viewProj;
    SLANG_PUBLIC float4x4 prevViewProj;
    
    // Buffer index in the heap
    SLANG_PUBLIC uint32_t instancesIndex;
    SLANG_PUBLIC uint32_t meshesIndex;
    SLANG_PUBLIC uint32_t materialsIndex;
    uint32_t padding;
};

// Blit pass
SLANG_PUBLIC static const uint32_t kBlitThreadX = 8;
SLANG_PUBLIC static const uint32_t kBlitThreadY = 8;

SLANG_PUBLIC struct BlitUniforms
{
    SLANG_PUBLIC uint32_t srcIndex;
    SLANG_PUBLIC uint32_t dstIndex;
    SLANG_PUBLIC SamplerType samplerType;
    uint32_t padding;
};

// Deferred pass
SLANG_PUBLIC static const uint32_t kDeferredThreadX = 8;
SLANG_PUBLIC static const uint32_t kDeferredThreadY = 8;

SLANG_PUBLIC struct DeferredUniforms
{
    SLANG_PUBLIC float4x4 lightSpaceMatrix;
    SLANG_PUBLIC float3 cameraPos;
    SLANG_PUBLIC uint32_t deferredInfoIndex;
    SLANG_PUBLIC float3 lightDir;
    SLANG_PUBLIC float lightIntensity;
    SLANG_PUBLIC float3 lightColor;
    SLANG_PUBLIC uint32_t dstIndex;
};

// Skybox pass
SLANG_PUBLIC struct SkyboxUniforms
{
    SLANG_PUBLIC float4x4 viewProj;
    SLANG_PUBLIC uint32_t skyboxIndex;
    uint32_t padding[3];
};

// Mipmap
SLANG_PUBLIC static const uint32_t kMipmapThreadX = 8;
SLANG_PUBLIC static const uint32_t kMipmapThreadY = 8;
SLANG_PUBLIC struct MipmapUniforms
{
    SLANG_PUBLIC uint32_t srcIndex;
    SLANG_PUBLIC uint32_t dstIndex;
    SLANG_PUBLIC uint32_t srcMipLevel;
    uint32_t padding;
};
}
