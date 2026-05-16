//
// Created by y1 on 2026-05-14.
//

#include "DeferredPass.h"

#include <algorithm>
#include <cmath>
#include <ranges>

#include <directx/d3dx12.h>
#include <directx/d3dx12_barriers.h>

DeferredPass::DeferredPass(
    DXApp                             &dxApp,
    std::string_view                   inputFile,
    std::span<const DXTexture>         gbufferTextures,
    const DXTexture                   &deferredTexture,
    uint32_t                           width,
    uint32_t                           height,
    const shader_io::DeferredUniforms &deferredUniforms
)
    : Pass(dxApp, inputFile)
    , m_gbufferTextures(gbufferTextures)
    , m_deferredTexture(deferredTexture)
    , m_width(width)
    , m_height(height)
    , m_deferredUniforms(deferredUniforms) {
}

void DeferredPass::TransitionBarriers(const DrawContext &context) {
    std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
    barriers.reserve(m_gbufferTextures.size() + 1);
    std::ranges::for_each(m_gbufferTextures, [&](const DXTexture &texture) {
        barriers.push_back(
            CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
        );
    });
    barriers.push_back(
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_deferredTexture.GetResource(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        )
    );
    context.commandList->ResourceBarrier(barriers.size(), barriers.data());
}

void DeferredPass::BindResources(const DrawContext &context) {
    context.commandList->SetComputeRoot32BitConstants(0, sizeof(shader_io::DeferredUniforms) / sizeof(uint32_t), &m_deferredUniforms, 0);
}

void DeferredPass::Record(const DrawContext &context) {
    context.commandList->Dispatch(
        static_cast<uint32_t>(std::ceil(m_width / shader_io::kDeferredThreadX)),
        static_cast<uint32_t>(std::ceil(m_height / shader_io::kDeferredThreadY)),
        1
    );
}
