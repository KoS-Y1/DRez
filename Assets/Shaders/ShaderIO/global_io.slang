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
    SLANG_PUBLIC GlobalSceneInfo *sceneInfo;
};
}
