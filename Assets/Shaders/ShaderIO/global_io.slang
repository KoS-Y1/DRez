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
    SLANG_PUBLIC uint32_t instancesIndex;
    SLANG_PUBLIC uint32_t meshesIndex;
    SLANG_PUBLIC uint32_t materialsIndex;
    uint32_t padding;
    // SLANG_PUBLIC float4 color;
};
}
