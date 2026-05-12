//
// Created by y1 on 2026-04-27.
//

#pragma once

#include <string>
#include <vector>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <directxmath.h>

namespace drez::dx::pipeline {
// Shared static samplers bound at fixed registers for every pipeline:
//   s0: nearest        s1: linear        s2: shadow comparison
const std::vector<CD3DX12_STATIC_SAMPLER_DESC> &GetStaticSamplers();
} // namespace drez::dx::pipeline

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

struct ComputePipelineConfig {
    std::string shader;
    std::string name;

    D3D12_PIPELINE_STATE_FLAGS flags{};

    explicit ComputePipelineConfig(std::string_view inputFile);
};