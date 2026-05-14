//
// Created by y1 on 2026-04-30.
//

#include "Renderer.h"

#include <algorithm>
#include <chrono>
#include <ranges>

#include <directx/d3dx12_barriers.h>
#include <directx/d3dx12_resource_helpers.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl3.h>

#include "BlitPass.h"
#include "DXDebug.h"
#include "DXUtils.h"
#include "DeferredPass.h"
#include "GbufferPass.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include "ShadowPass.h"
#include "SkyboxPass.h"
#include "VertexFormats.h"

namespace {
// Initial directional light shared by the shadow and deferred passes
constexpr DirectX::XMFLOAT3 kInitialLightDir{0.3f, 1.0f, 0.5f};
constexpr DirectX::XMFLOAT3 kInitialLightColor{0.9f, 0.50f, 0.2f};
constexpr float             kInitialLightIntensity{3.0f};

// Orthographic frustum the shadow map covers, in world units, centered on origin
constexpr float kShadowOrthoSize{20.0f};
constexpr float kShadowNear{0.1f};
constexpr float kShadowFar{100.0f};
constexpr float kLightDistance{10.0f};

constexpr uint32_t kMaxTimestampPasses{8};
} // namespace

Renderer::Renderer(DXApp &app, const Camera &camera)
    : m_app(app)
    , m_camera(camera)
    , m_width(app.GetWindowWidth())
    , m_height(app.GetWindowHeight())
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height)})
    , m_rect(CD3DX12_RECT{0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height)}) {
    // GPU timing
    m_timestamps = DXTimestamps{m_app, kMaxTimestampPasses};

    // Rendering resource
    {
        int32_t rtvOffset{0};
        int32_t dsvOffset{0};

        // Gbuffer attachments
        const std::vector<std::string>
            gbufferNames{"gbuffer0_texture", "gbuffer1_texture", "gbuffer2_texture", "gbuffer3_texture", "gubffer4_texture"};

        const std::vector<DXGI_FORMAT> gbufferFormats{
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R16G16_FLOAT,
        };

        m_gbufferTextures.reserve(gbufferNames.size());
        m_gbufferRtvOffsets.reserve(gbufferNames.size());
        m_gbufferSrvs.reserve(gbufferNames.size());

        std::ranges::for_each(std::views::iota(uint32_t{0}, gbufferNames.size()), [&](uint32_t i) {
            const D3D12_SHADER_RESOURCE_VIEW_DESC gbufferSrvDesc{
                .Format                  = gbufferFormats[i],
                .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D               = {.MipLevels = 1},
            };

            DXTexture texture = m_app.CreateTexture(
                m_width,
                m_height,
                gbufferFormats[i],
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                D3D12_HEAP_FLAG_NONE,
                shader_io::SamplerType::Nearest,
                gbufferNames[i]
            );
            m_app.CreateRenderTargetView(texture.GetResource(), rtvOffset);
            DXShaderResourceView srv = m_app.CreateDXShaderResourceView(texture.GetResource(), gbufferSrvDesc);

            m_gbufferTextures.push_back(std::move(texture));
            m_gbufferRtvOffsets.push_back(rtvOffset++);
            m_gbufferSrvs.push_back(std::move(srv));
        });

        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
            barriers.reserve(m_gbufferTextures.size());
            for (const auto &texture: m_gbufferTextures) {
                barriers.push_back(
                    CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
                );
            }
            commandList->ResourceBarrier(barriers.size(), barriers.data());
        });

        // Depth
        m_depthTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_D32_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Nearest,
            "depth_texture"
        );
        m_app.CreateDepthStencilView(m_depthTexture.GetResource(), dsvOffset);
        m_depthTextureDsvOffset = dsvOffset++;

        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            auto barrier =
                CD3DX12_RESOURCE_BARRIER::Transition(m_depthTexture.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            commandList->ResourceBarrier(1, &barrier);
        });

        // Shadow map (R32_TYPELESS so the resource can alias as DSV/D32_FLOAT and SRV/R32_FLOAT)
        m_shadowMapTexture = m_app.CreateTexture(
            kShadowMapSize,
            kShadowMapSize,
            DXGI_FORMAT_R32_TYPELESS,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Linear,
            "shadow_map_texture",
            DXGI_FORMAT_D32_FLOAT
        );
        const D3D12_DEPTH_STENCIL_VIEW_DESC shadowDsvDesc{
            .Format        = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags         = D3D12_DSV_FLAG_NONE,
            .Texture2D     = {.MipSlice = 0},
        };
        m_app.CreateDepthStencilView(m_shadowMapTexture.GetResource(), dsvOffset, shadowDsvDesc);
        m_shadowMapDsvOffset = dsvOffset++;

        const D3D12_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc{
            .Format                  = DXGI_FORMAT_R32_FLOAT,
            .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D               = {.MipLevels = 1},
        };
        m_shadowMapSrv = m_app.CreateDXShaderResourceView(m_shadowMapTexture.GetResource(), shadowSrvDesc);

        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_shadowMapTexture.GetResource(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        // Deferred composited HDR target (compute write -> skybox render -> blit read)
        m_deferredTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Linear,
            "deferred_texture"
        );
        const D3D12_SHADER_RESOURCE_VIEW_DESC deferredSrvDesc{
            .Format                  = m_deferredTexture.GetFormat(),
            .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D               = {.MipLevels = 1},
        };
        m_deferredTextureSrv = m_app.CreateDXShaderResourceView(m_deferredTexture.GetResource(), deferredSrvDesc);
        const D3D12_UNORDERED_ACCESS_VIEW_DESC deferredUavDesc{
            .Format        = m_deferredTexture.GetFormat(),
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
            .Texture2D     = {.MipSlice = 0, .PlaneSlice = 0},
        };
        m_deferredTextureUav = m_app.CreateDXUnorderedAccessView(m_deferredTexture.GetResource(), deferredUavDesc);
        m_app.CreateRenderTargetView(m_deferredTexture.GetResource(), rtvOffset);
        m_deferredTextureRtvOffset = rtvOffset++;

        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_deferredTexture.GetResource(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        // Composed (LDR present-ready) target
        m_composedTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Nearest,
            "composed_texture"
        );
        const D3D12_UNORDERED_ACCESS_VIEW_DESC composedUavDesc{
            .Format        = m_composedTexture.GetFormat(),
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
            .Texture2D     = {.MipSlice = 0, .PlaneSlice = 0},
        };
        m_composedTextureUav = m_app.CreateDXUnorderedAccessView(m_composedTexture.GetResource(), composedUavDesc);
        m_app.CreateRenderTargetView(m_composedTexture.GetResource(), rtvOffset);
        m_composedTextureRtvOffset = rtvOffset++;
        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            auto barrier =
                CD3DX12_RESOURCE_BARRIER::Transition(m_composedTexture.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
            commandList->ResourceBarrier(1, &barrier);
        });
    }

    // Skybox
    {
        const std::vector<VertexP> skyboxVertices{
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, 1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f)}
        };

        const uint64_t size = std::span<const VertexP>{skyboxVertices}.size_bytes();

        m_skyboxVertexBuffer =
            m_app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, size, "skybox_vetex_buffer");
        DXBuffer stagingBuffer =
            m_app.CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, size, "staging_buffer_skybox_vertex");

        D3D12_SUBRESOURCE_DATA data;
        data.pData      = skyboxVertices.data();
        data.RowPitch   = size;
        data.SlicePitch = size;


        m_app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_skyboxVertexBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_skyboxVertexBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        m_skyboxVertexBufferView.BufferLocation = m_skyboxVertexBuffer.GetGPUVirtualAddress();
        m_skyboxVertexBufferView.StrideInBytes  = sizeof(VertexP);
        m_skyboxVertexBufferView.SizeInBytes    = size;
    }

    // Deferred info buffer
    {
        shader_io::DeferredInfo info{};
        info.gbuffer0Index   = m_gbufferSrvs[0].GetIndex();
        info.gbuffer1Index   = m_gbufferSrvs[1].GetIndex();
        info.gbuffer2Index   = m_gbufferSrvs[2].GetIndex();
        info.gbuffer3Index   = m_gbufferSrvs[3].GetIndex();
        info.irradianceIndex = ResourceManager::GetInstance().GetIrradianceBindlessIndex();
        info.specularIndex   = ResourceManager::GetInstance().GetSpecularBindlessIndex();
        info.brdfLutIndex    = ResourceManager::GetInstance().GetBrdfLutBindlessIndex();
        info.shadowMapIndex  = m_shadowMapSrv.GetIndex();

        const uint64_t size = sizeof(shader_io::DeferredInfo);

        m_deferredInfoBuffer =
            m_app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, size, "deferred_info_buffer");
        DXBuffer stagingBuffer =
            m_app.CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, size, "staging_buffer_deferred_info");

        D3D12_SUBRESOURCE_DATA data{};
        data.pData      = &info;
        data.RowPitch   = size;
        data.SlicePitch = size;

        m_app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_deferredInfoBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_deferredInfoBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = DXGI_FORMAT_UNKNOWN,
            .ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer                  = {
                                        .FirstElement        = 0,
                                        .NumElements         = 1,
                                        .StructureByteStride = sizeof(shader_io::DeferredInfo),
                                        .Flags               = D3D12_BUFFER_SRV_FLAG_NONE,
                                        },
        };
        m_deferredInfoBufferSrv = m_app.CreateDXShaderResourceView(m_deferredInfoBuffer.GetBuffer(), desc);
    }

    // Uniforms
    {
        std::ranges::for_each(m_globalUniforms, [](shader_io::GlobalUniforms &uniforms) {
            uniforms.instancesIndex = ResourceManager::GetInstance().GetInstancesBindlessIndex();
            uniforms.meshesIndex    = ResourceManager::GetInstance().GetMeshesBindlessIndex();
            uniforms.materialsIndex = ResourceManager::GetInstance().GetMaterialsBindlessIndex();
        });

        m_deferredUniforms.deferredInfoIndex = m_deferredInfoBufferSrv.GetIndex();
        m_deferredUniforms.dstIndex          = m_deferredTextureUav.GetIndex();
        m_deferredUniforms.lightDir          = kInitialLightDir;
        m_deferredUniforms.lightColor        = kInitialLightColor;
        m_deferredUniforms.lightIntensity    = kInitialLightIntensity;

        m_skyboxUniforms.skyboxIndex = ResourceManager::GetInstance().GetSkyboxBindlessIndex();

        m_blitUniforms.srcIndex    = m_deferredTextureSrv.GetIndex();
        m_blitUniforms.dstIndex    = m_composedTextureUav.GetIndex();
        m_blitUniforms.samplerType = m_deferredTexture.GetSamplerType();
    }

    // Passes (executed in vector order each frame)
    {
        m_passes.push_back(
            std::make_unique<ShadowPass>(
                m_app,
                "../Assets/Shaders/shadow.json",
                m_shadowMapTexture,
                m_shadowMapDsvOffset,
                kShadowMapSize,
                m_globalUniforms,
                m_lightSpaceMatrix
            )
        );
        m_passes.push_back(
            std::make_unique<GbufferPass>(
                m_app,
                "../Assets/Shaders/gbuffer.json",
                m_gbufferTextures,
                m_gbufferRtvOffsets,
                m_depthTextureDsvOffset,
                m_width,
                m_height,
                m_globalUniforms
            )
        );
        m_passes.push_back(
            std::make_unique<
                DeferredPass>(m_app, "../Assets/Shaders/deferred.json", m_gbufferTextures, m_deferredTexture, m_width, m_height, m_deferredUniforms)
        );
        m_passes.push_back(
            std::make_unique<SkyboxPass>(
                m_app,
                "../Assets/Shaders/skybox.json",
                m_deferredTexture,
                m_deferredTextureRtvOffset,
                m_depthTextureDsvOffset,
                m_skyboxVertexBufferView,
                m_width,
                m_height,
                m_skyboxUniforms
            )
        );
        m_passes.push_back(
            std::make_unique<BlitPass>(m_app, "../Assets/Shaders/blit.json", m_deferredTexture, m_composedTexture, m_width, m_height, m_blitUniforms)
        );
    }
}

