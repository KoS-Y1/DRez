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
    SLANG_PUBLIC float4x4 transform;
    SLANG_PUBLIC float4x4 normalMatrix;
    SLANG_PUBLIC float4x4 viewProj;

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
    SLANG_PUBLIC DeferredInfo deferredInfo;
    SLANG_PUBLIC uint32_t dstIndex;
}
}
