//
// Created by y1 on 2026-04-27.
//

#pragma once

#include <string>
#include <vector>

#include <directx/d3d12.h>

struct GraphicsPipelineConfig {
    std::string shader;
    std::string name;

    D3D12_RASTERIZER_DESC         rasterizer{};
    D3D12_BLEND_DESC              blendState{};
    D3D12_DEPTH_STENCIL_DESC      depthStencil{};
    D3D12_INPUT_LAYOUT_DESC       inputLayout{};
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topology{};

    std::vector<DXGI_FORMAT> rtvFormats{};
    DXGI_FORMAT              dsvFormat{};
    DXGI_SAMPLE_DESC         sample{};

    D3D_PRIMITIVE_TOPOLOGY primitiveTopology{};

    explicit GraphicsPipelineConfig(std::string_view inputFile);
};