//
// Created by y1 on 2026-04-30.
//

#include "Renderer.h"

#include <algorithm>

#include <directx/d3dx12_barriers.h>
#include <directx/d3dx12_resource_helpers.h>

#include "DXUtils.h"
#include "Mesh.h"
#include "ResourceManager.h"

Renderer::Renderer(DXApp &app, const Camera &camera)
    : m_app(app)
    , m_camera(camera)
    , m_width(app.GetWindowWidth())
    , m_height(app.GetWindowHeight())
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height)})
    , m_rect(CD3DX12_RECT{0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height)}) {
    // Load pipelines
    {
        m_gbuffer  = m_app.CreateGraphicsPipeline("../Assets/Shaders/gbuffer.json");
        m_deferred = m_app.CreateComputePipeline("../Assets/Shaders/deferred.json");
        m_blit     = m_app.CreateComputePipeline("../Assets/Shaders/blit.json");
    }

    // Load instances
    {
        // TODO: test for now
        {
            uint32_t            meshHandle     = ResourceManager::GetInstance().GetMeshHandle("Chessboard");
            uint32_t            materialHandle = ResourceManager::GetInstance().GetMaterialHandle("Chessboard");
            DirectX::XMFLOAT4X4 identity =
                DirectX::XMFLOAT4X4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            m_instances.emplace_back(meshHandle, materialHandle);
            m_instanceTransforms.push_back(std::move(identity));
        }
        {
            uint32_t            meshHandle     = ResourceManager::GetInstance().GetMeshHandle("King_Shared");
            uint32_t            materialHandle = ResourceManager::GetInstance().GetMaterialHandle("King_Black");
            DirectX::XMFLOAT4X4 identity =
                DirectX::XMFLOAT4X4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            m_instances.emplace_back(meshHandle, materialHandle);
            m_instanceTransforms.push_back(std::move(identity));
        }
    }

    // Instance buffer
    {
        const uint64_t size = std::span{m_instances}.size_bytes();

        m_instanceBuffer = app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, size, "instance_buffer");
        DXBuffer stagingBuffer =
            app.CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, size, "staging_buffer_instances");

        D3D12_SUBRESOURCE_DATA data{};
        data.pData      = m_instances.data();
        data.RowPitch   = size;
        data.SlicePitch = size;

        m_app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_instanceBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_instanceBuffer.GetBuffer(),
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
                                        .NumElements         = static_cast<uint32_t>(m_instances.size()),
                                        .StructureByteStride = sizeof(shader_io::InstanceInfo),
                                        .Flags               = D3D12_BUFFER_SRV_FLAG_NONE,
                                        },
        };
        m_instanceBufferSrv = m_app.CreateDXShaderResourceView(m_instanceBuffer.GetBuffer(), desc);
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
        constexpr DXGI_FORMAT kGbufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
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
                drez::dx_utils::GetFormatSize(kGbufferFormat),
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                D3D12_HEAP_FLAG_NONE,
                shader_io::SamplerType::Nearest,
                nullptr,
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
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                    texture.GetResource(),
                    D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
                ));
            }
            commandList->ResourceBarrier(barriers.size(), barriers.data());
        });

        // Depth
        m_depthTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_D32_FLOAT,
            drez::dx_utils::GetFormatSize(DXGI_FORMAT_D32_FLOAT),
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Nearest,
            nullptr,
            "depth_texture"
        );
        m_app.CreateDepthStencilView(m_depthTexture.GetResource(), dsvOffset);
        m_depthTextureDsvOffset = dsvOffset++;

        m_app.ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
            auto barrier =
                CD3DX12_RESOURCE_BARRIER::Transition(m_depthTexture.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            commandList->ResourceBarrier(1, &barrier);
        });

        // Deferred composited HDR target (compute write -> blit read)
        m_deferredTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            drez::dx_utils::GetFormatSize(DXGI_FORMAT_R16G16B16A16_FLOAT),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Linear,
            nullptr,
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
            drez::dx_utils::GetFormatSize(DXGI_FORMAT_R8G8B8A8_UNORM),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_HEAP_FLAG_NONE,
            shader_io::SamplerType::Nearest,
            nullptr,
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
        info.shadowMapIndex  = 0;

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
        for (auto &uniforms: m_globalUniforms) {
            uniforms.instancesIndex = m_instanceBufferSrv.GetIndex();
            uniforms.meshesIndex    = ResourceManager::GetInstance().GetMeshesBindlessIndex();
            uniforms.materialsIndex = ResourceManager::GetInstance().GetMaterialsBindlessIndex();
        }

        m_deferredUniforms.deferredInfoIndex = m_deferredInfoBufferSrv.GetIndex();
        m_deferredUniforms.dstIndex          = m_deferredTextureUav.GetIndex();

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

    // Gbuffer pass
    {
        commandList->SetGraphicsRootSignature(m_gbuffer.GetRootSignature());
        commandList->SetPipelineState(m_gbuffer.GetPipelineState());
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_rect);

        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_gbufferTextures.size());
        for (const auto &texture: m_gbufferTextures) {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                texture.GetResource(),
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            ));
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

        for (uint32_t i = 0; i < m_instances.size(); ++i) {
            m_globalUniforms[frameIndex].transform    = m_instanceTransforms[i];
            // TODO: inverse transpose to get the actual uniforms
            m_globalUniforms[frameIndex].normalMatrix = m_instanceTransforms[i];
            commandList->SetGraphicsRoot32BitConstants(0, sizeof(shader_io::GlobalUniforms) / sizeof(uint32_t), &m_globalUniforms[frameIndex], 0);

            const Mesh                   &mesh         = ResourceManager::GetInstance().GetMesh(m_instances[i].meshHandle);
            const shader_io::TriangleMesh triangleMesh = mesh.GetMesh().triangleMesh;
            commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());

            commandList->DrawIndexedInstanced(triangleMesh.indices.count, 1, 0, 0, i);
        }
    }

    // Deferred pass
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_gbufferTextures.size() + 1);
        for (const auto &texture: m_gbufferTextures) {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                texture.GetResource(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            ));
        }
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            m_deferredTexture.GetResource(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        ));
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

    // Blit pass
    {
        commandList->SetComputeRootSignature(m_blit.GetRootSignature());
        commandList->SetPipelineState(m_blit.GetPipelineState());
        commandList->SetComputeRoot32BitConstants(0, sizeof(shader_io::BlitUniforms) / sizeof(uint32_t), &m_blitUniforms, 0);

        const std::vector<CD3DX12_RESOURCE_BARRIER> barriers{
            CD3DX12_RESOURCE_BARRIER::Transition(
                m_deferredTexture.GetResource(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
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

    const DirectX::XMFLOAT3 eye = m_camera.GetLocation();
    m_deferredUniforms.cameraPos = {eye.x, eye.y, eye.z};
}
