//
// Created by y1 on 2026-04-26.
//

#pragma once

#include <string>

#include <directx/d3d12.h>
#include <wrl/client.h>

class DXApp;

class DXGraphicsPipeline {
public:
    DXGraphicsPipeline() = default;
    DXGraphicsPipeline(DXApp &dxApp, std::string_view inputFile);

    DXGraphicsPipeline(const DXGraphicsPipeline &)            = delete;
    DXGraphicsPipeline &operator=(const DXGraphicsPipeline &) = delete;

    DXGraphicsPipeline(DXGraphicsPipeline &&)            = default;
    DXGraphicsPipeline &operator=(DXGraphicsPipeline &&) = default;

    ~DXGraphicsPipeline() = default;

    [[nodiscard]] ID3D12RootSignature *GetRootSignature() const { return m_rootSignature.Get(); }

    [[nodiscard]] ID3D12PipelineState *GetPipelineState() const { return m_pipelineState.Get(); }

private:
    std::string m_name;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
};
