//
// Created by y1 on 2026-04-29.
//

#include "DXTexture.h"

#include <directx/d3dx12.h>

#include "DXApp.h"
#include "DXUtils.h"
#include "Debug.h"

DXTexture::DXTexture(
    DXApp               &app,
    uint64_t             width,
    uint32_t             height,
    DXGI_FORMAT          format,
    uint32_t             formatSize,
    D3D12_RESOURCE_FLAGS resourceFlags,
    D3D12_HEAP_FLAGS     heapFlags,
    D3D12_SAMPLER_DESC   sampler,
    const void          *data,
    std::string          name
)
    : m_name(std::move(name))
    , m_format(format)
    , m_sampler(std::move(sampler))
    , m_bindlessIndex(app.AllocateBindlessIndex()) {
    bool isDepthStencil = drez::dx_utils::IsDepthStencilFormat(m_format);

    {
        D3D12_RESOURCE_DESC desc{
            .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width            = width,
            .Height           = height,
            .DepthOrArraySize = 1,
            .MipLevels        = 1,
            .Format           = m_format,
            .SampleDesc       = {.Count = 1, .Quality = 0},
            .Flags            = resourceFlags
        };
        CD3DX12_HEAP_PROPERTIES heapProperties{D3D12_HEAP_TYPE_DEFAULT};


        if (data) {
            HRESULT result =
                app.GetDevice()
                    ->CreateCommittedResource(&heapProperties, heapFlags, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_texture));
            DebugCheckCritical(SUCCEEDED(result), "Failed to create texture resource {}", m_name);

            D3D12_SUBRESOURCE_DATA textureData{
                .pData      = data,
                .RowPitch   = static_cast<int64_t>(formatSize * width),
                .SlicePitch = static_cast<int64_t>(formatSize * width * height),
            };

            DXBuffer stagingBuffer = app.CreateBuffer(
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_HEAP_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                width * height * formatSize,
                "staging_buffer" + name
            );

            app.ImmediateSubmit([&](ID3D12GraphicsCommandList *commandList) {
                UpdateSubresources(commandList, m_texture.Get(), stagingBuffer.GetBuffer(), 0, 0, 1, &textureData);

                auto barrier =
                    CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                commandList->ResourceBarrier(1, &barrier);
            });
        } else if (resourceFlags == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET || resourceFlags == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
            D3D12_CLEAR_VALUE clearValue{.Format = m_format};
            if (isDepthStencil) {
                clearValue.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{.Depth = 1.0f, .Stencil = 0};
            } else {
                clearValue.Color[0] = 0.0f;
                clearValue.Color[1] = 0.0f;
                clearValue.Color[2] = 0.0f;
                clearValue.Color[3] = 1.0f;
            }
            HRESULT result =
                app.GetDevice()
                    ->CreateCommittedResource(&heapProperties, heapFlags, &desc, D3D12_RESOURCE_STATE_COMMON, &clearValue, IID_PPV_ARGS(&m_texture));
            DebugCheckCritical(SUCCEEDED(result), "Failed to create texture resource {}", m_name);
        } else {
            HRESULT result =
                app.GetDevice()
                    ->CreateCommittedResource(&heapProperties, heapFlags, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_texture));
            DebugCheckCritical(SUCCEEDED(result), "Failed to create texture resource {}", m_name);
        }
    }

    {
        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = isDepthStencil ? DXGI_FORMAT_R32_FLOAT : m_format,
            .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D               = {.MipLevels = 1},
        };

        app.CreateShaderResourceView(m_texture.Get(), m_bindlessIndex, desc);
    }
}