//
// Created by y1 on 2026-05-10.
//

#include "DXComputePipeline.h"

#include "DXApp.h"
#include "DXPipelineConfig.h"
#include "Debug.h"
#include "ShaderCompiler.h"

using Microsoft::WRL::ComPtr;

DXComputePipeline::DXComputePipeline(DXApp &app, std::string_view filePath) {
    const ComputePipelineConfig config{filePath};
    m_name = config.name;

    // Create root signature
    {
        std::vector<CD3DX12_ROOT_PARAMETER1> rootParams = ShaderCompiler::GetInstance().GetRootParameters(config.shader);

        const std::vector<CD3DX12_STATIC_SAMPLER_DESC> &staticSamplers = drez::dx::pipeline::GetStaticSamplers();

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Init_1_1(
            rootParams.size(),
            rootParams.data(),
            staticSamplers.size(),
            staticSamplers.data(),
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
        );

        ComPtr<ID3DBlob> signature{};
        ComPtr<ID3DBlob> error{};
        HRESULT          result = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
        DebugCheckCritical(
            SUCCEEDED(result),
            "Compute pipeline {}: failed to serialize signature, error 0x{:x}",
            m_name,
            static_cast<uint32_t>(result)
        );

        result = app.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        DebugCheckCritical(
            SUCCEEDED(result),
            "Compute pipeline {}: failed to create rootSignature, error 0x{:x}",
            m_name,
            static_cast<uint32_t>(result)
        );
    }

    // Create pipeline
    {
        const auto &entryPoints = ShaderCompiler::GetInstance().GetEntryPoints(config.shader);

        auto GetShaderByteCode = [&entryPoints](SlangStage stage) -> D3D12_SHADER_BYTECODE {
            auto pair = entryPoints.find(stage);
            if (pair == entryPoints.end()) {
                return D3D12_SHADER_BYTECODE{nullptr, 0};
            }
            return {pair->second->getBufferPointer(), pair->second->getBufferSize()};
        };


        const D3D12_SHADER_BYTECODE computeBytecode = GetShaderByteCode(SLANG_STAGE_COMPUTE);
        const D3D12_COMPUTE_PIPELINE_STATE_DESC
            desc{.pRootSignature = m_rootSignature.Get(), .CS = computeBytecode, .NodeMask = 0, .Flags = config.flags};

        HRESULT result = app.GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_pipelineState));
        DebugCheckCritical(
            SUCCEEDED(result),
            "Compute pipeline {}: failed to create graphics pipeline state, error 0x{:x}",
            m_name,
            static_cast<uint32_t>(result)
        );
    }
}