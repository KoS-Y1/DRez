//
// Created by y1 on 2026-05-14.
//

#include "GbufferPass.h"

#include <ranges>

#include <directx/d3dx12_barriers.h>

#include "DXDebug.h"
#include "Mesh.h"
#include "ResourceManager.h"

GbufferPass::GbufferPass(
    DXApp                                                                  &dxApp,
    std::string_view                                                        inputFile,
    const std::vector<DXTexture>                                           &gbufferTextures,
    const std::vector<int32_t>                                             &gbufferRtvOffsets,
    int32_t                                                                 depthDsvOffset,
    uint32_t                                                                width,
    uint32_t                                                                height,
    const std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> &globalUniforms
)
    : Pass(dxApp, inputFile)
    , m_gbufferTextures(gbufferTextures)
    , m_gbufferRtvOffsets(gbufferRtvOffsets)
    , m_depthDsvOffset(depthDsvOffset)
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)})
    , m_scissor(CD3DX12_RECT{0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)})
    , m_globalUniforms(globalUniforms) {}

void GbufferPass::TransitionBarriers(const DrawContext &context) {
    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
    barriers.reserve(m_gbufferTextures.size());
    std::ranges::for_each(m_gbufferTextures, [&](const DXTexture &texture) {
        barriers.push_back(
            CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
        );
    });
    context.commandList->ResourceBarrier(barriers.size(), barriers.data());
}

void GbufferPass::BindResources(const DrawContext &context) {
    context.commandList->RSSetViewports(1, &m_viewport);
    context.commandList->RSSetScissorRects(1, &m_scissor);

    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    rtvHandles.reserve(m_gbufferRtvOffsets.size());
    std::ranges::for_each(m_gbufferRtvOffsets, [&](int32_t offset) { rtvHandles.push_back(m_app.GetRenderTargetViewHandle(offset)); });
    const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_depthDsvOffset);
    context.commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), FALSE, &dsvHandle);

    static constexpr float rtvClearColor[]{1.0f, 0.0f, 0.0f, 1.0f};
    std::ranges::for_each(rtvHandles, [&](const CD3DX12_CPU_DESCRIPTOR_HANDLE &handle) {
        context.commandList->ClearRenderTargetView(handle, rtvClearColor, 0, nullptr);
    });
    context.commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    context.commandList
        ->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::GlobalUniforms) / sizeof(uint32_t), &m_globalUniforms[context.frameIndex], 0);
}

void GbufferPass::Record(const DrawContext &context) {
    std::ranges::for_each(std::views::iota(0u, ResourceManager::GetInstance().GetInstanceCount()), [&](uint32_t i) {
        const Mesh                   &mesh         = ResourceManager::GetInstance().GetMesh(ResourceManager::GetInstance().GetInstanceInfo(i).meshHandle);
        const shader_io::TriangleMesh triangleMesh = mesh.GetMesh().triangleMesh;
        drez::dx::debug::ScopedEvent  drawScope{context.commandList, mesh.GetName()};
        context.commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());
        context.commandList->DrawIndexedInstanced(triangleMesh.indices.count, 1, 0, 0, i);
    });
}