Renderer::~Renderer() {
    m_app.WaitForGpu();
}

void Renderer::Render() {
    FrameInfo                  frameInfo   = m_app.BeginFrame();
    ID3D12GraphicsCommandList *commandList = frameInfo.commandList;
    uint32_t                   frameIndex  = frameInfo.frameIndex;

    m_timestamps.BeginFrame(frameIndex);

    // ImGui new frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    BuildImGuiContent();

    Update(frameInfo);

    const DrawContext context{
        .commandList = commandList,
        .timestamps  = &m_timestamps,
        .frameIndex  = frameIndex,
    };

    std::ranges::for_each(m_passes, [&](const std::unique_ptr<Pass> &pass) { pass->Execute(context); });

    // ImGui pass
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "ImGui pass"};
        m_timestamps.BeginPass(commandList, "ImGui");

        auto toRtv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_composedTexture.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        commandList->ResourceBarrier(1, &toRtv);

        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_app.GetRenderTargetViewHandle(m_composedTextureRtvOffset);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_rect);

        ID3D12DescriptorHeap * const imguiHeap = m_app.GetImGuiDescriptorHeap();
        commandList->SetDescriptorHeaps(1, &imguiHeap);

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

        auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
            m_composedTexture.GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
        commandList->ResourceBarrier(1, &toCopy);

        m_timestamps.EndPass(commandList);
    }

    // Copy to present
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Copy to present"};
        m_app.CopyToPresentImage(m_composedTexture.GetResource());
    }

    m_timestamps.EndFrame(commandList);

    m_app.EndFrame();
}

