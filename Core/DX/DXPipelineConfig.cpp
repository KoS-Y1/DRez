//
// Created by y1 on 2026-04-27.
//

#include "DXPipelineConfig.h"

#include <unordered_map>

#include <directx/d3dx12.h>

#include "Debug.h"
#include "JsonFile.h"
#include "VertexFormats.h"

namespace {
const std::unordered_map<std::string, D3D12_FILL_MODE> kFillModeMap{
    {"D3D12_FILL_MODE_WIREFRAME", D3D12_FILL_MODE_WIREFRAME},
    {"D3D12_FILL_MODE_SOLID",     D3D12_FILL_MODE_SOLID    },
};

const std::unordered_map<std::string, D3D12_CULL_MODE> kCullModeMap{
    {"D3D12_CULL_MODE_NONE",  D3D12_CULL_MODE_NONE },
    {"D3D12_CULL_MODE_FRONT", D3D12_CULL_MODE_FRONT},
    {"D3D12_CULL_MODE_BACK",  D3D12_CULL_MODE_BACK },
};

const std::unordered_map<std::string, D3D12_CONSERVATIVE_RASTERIZATION_MODE> kConservativeRasterMap{
    {"D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF", D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF},
    {"D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON",  D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON },
};

const std::unordered_map<std::string, D3D12_DEPTH_WRITE_MASK> kDepthWriteMaskMap{
    {"D3D12_DEPTH_WRITE_MASK_ZERO", D3D12_DEPTH_WRITE_MASK_ZERO},
    {"D3D12_DEPTH_WRITE_MASK_ALL",  D3D12_DEPTH_WRITE_MASK_ALL },
};

const std::unordered_map<std::string, D3D12_COMPARISON_FUNC> kComparisonFuncMap{
    {"D3D12_COMPARISON_FUNC_NEVER",         D3D12_COMPARISON_FUNC_NEVER        },
    {"D3D12_COMPARISON_FUNC_LESS",          D3D12_COMPARISON_FUNC_LESS         },
    {"D3D12_COMPARISON_FUNC_EQUAL",         D3D12_COMPARISON_FUNC_EQUAL        },
    {"D3D12_COMPARISON_FUNC_LESS_EQUAL",    D3D12_COMPARISON_FUNC_LESS_EQUAL   },
    {"D3D12_COMPARISON_FUNC_GREATER",       D3D12_COMPARISON_FUNC_GREATER      },
    {"D3D12_COMPARISON_FUNC_NOT_EQUAL",     D3D12_COMPARISON_FUNC_NOT_EQUAL    },
    {"D3D12_COMPARISON_FUNC_GREATER_EQUAL", D3D12_COMPARISON_FUNC_GREATER_EQUAL},
    {"D3D12_COMPARISON_FUNC_ALWAYS",        D3D12_COMPARISON_FUNC_ALWAYS       },
};

const std::unordered_map<std::string, D3D12_STENCIL_OP> kStencilOpMap{
    {"D3D12_STENCIL_OP_KEEP",     D3D12_STENCIL_OP_KEEP    },
    {"D3D12_STENCIL_OP_ZERO",     D3D12_STENCIL_OP_ZERO    },
    {"D3D12_STENCIL_OP_REPLACE",  D3D12_STENCIL_OP_REPLACE },
    {"D3D12_STENCIL_OP_INCR_SAT", D3D12_STENCIL_OP_INCR_SAT},
    {"D3D12_STENCIL_OP_DECR_SAT", D3D12_STENCIL_OP_DECR_SAT},
    {"D3D12_STENCIL_OP_INVERT",   D3D12_STENCIL_OP_INVERT  },
    {"D3D12_STENCIL_OP_INCR",     D3D12_STENCIL_OP_INCR    },
    {"D3D12_STENCIL_OP_DECR",     D3D12_STENCIL_OP_DECR    },
};

const std::unordered_map<std::string, D3D12_PRIMITIVE_TOPOLOGY_TYPE> kPrimitiveTopologyTypeMap{
    {"D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT",    D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT   },
    {"D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE",     D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE    },
    {"D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE},
    {"D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH",    D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH   },
};

const std::unordered_map<std::string, DXGI_FORMAT> kFormatMap{
    {"DXGI_FORMAT_UNKNOWN",              DXGI_FORMAT_UNKNOWN             },
    {"DXGI_FORMAT_R8G8B8A8_UNORM",       DXGI_FORMAT_R8G8B8A8_UNORM      },
    {"DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
    {"DXGI_FORMAT_B8G8R8A8_UNORM",       DXGI_FORMAT_B8G8R8A8_UNORM      },
    {"DXGI_FORMAT_B8G8R8A8_UNORM_SRGB",  DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
    {"DXGI_FORMAT_R16_FLOAT",            DXGI_FORMAT_R16_FLOAT           },
    {"DXGI_FORMAT_R16G16_FLOAT",         DXGI_FORMAT_R16G16_FLOAT        },
    {"DXGI_FORMAT_R16G16B16A16_FLOAT",   DXGI_FORMAT_R16G16B16A16_FLOAT  },
    {"DXGI_FORMAT_R32_FLOAT",            DXGI_FORMAT_R32_FLOAT           },
    {"DXGI_FORMAT_R32G32_FLOAT",         DXGI_FORMAT_R32G32_FLOAT        },
    {"DXGI_FORMAT_R32G32B32_FLOAT",      DXGI_FORMAT_R32G32B32_FLOAT     },
    {"DXGI_FORMAT_R32G32B32A32_FLOAT",   DXGI_FORMAT_R32G32B32A32_FLOAT  },
    {"DXGI_FORMAT_D16_UNORM",            DXGI_FORMAT_D16_UNORM           },
    {"DXGI_FORMAT_D24_UNORM_S8_UINT",    DXGI_FORMAT_D24_UNORM_S8_UINT   },
    {"DXGI_FORMAT_D32_FLOAT",            DXGI_FORMAT_D32_FLOAT           },
    {"DXGI_FORMAT_D32_FLOAT_S8X24_UINT", DXGI_FORMAT_D32_FLOAT_S8X24_UINT},
};

const std::unordered_map<std::string, D3D12_INPUT_LAYOUT_DESC> kInputLayoutMap{
    {"VertexEmpty", VertexEmpty::GetInputLayout()},
    {"VertexPT2D",  VertexPT2D::GetInputLayout() },
};

const std::unordered_map<std::string, D3D_PRIMITIVE_TOPOLOGY> kPrimitiveTopologyMap{
    {"D3D_PRIMITIVE_TOPOLOGY_POINTLIST",         D3D_PRIMITIVE_TOPOLOGY_POINTLIST        },
    {"D3D_PRIMITIVE_TOPOLOGY_LINELIST",          D3D_PRIMITIVE_TOPOLOGY_LINELIST         },
    {"D3D_PRIMITIVE_TOPOLOGY_LINESTRIP",         D3D_PRIMITIVE_TOPOLOGY_LINESTRIP        },
    {"D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST",      D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST     },
    {"D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP",     D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP    },
    {"D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN",       D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN      },
    {"D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ",      D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ     },
    {"D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ",     D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ    },
    {"D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ",  D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ },
    {"D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ", D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ}
};

const std::unordered_map<std::string, D3D12_PIPELINE_STATE_FLAGS> kPipelineStateFlagsMap{
    {"D3D12_PIPELINE_STATE_FLAG_NONE",                           D3D12_PIPELINE_STATE_FLAG_NONE                          },
    {"D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG",                     D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG                    },
    {"D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS",             D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS            },
    {"D3D12_PIPELINE_STATE_FLAG_DYNAMIC_INDEX_BUFFER_STRIP_CUT", D3D12_PIPELINE_STATE_FLAG_DYNAMIC_INDEX_BUFFER_STRIP_CUT},
    {"D3D12_PIPELINE_STATE_FLAG_DISABLE_CACHED_BLOB",            D3D12_PIPELINE_STATE_FLAG_DISABLE_CACHED_BLOB           }
};

template<typename T>
T LookupMap(const std::unordered_map<std::string, T> &map, std::string_view key, std::string_view typeName) {
    auto it = map.find(std::string(key));
    DebugCheckCritical(it != map.end(), "Invalid {} {}", typeName, key);
    return it->second;
}

D3D12_RASTERIZER_DESC GetRasterizer(const JsonFile &file) {
    D3D12_RASTERIZER_DESC desc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    desc.FillMode              = LookupMap(kFillModeMap, file.Get<std::string>("fillMode", "D3D12_FILL_MODE_SOLID"), "fill mode");
    desc.CullMode              = LookupMap(kCullModeMap, file.Get<std::string>("cullMode", "D3D12_CULL_MODE_NONE"), "cull mode");
    desc.FrontCounterClockwise = file.Get<bool>("frontCounterClockwise", false) ? TRUE : FALSE;
    desc.DepthBias             = file.Get<int>("depthBias", D3D12_DEFAULT_DEPTH_BIAS);
    desc.DepthBiasClamp        = file.Get<float>("depthBiasClamp", D3D12_DEFAULT_DEPTH_BIAS_CLAMP);
    desc.SlopeScaledDepthBias  = file.Get<float>("slopeScaledDepthBias", D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS);
    desc.DepthClipEnable       = file.Get<bool>("depthClipEnable", true) ? TRUE : FALSE;
    desc.MultisampleEnable     = file.Get<bool>("multisampleEnable", false) ? TRUE : FALSE;
    desc.AntialiasedLineEnable = file.Get<bool>("antialiasedLineEnable", false) ? TRUE : FALSE;
    desc.ForcedSampleCount     = static_cast<UINT>(file.Get<int>("forcedSampleCount", 0));
    desc.ConservativeRaster    = LookupMap(
        kConservativeRasterMap,
        file.Get<std::string>("conservativeRaster", "D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF"),
        "conservative raster"
    );

    return desc;
}

D3D12_BLEND_DESC GetBlend(const JsonFile &file) {
    D3D12_BLEND_DESC desc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    desc.AlphaToCoverageEnable  = file.Get<bool>("alphaToCoverageEnable", false) ? TRUE : FALSE;
    desc.IndependentBlendEnable = file.Get<bool>("independentBlendEnable", false) ? TRUE : FALSE;

    // TODO: parse per-render-target blend state; for now keep D3D12_DEFAULT entries
    return desc;
}

D3D12_DEPTH_STENCILOP_DESC GetStencilOp(const JsonFile &file, const D3D12_DEPTH_STENCILOP_DESC &defaults) {
    D3D12_DEPTH_STENCILOP_DESC desc = defaults;

    desc.StencilFailOp      = LookupMap(kStencilOpMap, file.Get<std::string>("stencilFailOp", "D3D12_STENCIL_OP_KEEP"), "stencil fail op");
    desc.StencilDepthFailOp = LookupMap(kStencilOpMap, file.Get<std::string>("stencilDepthFailOp", "D3D12_STENCIL_OP_KEEP"), "stencil depth fail op");
    desc.StencilPassOp      = LookupMap(kStencilOpMap, file.Get<std::string>("stencilPassOp", "D3D12_STENCIL_OP_KEEP"), "stencil pass op");
    desc.StencilFunc        = LookupMap(kComparisonFuncMap, file.Get<std::string>("stencilFunc", "D3D12_COMPARISON_FUNC_ALWAYS"), "stencil func");

    return desc;
}

D3D12_DEPTH_STENCIL_DESC GetDepthStencil(const JsonFile &file) {
    D3D12_DEPTH_STENCIL_DESC desc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    desc.DepthEnable      = file.Get<bool>("depthEnable", false) ? TRUE : FALSE;
    desc.DepthWriteMask   = LookupMap(kDepthWriteMaskMap, file.Get<std::string>("depthWriteMask", "D3D12_DEPTH_WRITE_MASK_ZERO"), "depth write mask");
    desc.DepthFunc        = LookupMap(kComparisonFuncMap, file.Get<std::string>("depthFunc", "D3D12_COMPARISON_FUNC_ALWAYS"), "depth func");
    desc.StencilEnable    = file.Get<bool>("stencilEnable", false) ? TRUE : FALSE;
    desc.StencilReadMask  = static_cast<UINT8>(file.Get<int>("stencilReadMask", D3D12_DEFAULT_STENCIL_READ_MASK));
    desc.StencilWriteMask = static_cast<UINT8>(file.Get<int>("stencilWriteMask", D3D12_DEFAULT_STENCIL_WRITE_MASK));

    const JsonFile frontFile = file.Field("frontFace");
    desc.FrontFace           = GetStencilOp(frontFile, desc.FrontFace);

    const JsonFile backFile = file.Field("backFace");
    desc.BackFace           = GetStencilOp(backFile, desc.BackFace);

    return desc;
}

DXGI_SAMPLE_DESC GetSample(const JsonFile &file) {
    DXGI_SAMPLE_DESC desc{1, 0};

    desc.Count   = static_cast<UINT>(file.Get<int>("count", 1));
    desc.Quality = static_cast<UINT>(file.Get<int>("quality", 0));

    return desc;
}


} // namespace

GraphicsPipelineConfig::GraphicsPipelineConfig(std::string_view inputFile) {
    JsonFile json(inputFile);

    shader = json.Get<std::string>("shader");
    name   = json.Get<std::string>("name");

    const JsonFile rasterizerField = json.Field("rasterizer");
    rasterizer                     = GetRasterizer(rasterizerField);

    const JsonFile blendField = json.Field("blendState");
    blendState                = GetBlend(blendField);

    const JsonFile depthStencilFile = json.Field("depthStencil");
    depthStencil                    = GetDepthStencil(depthStencilFile);

    inputLayout = LookupMap(kInputLayoutMap, json.Get<std::string>("inputLayout"), "inputLayout");

    topology = LookupMap(kPrimitiveTopologyTypeMap, json.Get<std::string>("topology", "D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE"), "topology");

    for (const auto &fmt: json.Get<std::vector<std::string>>("rtvFormats", {})) {
        rtvFormats.emplace_back(LookupMap(kFormatMap, fmt, "rtv format"));
    }
    dsvFormat = LookupMap(kFormatMap, json.Get<std::string>("dsvFormat", "DXGI_FORMAT_UNKNOWN"), "dsv format");

    primitiveTopology =
        LookupMap(kPrimitiveTopologyMap, json.Get<std::string>("primitiveTopology", "D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST"), "primitive topology");

    const JsonFile sampleFile = json.Field("sample");
    sample                    = GetSample(sampleFile);
}

ComputePipelineConfig::ComputePipelineConfig(std::string_view inputFile) {
    JsonFile json(inputFile);

    shader = json.Get<std::string>("shader");
    name   = json.Get<std::string>("name");

    flags = LookupMap(kPipelineStateFlagsMap, json.Get<std::string>("flags", "D3D12_PIPELINE_STATE_FLAG_NONE"), "flags");
}