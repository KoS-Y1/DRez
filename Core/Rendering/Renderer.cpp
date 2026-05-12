//
// Created by y1 on 2026-04-30.
//

#include "Renderer.h"

#include <algorithm>
#include <ranges>

#include <directx/d3dx12_barriers.h>
#include <directx/d3dx12_resource_helpers.h>

#include "DXDebug.h"
#include "DXUtils.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include "VertexFormats.h"

namespace {
// Fixed directional light shared by the shadow and deferred passes
constexpr DirectX::XMFLOAT3 kLightDir{0.3f, 1.0f, 0.5f};
constexpr DirectX::XMFLOAT3 kLightColor{0.9f, 0.50f, 0.2f};
constexpr float             kLightIntensity{3.0f};

// Orthographic frustum the shadow map covers, in world units, centered on origin
constexpr float kShadowOrthoSize{20.0f};
constexpr float kShadowNear{0.1f};
constexpr float kShadowFar{100.0f};
constexpr float kLightDistance{10.0f};
} // namespace

Renderer::Renderer(DXApp &app, const Camera &camera)
    : m_app(app)
    , m_camera(camera)
    , m_width(app.GetWindowWidth())
    , m_height(app.GetWindowHeight())
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height)})
    , m_rect(CD3DX12_RECT{0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height)})
    , m_shadowViewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(kShadowMapSize), static_cast<float>(kShadowMapSize)})
    , m_shadowRect(CD3DX12_RECT{0, 0, static_cast<int32_t>(kShadowMapSize), static_cast<int32_t>(kShadowMapSize)}) {
    // Load pipelines
    {
        m_shadow   = m_app.CreateGraphicsPipeline("../Assets/Shaders/shadow.json");
        m_gbuffer  = m_app.CreateGraphicsPipeline("../Assets/Shaders/gbuffer.json");
        m_deferred = m_app.CreateComputePipeline("../Assets/Shaders/deferred.json");
        m_skybox   = m_app.CreateGraphicsPipeline("../Assets/Shaders/skybox.json");
        m_blit     = m_app.CreateComputePipeline("../Assets/Shaders/blit.json");
    }

    // Light-space matrix
    {
        using namespace DirectX;
        const XMVECTOR L              = XMVector3Normalize(XMLoadFloat3(&kLightDir));
        const XMVECTOR sceneCenter    = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMVECTOR lightPos       = XMVectorAdd(sceneCenter, XMVectorScale(L, kLightDistance));
        const XMVECTOR worldUp        = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX lightView      = XMMatrixLookAtRH(lightPos, sceneCenter, worldUp);
        const XMMATRIX lightProj      = XMMatrixOrthographicRH(kShadowOrthoSize, kShadowOrthoSize, kShadowNear, kShadowFar);
        const XMMATRIX lightViewProj  = XMMatrixMultiply(lightView, lightProj);
        XMStoreFloat4x4(&m_lightSpaceMatrix, lightViewProj);
    }

    // Rendering resource
    {
        int32_t rtvOffset{0};
        int32_t dsvOffset{0};

        // Gbuffer attachments
        const std::vector<std::string> gbufferNames{
            "gbuffer0_texture",
            "gbuffer1_texture",
            "gbuffer2_texture",
            "gbuffer3_texture",
        };
        constexpr DXGI_FORMAT                 kGbufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
        const D3D12_SHADER_RESOURCE_VIEW_DESC gbufferSrvDesc{
            .Format                  = kGbufferFormat,
            .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D               = {.MipLevels = 1},
        };

        m_gbufferTextures.reserve(gbufferNames.size());
        m_gbufferRtvOffsets.reserve(gbufferNames.size());
        m_gbufferSrvs.reserve(gbufferNames.size());

        for (const auto &name: gbufferNames) {
            DXTexture texture = m_app.CreateTexture(
                m_width,
                m_height,
                kGbufferFormat,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                D3D12_HEAP_FLAG_NONE,
                shader_io::SamplerType::Nearest,
                name
            );
            m_app.CreateRenderTargetView(texture.GetResource(), rtvOffset);
            DXShaderResourceView srv = m_app.CreateDXShaderResourceView(texture.GetResource(), gbufferSrvDesc);

            m_gbufferTextures.push_back(std::move(texture));
            m_gbufferRtvOffsets.push_back(rtvOffset++);
            m_gbufferSrvs.push_back(std::move(srv));
        }

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
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
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
        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            auto barrier =
                CD3DX12_RESOURCE_BARRIER::Transition(m_composedTexture.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
            commandList->ResourceBarrier(1, &barrier);
        });
    }

    // Skybox
    {
        const std::vector<VertexP> skyboxVertices{
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, -1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, 1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, 1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(-1.0f, 1.0f, -1.0f)},
            VertexP{DirectX::XMFLOAT3(1.0f, 1.0f, -1.0f)}
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

        m_deferredUniforms.lightSpaceMatrix  = m_lightSpaceMatrix;
        m_deferredUniforms.deferredInfoIndex = m_deferredInfoBufferSrv.GetIndex();
        m_deferredUniforms.dstIndex          = m_deferredTextureUav.GetIndex();
        m_deferredUniforms.lightDir          = kLightDir;
        m_deferredUniforms.lightColor        = kLightColor;
        m_deferredUniforms.lightIntensity    = kLightIntensity;

        m_skyboxUniforms.skyboxIndex = ResourceManager::GetInstance().GetSkyboxBindlessIndex();

        m_blitUniforms.srcIndex    = m_deferredTextureSrv.GetIndex();
        m_blitUniforms.dstIndex    = m_composedTextureUav.GetIndex();
        m_blitUniforms.samplerType = m_deferredTexture.GetSamplerType();
    }
}

