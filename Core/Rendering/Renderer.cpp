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

Renderer::Renderer(DXApp &app)
    : m_app(app)
    , m_width(app.GetWindowWidth())
    , m_height(app.GetWindowHeight())
    , m_viewport(CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height)})
    , m_rect(CD3DX12_RECT{0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height)}) {
    // Load pipelines
    {
        // TODO: test for now
        m_forward = m_app.CreateGraphicsPipeline("../Assets/Shaders/forward.json");
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
            uint32_t            materialHandle = ResourceManager::GetInstance().GetMaterialHandle("King_Shared");
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
    }

    // Global scene info and global uniform
    {
        for (uint32_t i = 0; i < m_globalSceneInfos.size(); ++i) {
            auto &info     = m_globalSceneInfos[i];
            info.instances = reinterpret_cast<shader_io::InstanceInfo *>(m_instanceBuffer.GetGPUVirtualAddress());
            info.meshes    = reinterpret_cast<shader_io::MeshInfo *>(ResourceManager::GetInstance().GetMeshBufferAddress());
            info.materials = reinterpret_cast<shader_io::MaterialInfo *>(ResourceManager::GetInstance().GetMaterialBufferAddress());

            m_sceneBuffers[i] = m_app.CreateBuffer(
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_HEAP_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                sizeof(shader_io::GlobalSceneInfo),
                "scene_buffer_" + std::to_string(i++)
            );

            m_globalUniforms[i].sceneInfo = reinterpret_cast<shader_io::GlobalSceneInfo *>(m_sceneBuffers[i].GetGPUVirtualAddress());
        }
    }


    // Render resources
    {
        int32_t rtvOffset{0};

        const D3D12_SAMPLER_DESC defaultSampler{
            .Filter         = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .MipLODBias     = 0.0f,
            .MaxAnisotropy  = 1,
            .ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
            .MinLOD         = 0.0f,
            .MaxLOD         = D3D12_FLOAT32_MAX,
        };

        m_finalTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            drez::dx_utils::GetFormatSize(DXGI_FORMAT_R16G16B16A16_FLOAT),
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_HEAP_FLAG_NONE,
            defaultSampler,
            nullptr,
            "final_texture"
        );
        m_app.CreateRenderTargetView(m_finalTexture.GetResource(), rtvOffset);
        m_finalTextureRtvOffset = rtvOffset++;
    }

    // TODO: bindless descriptor for textures
    {
    }
}

void Renderer::Render() {
    FrameInfo                  frameInfo   = m_app.BeginFrame();
    ID3D12GraphicsCommandList *commandList = frameInfo.commandList;
    uint32_t                   frameIndex  = frameInfo.frameIndex;

    Update(frameInfo);

    // Forward pass
    {
        commandList->SetGraphicsRootSignature(m_forward.GetRootSignature());
        commandList->SetPipelineState(m_forward.GetPipelineState());
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_rect);

        auto barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(m_finalTexture.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_app.GetRenderTargetView(m_finalTextureRtvOffset);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        static constexpr float rtvClearColor[]{0.0f, 0.0f, 0.0f, 1.0f};
        commandList->ClearRenderTargetView(rtvHandle, rtvClearColor, 0, nullptr);

        commandList->IASetPrimitiveTopology(m_forward.GetPrimitiveTopology());

        for (uint32_t i = 0; i < m_instances.size(); ++i) {
            m_globalUniforms[frameIndex].transform    = m_instanceTransforms[i];
            // TODO: inverse transpose to get the actual uniforms
            m_globalUniforms[frameIndex].normalMatrix = m_instanceTransforms[i];

            const Mesh                   &mesh         = ResourceManager::GetInstance().GetMesh(m_instances[i].meshHandle);
            const shader_io::TriangleMesh triangleMesh = mesh.GetMesh().triangleMesh;
            commandList->IASetIndexBuffer(&mesh.GetIndexBufferView());

            commandList->DrawIndexedInstanced(triangleMesh.indices.count, 1, 0, 0, i);
        }
    }

    // Copy to present
    {
        auto barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(m_finalTexture.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->ResourceBarrier(1, &barrier);

        m_app.CopyToPresentImage(m_finalTexture.GetResource());
    }


    m_app.EndFrame();
}

void Renderer::Update(const FrameInfo &frameInfo) {
    const uint32_t frameIndex = frameInfo.frameIndex;

    // Update global scene info
    {
        // TODO: load with actual camera
        DirectX::FXMMATRIX viewProj = DirectX::XMMatrixMultiply(DirectX::XMMatrixIdentity(), DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&m_globalSceneInfos[frameIndex].viewProj, viewProj);

        m_sceneBuffers[frameIndex].Upload(sizeof(shader_io::GlobalSceneInfo), &m_globalSceneInfos);
    }
}