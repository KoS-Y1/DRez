//
// Created by y1 on 2026-05-14.
//

#include "SkyboxPass.h"

#include <directx/d3dx12_barriers.h>

SkyboxPass::SkyboxPass(
    DXApp                                &dxApp,
    std::string_view                      inputFile,
    const DXTexture                      &deferredTexture,
    int32_t                               deferredTextureRtvOffset,
    int32_t                               depthDsvOffset,
    const D3D12_VERTEX_BUFFER_VIEW       &skyboxVertexBufferView,
    uint32_t                              width,
    uint32_t                              height,
    const shader_io::SkyboxUniforms      &skyboxUniforms
)
    : Pass(dxApp, inputFile)
    , m_deferredTexture(deferredTexture)
    , m_deferredTextureRtvOffset(deferredTextureRtvOffset)
    , m_depthDsvOffset(depthDsvOffset)
    , m_skyboxVertexBufferView(skyboxVertexBufferView)
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)})
    , m_scissor(CD3DX12_RECT{0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)})
    , m_skyboxUniforms(skyboxUniforms) {}

void SkyboxPass::TransitionBarriers(const DrawContext &context) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_deferredTexture.GetResource(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    context.commandList->ResourceBarrier(1, &barrier);
}

void SkyboxPass::BindResources(const DrawContext &context) {
    context.commandList->RSSetViewports(1, &m_viewport);
    context.commandList->RSSetScissorRects(1, &m_scissor);

    const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_app.GetRenderTargetViewHandle(m_deferredTextureRtvOffset);
    const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_depthDsvOffset);
    context.commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    context.commandList->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::SkyboxUniforms) / sizeof(uint32_t), &m_skyboxUniforms, 0);

    context.commandList->IASetVertexBuffers(0, 1, &m_skyboxVertexBufferView);
}

void SkyboxPass::Record(const DrawContext &context) {
    context.commandList->DrawInstanced(m_skyboxVertexBufferView.SizeInBytes / m_skyboxVertexBufferView.StrideInBytes, 1, 0, 0);
}
