//
// Created by y1 on 2026-04-29.
//

#include "DXTexture.h"

#include <directx/d3dx12_core.h>

#include "DXApp.h"
#include "Debug.h"

#include <directx/d3dx12_resource_helpers.h>

DXTexture::DXTexture(
    DXApp                    &app,
    uint64_t                  width,
    uint32_t                  height,
    DXGI_FORMAT               format,
    uint32_t                  formatSize,
    D3D12_RESOURCE_FLAGS      resourceFlags,
    D3D12_HEAP_FLAGS          heapFlags,
    D3D12_STATIC_SAMPLER_DESC sampler,
    const void               *data,
    std::string               name
)
    : m_name(std::move(name))
    , m_sampler(std::move(sampler)) {
    D3D12_RESOURCE_DESC desc{
        .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width            = width,
        .Height           = height,
        .DepthOrArraySize = 1,
        .MipLevels        = 1,
        .Format           = format,
        .SampleDesc       = {.Count = 1, .Quality = 0},
        .Flags            = resourceFlags
    };
    CD3DX12_HEAP_PROPERTIES heapProperties{D3D12_HEAP_TYPE_DEFAULT};
    HRESULT                 result =
        app.GetDevice()->CreateCommittedResource(&heapProperties, heapFlags, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_texture));
    DebugCheckCritical(SUCCEEDED(result), "Failed to create texture resource {}", m_name);

    if (data) {
        D3D12_SUBRESOURCE_DATA textureData{
            .pData      = data,
            .RowPitch   = static_cast<int64_t>(formatSize * width),
            .SlicePitch = static_cast<int64_t>(formatSize * width * height),
        };

        DXBuffer stagingBuffer = app.CreaetBuffer(
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_HEAP_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            width * height * formatSize,
            "staging buffer" + name
        );

        app.ImmediateSubmit([&](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources(commandList, m_texture.Get(), stagingBuffer.GetBuffer(), 0, 0, 1, &textureData);
        });
    }
}