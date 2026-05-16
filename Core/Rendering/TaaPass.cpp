//
// Created by y1 on 2026-05-16.
//

#include "TaaPass.h"

#include <cmath>
#include <vector>

#include <directx/d3dx12_barriers.h>

TaaPass::TaaPass(
    DXApp                        &dxApp,
    std::string_view              inputFile,
    std::span<const DXTexture>    taaTextures,
    const DXTexture              &deferredTexture,
    const DXTexture              &velocityTexture,
    const DXTexture              &depthTexture,
    uint32_t                      width,
    uint32_t                      height,
    const shader_io::TaaUniforms &taaUniforms
)
    : Pass(dxApp, inputFile)
    , m_taaTextures(taaTextures)
    , m_deferredTexture(deferredTexture)
    , m_velocityTexture(velocityTexture)
    , m_depthTexture(depthTexture)
    , m_width(width)
    , m_height(height)
    , m_taaUniforms(taaUniforms) {
}

void TaaPass::TransitionBarriers(const DrawContext &context) {
    const std::vector<CD3DX12_RESOURCE_BARRIER> barriers{
        // Current TAA target: read state -> UAV write
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_taaTextures[context.frameIndex].GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        ),
        // Deferred src: skybox left it as a render target
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_deferredTexture.GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        ),
        // Depth: gbuffer/skybox left it as a depth target
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_depthTexture.GetResource(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        ),
    };
    context.commandList->ResourceBarrier(barriers.size(), barriers.data());
}

void TaaPass::BindResources(const DrawContext &context) {
    context.commandList->SetComputeRoot32BitConstants(0, sizeof(shader_io::TaaUniforms) / sizeof(uint32_t), &m_taaUniforms, 0);
}

void TaaPass::Record(const DrawContext &context) {
    context.commandList->Dispatch(
        static_cast<uint32_t>(std::ceil(m_width / shader_io::kTaaThreadX)),
        static_cast<uint32_t>(std::ceil(m_height / shader_io::kTaaThreadY)),
        1
    );
}

void TaaPass::FinalizeBarriers(const DrawContext &context) {
    const std::vector<CD3DX12_RESOURCE_BARRIER> barriers{
        // Restore deferred src for next frame's deferred pass
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_deferredTexture.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
        ),
        // Restore depth for next frame's gbuffer pass
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_depthTexture.GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        ),
    };
    context.commandList->ResourceBarrier(barriers.size(), barriers.data());
}