//
// Created by y1 on 2026-05-14.
//

#include "BlitPass.h"

#include <cmath>
#include <vector>

#include <directx/d3dx12.h>
#include <directx/d3dx12_barriers.h>

BlitPass::BlitPass(
    DXApp                          &dxApp,
    std::string_view                inputFile,
    const DXTexture                &deferredTexture,
    const DXTexture                &composedTexture,
    uint32_t                        width,
    uint32_t                        height,
    const shader_io::BlitUniforms  &blitUniforms
)
    : Pass(dxApp, inputFile)
    , m_deferredTexture(deferredTexture)
    , m_composedTexture(composedTexture)
    , m_width(width)
    , m_height(height)
    , m_blitUniforms(blitUniforms) {}

void BlitPass::TransitionBarriers(const DrawContext &context) {
    const std::vector<CD3DX12_RESOURCE_BARRIER> barriers{
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_deferredTexture.GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_composedTexture.GetResource(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        ),
    };
    context.commandList->ResourceBarrier(barriers.size(), barriers.data());
}

void BlitPass::BindResources(const DrawContext &context) {
    context.commandList->SetComputeRoot32BitConstants(0, sizeof(shader_io::BlitUniforms) / sizeof(uint32_t), &m_blitUniforms, 0);
}

void BlitPass::Record(const DrawContext &context) {
    context.commandList->Dispatch(
        static_cast<uint32_t>(std::ceil(m_width / shader_io::kBlitThreadX)),
        static_cast<uint32_t>(std::ceil(m_height / shader_io::kBlitThreadY)),
        1
    );
}