void Renderer::Render() {
    FrameInfo                  frameInfo   = m_app.BeginFrame();
    ID3D12GraphicsCommandList *commandList = frameInfo.commandList;
    uint32_t                   frameIndex  = frameInfo.frameIndex;

    Update(frameInfo);

    // Shadow pass
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Shadow pass"};

        commandList->SetGraphicsRootSignature(m_shadow.GetRootSignature());
        commandList->SetPipelineState(m_shadow.GetPipelineState());
        commandList->RSSetViewports(1, &m_shadowViewport);
        commandList->RSSetScissorRects(1, &m_shadowRect);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_shadowMapTexture.GetResource(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        );
        commandList->ResourceBarrier(1, &barrier);

        const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_shadowMapDsvOffset);
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        commandList->IASetPrimitiveTopology(m_shadow.GetPrimitiveTopology());

        // Reuse GlobalUniforms; viewProj is repurposed as the light-space matrix here
        shader_io::GlobalUniforms shadowUniforms = m_globalUniforms[frameIndex];
        shadowUniforms.viewProj                  = m_lightSpaceMatrix;
        commandList->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::GlobalUniforms) / sizeof(uint32_t), &shadowUniforms, 0);

        std::ranges::for_each(std::views::iota(0u, ResourceManager::GetInstance().GetInstanceCount()), [&](uint32_t i) {
            const Mesh                   &mesh         = ResourceManager::GetInstance().GetMesh(ResourceManager::GetInstance().GetInstanceInfo(i).meshHandle);
            const shader_io::TriangleMesh triangleMesh = mesh.GetMesh().triangleMesh;
            drez::dx::debug::ScopedEvent drawScope{commandList, mesh.GetName()};
            commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());
            commandList->DrawIndexedInstanced(triangleMesh.indices.count, 1, 0, 0, i);
        });

        auto readBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_shadowMapTexture.GetResource(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
        );
        commandList->ResourceBarrier(1, &readBarrier);
    }

    // Gbuffer pass
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Gbuffer pass"};

        commandList->SetGraphicsRootSignature(m_gbuffer.GetRootSignature());
        commandList->SetPipelineState(m_gbuffer.GetPipelineState());
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_rect);

        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_gbufferTextures.size());
        for (const auto &texture: m_gbufferTextures) {
            barriers.push_back(
                CD3DX12_RESOURCE_BARRIER::Transition(
                    texture.GetResource(),
                    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_RENDER_TARGET
                )
            );
        }
        commandList->ResourceBarrier(barriers.size(), barriers.data());

        std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
        rtvHandles.reserve(m_gbufferRtvOffsets.size());
        for (const int32_t offset: m_gbufferRtvOffsets) {
            rtvHandles.push_back(m_app.GetRenderTargetViewHandle(offset));
        }
        const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_depthTextureDsvOffset);
        commandList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), FALSE, &dsvHandle);

        static constexpr float rtvClearColor[]{0.0f, 0.0f, 0.0f, 0.0f};
        for (const auto &handle: rtvHandles) {
            commandList->ClearRenderTargetView(handle, rtvClearColor, 0, nullptr);
        }
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        commandList->IASetPrimitiveTopology(m_gbuffer.GetPrimitiveTopology());

        commandList->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::GlobalUniforms) / sizeof(uint32_t), &m_globalUniforms[frameIndex], 0);

        std::ranges::for_each(std::views::iota(0u, ResourceManager::GetInstance().GetInstanceCount()), [&](uint32_t i) {
            const Mesh                   &mesh         = ResourceManager::GetInstance().GetMesh(ResourceManager::GetInstance().GetInstanceInfo(i).meshHandle);
            const shader_io::TriangleMesh triangleMesh = mesh.GetMesh().triangleMesh;
            drez::dx::debug::ScopedEvent drawScope{commandList, mesh.GetName()};
            commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());
            commandList->DrawIndexedInstanced(triangleMesh.indices.count, 1, 0, 0, i);
        });
    }

    // Deferred pass
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Deferred pass"};

        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_gbufferTextures.size() + 1);
        for (const auto &texture: m_gbufferTextures) {
            barriers.push_back(
                CD3DX12_RESOURCE_BARRIER::Transition(
                    texture.GetResource(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
                )
            );
        }
        barriers.push_back(
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_deferredTexture.GetResource(),
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            )
        );
        commandList->ResourceBarrier(barriers.size(), barriers.data());

        commandList->SetComputeRootSignature(m_deferred.GetRootSignature());
        commandList->SetPipelineState(m_deferred.GetPipelineState());
        commandList->SetComputeRoot32BitConstants(0, sizeof(shader_io::DeferredUniforms) / sizeof(uint32_t), &m_deferredUniforms, 0);

        commandList->Dispatch(
            static_cast<uint32_t>(std::ceil(m_width / shader_io::kDeferredThreadX)),
            static_cast<uint32_t>(std::ceil(m_height / shader_io::kDeferredThreadY)),
            1
        );
    }

    // Skybox pass
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Skybox pass"};

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_deferredTexture.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        commandList->ResourceBarrier(1, &barrier);

        commandList->SetGraphicsRootSignature(m_skybox.GetRootSignature());
        commandList->SetPipelineState(m_skybox.GetPipelineState());
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_rect);

        const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_app.GetRenderTargetViewHandle(m_deferredTextureRtvOffset);
        const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_depthTextureDsvOffset);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        commandList->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::SkyboxUniforms) / sizeof(uint32_t), &m_skyboxUniforms, 0);

        commandList->IASetPrimitiveTopology(m_skybox.GetPrimitiveTopology());
        commandList->IASetVertexBuffers(0, 1, &m_skyboxVertexBufferView);
        commandList->DrawInstanced(m_skyboxVertexBufferView.SizeInBytes / m_skyboxVertexBufferView.StrideInBytes, 1, 0, 0);
    }

    // Blit pass
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Blit pass"};

        commandList->SetComputeRootSignature(m_blit.GetRootSignature());
        commandList->SetPipelineState(m_blit.GetPipelineState());
        commandList->SetComputeRoot32BitConstants(0, sizeof(shader_io::BlitUniforms) / sizeof(uint32_t), &m_blitUniforms, 0);

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
            )
        };
        commandList->ResourceBarrier(barriers.size(), barriers.data());

        commandList->Dispatch(
            static_cast<uint32_t>(std::ceil(m_width / shader_io::kBlitThreadX)),
            static_cast<uint32_t>(std::ceil(m_height / shader_io::kBlitThreadY)),
            1
        );
    }

    // Copy to present
    {
        drez::dx::debug::ScopedEvent passScope{commandList, "Copy to present"};

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_composedTexture.GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
        commandList->ResourceBarrier(1, &barrier);

        m_app.CopyToPresentImage(m_composedTexture.GetResource());
    }


    m_app.EndFrame();
}

void Renderer::Update(const FrameInfo &frameInfo) {
    const uint32_t frameIndex = frameInfo.frameIndex;

    const DirectX::XMFLOAT4X4 viewFloat = m_camera.GetViewMatrix();
    const DirectX::XMFLOAT4X4 projFloat = m_camera.GetProjectionMatrix();
    const DirectX::XMMATRIX   view      = DirectX::XMLoadFloat4x4(&viewFloat);
    const DirectX::XMMATRIX   proj      = DirectX::XMLoadFloat4x4(&projFloat);
    const DirectX::XMMATRIX   viewProj  = DirectX::XMMatrixMultiply(view, proj);
    DirectX::XMStoreFloat4x4(&m_globalUniforms[frameIndex].viewProj, viewProj);

    DirectX::XMMATRIX viewNoTrans = view;
    viewNoTrans.r[3]              = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    DirectX::XMStoreFloat4x4(&m_skyboxUniforms.viewProj, DirectX::XMMatrixMultiply(viewNoTrans, proj));

    const DirectX::XMFLOAT3 eye  = m_camera.GetLocation();
    m_deferredUniforms.cameraPos = {eye.x, eye.y, eye.z};
}
