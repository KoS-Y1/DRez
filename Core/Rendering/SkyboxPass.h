//
// Created by y1 on 2026-05-14.
//

#pragma once

#include <directx/d3dx12.h>

#include "DXApp.h"
#include "DXTexture.h"
#include "Pass.h"

#include "global_io.slang"

class SkyboxPass : public Pass {
public:
    SkyboxPass(
        DXApp                                &dxApp,
        std::string_view                      inputFile,
        const DXTexture                      &deferredTexture,
        int32_t                               deferredTextureRtvOffset,
        int32_t                               depthDsvOffset,
        const D3D12_VERTEX_BUFFER_VIEW       &skyboxVertexBufferView,
        uint32_t                              width,
        uint32_t                              height,
        const shader_io::SkyboxUniforms      &skyboxUniforms
    );

protected:
    void TransitionBarriers(const DrawContext &context) override;
    void BindResources(const DrawContext &context) override;
    void Record(const DrawContext &context) override;

private:
    const DXTexture                 &m_deferredTexture;
    int32_t                          m_deferredTextureRtvOffset;
    int32_t                          m_depthDsvOffset;
    const D3D12_VERTEX_BUFFER_VIEW  &m_skyboxVertexBufferView;
    CD3DX12_VIEWPORT                 m_viewport;
    CD3DX12_RECT                     m_scissor;
    const shader_io::SkyboxUniforms &m_skyboxUniforms;
};
