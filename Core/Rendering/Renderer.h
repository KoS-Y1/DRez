//
// Created by y1 on 2026-04-30.
//

#pragma once

#include <array>
#include <memory>
#include <vector>

#include <directx/d3dx12.h>
#include <directxmath.h>

#include "Camera.h"
#include "DXApp.h"
#include "DXBuffer.h"
#include "DXShaderResourceView.h"
#include "DXTexture.h"
#include "DXTimestamps.h"
#include "DXUnorderedAccessView.h"
#include "Pass.h"

#include "global_io.slang"
#include "resources_io.slang"

class Renderer {
public:
    Renderer() = delete;
    Renderer(DXApp &app, const Camera &camera);

    Renderer(const Renderer &)            = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&)                 = delete;
    Renderer &operator=(Renderer &&)      = delete;

    ~Renderer() = default;

    void Render();


private:
    DXApp        &m_app;
    const Camera &m_camera;
    uint32_t      m_width{};
    uint32_t      m_height{};

    CD3DX12_VIEWPORT m_viewport{};
    CD3DX12_RECT     m_rect{};

    void Update(const FrameInfo &frameInfo);
    void BuildImGuiContent();

private:
    // Resources
    std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> m_globalUniforms{};
    shader_io::DeferredUniforms                                      m_deferredUniforms{};
    shader_io::SkyboxUniforms                                        m_skyboxUniforms{};
    shader_io::BlitUniforms                                          m_blitUniforms{};

    DXBuffer m_skyboxVertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW m_skyboxVertexBufferView{};

    DXTexture             m_composedTexture{};
    DXUnorderedAccessView m_composedTextureUav{};
    int32_t               m_composedTextureRtvOffset{};

    DXTexture m_depthTexture{};
    int32_t   m_depthTextureDsvOffset{};

    // Shadow map
    static constexpr uint32_t kShadowMapSize = 2048;
    DXTexture                 m_shadowMapTexture{};
    DXShaderResourceView      m_shadowMapSrv{};
    int32_t                   m_shadowMapDsvOffset{};
    DirectX::XMFLOAT4X4       m_lightSpaceMatrix{};

    // Gbuffer attachments
    std::vector<DXTexture>            m_gbufferTextures{};
    std::vector<int32_t>              m_gbufferRtvOffsets{};
    std::vector<DXShaderResourceView> m_gbufferSrvs{};

    // Deferred composited (HDR) target
    DXTexture             m_deferredTexture{};
    DXShaderResourceView  m_deferredTextureSrv{};
    DXUnorderedAccessView m_deferredTextureUav{};
    int32_t               m_deferredTextureRtvOffset{};

    // Deferred bindless info buffer
    DXBuffer             m_deferredInfoBuffer{};
    DXShaderResourceView m_deferredInfoBufferSrv{};

    // Passes (executed in vector order each frame)
    std::vector<std::unique_ptr<Pass>> m_passes;

    // GPU timing
    DXTimestamps m_timestamps{};

    // Frame timing history
    static constexpr uint32_t            kFrameHistorySize{120};
    std::array<float, kFrameHistorySize> m_frameTimeHistory{};
    uint32_t                             m_frameTimeHistoryOffset{};
    float                                m_lastFrameTimeMs{};
    int64_t                              m_lastFrameTickCount{};
};
