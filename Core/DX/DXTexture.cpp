//
// Created by y1 on 2026-04-29.
//

#include "DXTexture.h"

#include <directx/d3dx12.h>

#include "DXApp.h"
#include "DXDebug.h"
#include "DXUtils.h"
#include "Debug.h"

DXTexture::DXTexture(
    const DXApp           &app,
    uint64_t               width,
    uint32_t               height,
    DXGI_FORMAT            format,
    D3D12_RESOURCE_FLAGS   resourceFlags,
    D3D12_HEAP_FLAGS       heapFlags,
    shader_io::SamplerType samplerType,
    std::string            name,
    DXGI_FORMAT            clearFormat,
    uint16_t               mipLevels
)
    : m_name(std::move(name))
    , m_width(width)
    , m_height(height)
    , m_mipLevels(mipLevels)
    , m_format(format)
    , m_samplerType(samplerType) {
    const DXGI_FORMAT effectiveClearFormat = (clearFormat == DXGI_FORMAT_UNKNOWN) ? m_format : clearFormat;
    const bool        isDepthStencil       = drez::dx_utils::IsDepthStencilFormat(effectiveClearFormat);

    {
        D3D12_RESOURCE_DESC desc{
            .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width            = width,
            .Height           = height,
            .DepthOrArraySize = 1,
            .MipLevels        = mipLevels,
            .Format           = m_format,
            .SampleDesc       = {.Count = 1, .Quality = 0},
            .Flags            = resourceFlags
        };
        CD3DX12_HEAP_PROPERTIES heapProperties{D3D12_HEAP_TYPE_DEFAULT};

        if (resourceFlags == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET || resourceFlags == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
            D3D12_CLEAR_VALUE clearValue{.Format = effectiveClearFormat};
            if (isDepthStencil) {
                clearValue.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{.Depth = 1.0f, .Stencil = 0};
            } else {
                clearValue.Color[0] = 0.0f;
                clearValue.Color[1] = 0.0f;
                clearValue.Color[2] = 0.0f;
                clearValue.Color[3] = 0.0f;
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

        drez::dx::debug::SetObjectName(m_texture.Get(), m_name);
    }
}

void DXTexture::Upload(DXApp &app, const void *data) const {
    const uint32_t formatSize    = drez::dx_utils::GetFormatSize(m_format);
    DXBuffer       stagingBuffer = app.CreateBuffer(
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_HEAP_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        m_width * m_height * formatSize,
        "staging_buffer" + m_name
    );

    D3D12_SUBRESOURCE_DATA textureData;
    textureData.pData      = data;
    textureData.RowPitch   = formatSize * static_cast<uint32_t>(m_width);
    textureData.SlicePitch = textureData.RowPitch * m_height;

    app.ImmediateSubmit([&](ID3D12GraphicsCommandList *commandList) {
        UpdateSubresources(commandList, m_texture.Get(), stagingBuffer.GetBuffer(), 0, 0, 1, &textureData);

        auto barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);
    });
}