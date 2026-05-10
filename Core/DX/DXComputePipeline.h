//
// Created by y1 on 2026-05-10.
//

#pragma once
#include <string>

#include <directx/d3d12.h>
#include <wrl/client.h>

class DXApp;

class DXComputePipeline {
public:
    DXComputePipeline() = default;
    DXComputePipeline(DXApp &app, std::string_view filePath);

    DXComputePipeline(const DXComputePipeline &)            = delete;
    DXComputePipeline &operator=(const DXComputePipeline &) = delete;

    DXComputePipeline(DXComputePipeline &&)            = default;
    DXComputePipeline &operator=(DXComputePipeline &&) = default;

    ~DXComputePipeline() = default;

    [[nodiscard]] std::string_view GetName() const { return m_name; }

    [[nodiscard]] ID3D12RootSignature *GetRootSignature() const { return m_rootSignature.Get(); }

    [[nodiscard]] ID3D12PipelineState *GetPipelineState() const { return m_pipelineState.Get(); }

private:
    std::string m_name;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
};