void Renderer::Update(const FrameInfo &frameInfo) {
    const uint32_t frameIndex = frameInfo.frameIndex;

    // Frame timing
    {
        using clock    = std::chrono::steady_clock;
        const auto now = clock::now();
        if (m_lastFrameTickCount != 0) {
            const clock::time_point                        last{clock::duration{m_lastFrameTickCount}};
            const std::chrono::duration<float, std::milli> delta = now - last;
            m_lastFrameTimeMs                                    = delta.count();
            m_frameTimeHistory[m_frameTimeHistoryOffset]         = m_lastFrameTimeMs;
            m_frameTimeHistoryOffset                             = (m_frameTimeHistoryOffset + 1) % kFrameHistorySize;
        }
        m_lastFrameTickCount = now.time_since_epoch().count();
    }

    const uint32_t lastFrameIndex = frameIndex > 0 ? frameIndex - 1 : DXApp::kMaxFramesInFlight - 1;

    const DirectX::XMFLOAT4X4 viewFloat = m_camera.GetViewMatrix();
    const DirectX::XMFLOAT4X4 projFloat = m_camera.GetProjectionMatrix();
    const DirectX::XMMATRIX   view      = DirectX::XMLoadFloat4x4(&viewFloat);
    const DirectX::XMMATRIX   proj      = DirectX::XMLoadFloat4x4(&projFloat);
    const DirectX::XMMATRIX   viewProj  = DirectX::XMMatrixMultiply(view, proj);
    m_globalUniforms[frameIndex].prevViewProj = m_globalUniforms[lastFrameIndex].viewProj;
    DirectX::XMStoreFloat4x4(&m_globalUniforms[frameIndex].viewProj, viewProj);

    DirectX::XMMATRIX viewNoTrans = view;
    viewNoTrans.r[3]              = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    DirectX::XMStoreFloat4x4(&m_skyboxUniforms.viewProj, DirectX::XMMatrixMultiply(viewNoTrans, proj));

    const DirectX::XMFLOAT3 eye  = m_camera.GetLocation();
    m_deferredUniforms.cameraPos = {eye.x, eye.y, eye.z};

    // Light-space matrix (depends on editable light direction)
    {
        using namespace DirectX;
        const XMVECTOR L             = XMVector3Normalize(XMLoadFloat3(&m_deferredUniforms.lightDir));
        const XMVECTOR sceneCenter   = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMVECTOR lightPos      = XMVectorAdd(sceneCenter, XMVectorScale(L, kLightDistance));
        const XMVECTOR worldUp       = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX lightView     = XMMatrixLookAtRH(lightPos, sceneCenter, worldUp);
        const XMMATRIX lightProj     = XMMatrixOrthographicRH(kShadowOrthoSize, kShadowOrthoSize, kShadowNear, kShadowFar);
        const XMMATRIX lightViewProj = XMMatrixMultiply(lightView, lightProj);
        XMStoreFloat4x4(&m_lightSpaceMatrix, lightViewProj);
        m_deferredUniforms.lightSpaceMatrix = m_lightSpaceMatrix;
    }
}

