//
// Created by y1 on 2026-05-16.
//

#include "TaaPass.h"

#include <cmath>
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
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_taaTextures[context.frameIndex].GetResource(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    context.commandList->ResourceBarrier(1, &barrier);
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