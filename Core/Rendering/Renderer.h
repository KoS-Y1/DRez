//
// Created by y1 on 2026-04-30.
//

#pragma once

#include <array>
#include <vector>

#include <directx/d3dx12.h>
#include <directxmath.h>

#include "Camera.h"
#include "DXApp.h"
#include "DXBuffer.h"
#include "DXComputePipeline.h"
#include "DXGraphicsPipeline.h"
#include "DXShaderResourceView.h"
#include "DXTexture.h"
#include "DXUnorderedAccessView.h"

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

    DXGraphicsPipeline m_gbuffer{};
    DXComputePipeline  m_deferred{};
    DXGraphicsPipeline m_skybox{};
    DXComputePipeline  m_blit{};

    void Update(const FrameInfo &frameInfo);

private:
    // Resources
    std::vector<shader_io::InstanceInfo> m_instances{};
    std::vector<DirectX::XMFLOAT4X4>     m_instanceTransforms{};

    std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> m_globalUniforms{};
    shader_io::DeferredUniforms                                      m_deferredUniforms{};
    shader_io::SkyboxUniforms                                        m_skyboxUniforms{};
    shader_io::BlitUniforms                                          m_blitUniforms{};

    DXBuffer             m_instanceBuffer{};
    DXShaderResourceView m_instanceBufferSrv{};

    DXBuffer m_skyboxVertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW m_skyboxVertexBufferView{};

    DXTexture             m_composedTexture{};
    DXUnorderedAccessView m_composedTextureUav{};

    DXTexture m_depthTexture{};
    int32_t   m_depthTextureDsvOffset{};

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
};