void Renderer::BuildImGuiContent() {
    if (ImGui::Begin("Debug Window")) {
        if (ImGui::CollapsingHeader("Overall Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Frame Time: %.3f ms", m_lastFrameTimeMs);
            ImGui::Text("FPS: %.1f", m_lastFrameTimeMs > 0.0f ? 1000.0f / m_lastFrameTimeMs : 0.0f);
            ImGui::PlotLines(
                "Frame Time History",
                m_frameTimeHistory.data(),
                static_cast<int>(m_frameTimeHistory.size()),
                static_cast<int>(m_frameTimeHistoryOffset),
                nullptr,
                0.0f,
                33.3f,
                ImVec2(0, 80)
            );
        }

        if (ImGui::CollapsingHeader("Per-Pass Performance (GPU)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto        &timings = m_timestamps.GetTimings();
            std::vector<float> values;
            values.reserve(timings.size());
            float maxMs = 0.0f;
            std::ranges::for_each(timings, [&](const DXTimestamps::PassTime &t) {
                ImGui::Text("%s: %.3f ms", t.name.c_str(), t.milliseconds);
                values.push_back(t.milliseconds);
                maxMs = std::max(maxMs, t.milliseconds);
            });
            if (!values.empty()) {
                ImGui::PlotHistogram("Per-Pass (ms)", values.data(), static_cast<int>(values.size()), 0, nullptr, 0.0f, maxMs * 1.2f, ImVec2(0, 80));
            }
        }
    }
    ImGui::End();

    if (ImGui::Begin("Sunlight")) {
        ImGui::DragFloat3("Direction", &m_deferredUniforms.lightDir.x, 0.01f, -1.0f, 1.0f);
        ImGui::ColorEdit3("Color", &m_deferredUniforms.lightColor.x);
        ImGui::DragFloat("Intensity", &m_deferredUniforms.lightIntensity, 0.1f, 0.0f, 100.0f);
    }
    ImGui::End();
}
