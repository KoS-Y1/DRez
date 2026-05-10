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

        m_instanceBufferIndex = m_app.AllocateBindlessIndex();
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
        m_app.CreateShaderResourceView(m_instanceBuffer.GetBuffer(), m_instanceBufferIndex, desc);
    }

    // Global uniforms
    {
        for (auto &uniforms: m_globalUniforms) {
            uniforms.instancesIndex = m_instanceBufferIndex;
            uniforms.meshesIndex    = ResourceManager::GetInstance().GetMeshesBindlessIndex();
            uniforms.materialsIndex = ResourceManager::GetInstance().GetMaterialsBindlessIndex();
        }
    }


    // Render resources
    {
        int32_t rtvOffset{0};
        int32_t dsvOffset{0};

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
            DXGI_FORMAT_R8G8B8A8_UNORM,
            drez::dx_utils::GetFormatSize(DXGI_FORMAT_R8G8B8A8_UNORM),
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_HEAP_FLAG_NONE,
            defaultSampler,
            nullptr,
            "final_texture"
        );
        m_app.CreateRenderTargetView(m_finalTexture.GetResource(), rtvOffset);
        m_finalTextureRtvOffset = rtvOffset++;

        // Depth texture
        m_depthTexture = m_app.CreateTexture(
            m_width,
            m_height,
            DXGI_FORMAT_D32_FLOAT,
            drez::dx_utils::GetFormatSize(DXGI_FORMAT_D32_FLOAT),
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            D3D12_HEAP_FLAG_NONE,
            defaultSampler,
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
    }

        // std::ranges::for_each(m_globaluniforms, [](shader_io::globaluniforms &uniforms) {
        //     uniforms.color = directx::xmfloat4(1.0f, 1.0f, 1.0f, 1.0f);
        // });
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

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_app.GetRenderTargetViewHandle(m_finalTextureRtvOffset);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_app.GetDepthStencilViewHandle(m_depthTextureDsvOffset);
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        static constexpr float rtvClearColor[]{0.0f, 0.0f, 0.0f, 1.0f};
        commandList->ClearRenderTargetView(rtvHandle, rtvClearColor, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        commandList->IASetPrimitiveTopology(m_forward.GetPrimitiveTopology());

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

    // Update global uniforms
    {
        const DirectX::XMFLOAT4X4 viewFloat = m_camera.GetViewMatrix();
        const DirectX::XMFLOAT4X4 projFloat = m_camera.GetProjectionMatrix();
        const DirectX::XMMATRIX   view      = DirectX::XMLoadFloat4x4(&viewFloat);
        const DirectX::XMMATRIX   proj      = DirectX::XMLoadFloat4x4(&projFloat);
        const DirectX::XMMATRIX   viewProj  = DirectX::XMMatrixMultiply(view, proj);
        DirectX::XMStoreFloat4x4(&m_globalUniforms[frameIndex].viewProj, viewProj);
    }
}