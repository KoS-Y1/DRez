//
// Created by y1 on 2026-04-26.
//

#include "DXGraphicsPipeline.h"

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include "DXApp.h"
#include "DXPipelineConfig.h"
#include "Debug.h"
#include "ShaderCompiler.h"

using Microsoft::WRL::ComPtr;

DXGraphicsPipeline::DXGraphicsPipeline(DXApp &app, std::string_view filePath) {
    const GraphicsPipelineConfig config{filePath};
    m_name = config.name;
    m_primitiveTopology = config.primitiveTopology;

    // Create root signature
    {
        std::vector<CD3DX12_ROOT_PARAMETER1> rootParams = ShaderCompiler::GetInstance().GetRootParameters(config.shader);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Init_1_1(rootParams.size(), rootParams.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature{};
        ComPtr<ID3DBlob> error{};
        HRESULT          result = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
        DebugCheckCritical(
            SUCCEEDED(result),
            "Graphics pipeline {}: failed to serialize signature, error 0x{:x}",
            m_name,
            static_cast<uint32_t>(result)
        );

        result = app.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        DebugCheckCritical(
            SUCCEEDED(result),
            "Graphics pipeline {}: failed to create rootSignature, error 0x{:x}",
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


        const D3D12_SHADER_BYTECODE vsBytecode = GetShaderByteCode(SLANG_STAGE_VERTEX);
        const D3D12_SHADER_BYTECODE psBytecode = GetShaderByteCode(SLANG_STAGE_PIXEL);
        DebugInfo("Pipeline {}: VS={} bytes, PS={} bytes", m_name, vsBytecode.BytecodeLength, psBytecode.BytecodeLength);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
            .pRootSignature        = m_rootSignature.Get(),
            .VS                    = vsBytecode,
            .PS                    = psBytecode,
            .BlendState            = config.blendState,
            .SampleMask            = UINT_MAX,
            .RasterizerState       = config.rasterizer,
            .DepthStencilState     = config.depthStencil,
            .InputLayout           = config.inputLayout,
            .PrimitiveTopologyType = config.topology,
            .NumRenderTargets      = static_cast<uint32_t>(config.rtvFormats.size()),
            .DSVFormat             = config.dsvFormat,
            .SampleDesc            = config.sample,
        };
        for (uint32_t i = 0; i < config.rtvFormats.size(); ++i) {
            desc.RTVFormats[i] = config.rtvFormats[i];
        }

        HRESULT result = app.GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipelineState));
        DebugCheckCritical(
            SUCCEEDED(result),
            "Graphics pipeline {}: failed to create graphics pipeline state, error 0x{:x}",
            m_name,
            static_cast<uint32_t>(result)
        );
    }
}