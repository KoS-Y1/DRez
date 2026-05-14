//
// Created by y1 on 2026-05-14.
//

#include "ShadowPass.h"

#include <ranges>

#include <directx/d3dx12_barriers.h>

#include "DXDebug.h"
#include "Mesh.h"
#include "ResourceManager.h"

ShadowPass::ShadowPass(
    DXApp                                                                  &dxApp,
    std::string_view                                                        inputFile,
    const DXTexture                                                        &shadowMap,
    int32_t                                                                 shadowMapDsvOffset,
    uint32_t                                                                shadowMapSize,
    const std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> &globalUniforms,
    const DirectX::XMFLOAT4X4                                              &lightSpaceMatrix
)
    : Pass(dxApp, inputFile)
    , m_shadowMap(shadowMap)
    , m_shadowMapDsvOffset(shadowMapDsvOffset)
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(shadowMapSize), static_cast<float>(shadowMapSize)})
    , m_scissor(CD3DX12_RECT{0, 0, static_cast<int32_t>(shadowMapSize), static_cast<int32_t>(shadowMapSize)})
    , m_globalUniforms(globalUniforms)
    , m_lightSpaceMatrix(lightSpaceMatrix) {}

void ShadowPass::TransitionBarriers(const DrawContext &context) {
    auto barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    context.commandList->ResourceBarrier(1, &barrier);
}

void ShadowPass::BindResources(const DrawContext &context) {
    context.commandList->RSSetViewports(1, &m_viewport);
    context.commandList->RSSetScissorRects(1, &m_scissor);

    const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_shadowMapDsvOffset);
    context.commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
    context.commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Reuse GlobalUniforms; viewProj is repurposed as the light-space matrix here
    shader_io::GlobalUniforms shadowUniforms = m_globalUniforms[context.frameIndex];
    shadowUniforms.viewProj                  = m_lightSpaceMatrix;
    context.commandList->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::GlobalUniforms) / sizeof(uint32_t), &shadowUniforms, 0);
}

void ShadowPass::Record(const DrawContext &context) {
    std::ranges::for_each(std::views::iota(0u, ResourceManager::GetInstance().GetInstanceCount()), [&](uint32_t i) {
        const Mesh                   &mesh         = ResourceManager::GetInstance().GetMesh(ResourceManager::GetInstance().GetInstanceInfo(i).meshHandle);
        const shader_io::TriangleMesh triangleMesh = mesh.GetMesh().triangleMesh;
        drez::dx::debug::ScopedEvent  drawScope{context.commandList, mesh.GetName()};
        context.commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());
        context.commandList->DrawIndexedInstanced(triangleMesh.indices.count, 1, 0, 0, i);
    });
}

void ShadowPass::FinalizeBarriers(const DrawContext &context) {
    // Return shadow map to shader-resource state for downstream sampling passes
    auto readBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    context.commandList->ResourceBarrier(1, &readBarrier);
}
