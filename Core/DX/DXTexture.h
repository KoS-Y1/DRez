//
// Created by y1 on 2026-04-29.
//

#pragma once

#include <string>

#include <directx/d3d12.h>
#include <wrl/client.h>

class DXApp;

class DXTexture {
public:
    DXTexture() = default;
    DXTexture(
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
    );

    DXTexture(const DXTexture &)            = delete;
    DXTexture &operator=(const DXTexture &) = delete;
    DXTexture(DXTexture &&)                 = default;
    DXTexture &operator=(DXTexture &&)      = default;

    ~DXTexture() = default;

    [[nodiscard]] std::string_view GetName() const { return m_name; }

    [[nodiscard]] ID3D12Resource *GetResource() const { return m_texture.Get(); }

    [[nodiscard]] DXGI_FORMAT GetFormat() const { return m_format; }

    [[nodiscard]] const D3D12_SAMPLER_DESC &GetSampler() const { return m_sampler; }

    [[nodiscard]] uint32_t GetBindlessIndex() const { return m_bindlessIndex; }

private:
    std::string m_name{};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture{};
    DXGI_FORMAT                            m_format{DXGI_FORMAT_UNKNOWN};
    D3D12_SAMPLER_DESC                     m_sampler{};
    uint32_t                               m_bindlessIndex{};
};
