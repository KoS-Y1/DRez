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
#include "DXGraphicsPipeline.h"
#include "DXComputePipeline.h"
#include "DXTexture.h"

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

    // TODO: for testing
    DXGraphicsPipeline m_forward{};
    DXComputePipeline m_blit{};

    void Update(const FrameInfo &frameInfo);

private:
    // Resources
    std::vector<shader_io::InstanceInfo> m_instances{};
    std::vector<DirectX::XMFLOAT4X4>     m_instanceTransforms{};

    std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> m_globalUniforms{};
    shader_io::BlitUniforms m_blitUniforms{};

    DXBuffer m_instanceBuffer{};
    uint32_t m_instanceBufferIndex{};

    DXTexture m_composedTexture{};

    DXTexture m_depthTexture{};
    int32_t   m_depthTextureDsvOffset{};

    DXTexture m_deferredTexture{};
    int32_t   m_deferredTextureRtvOffset{};

};
