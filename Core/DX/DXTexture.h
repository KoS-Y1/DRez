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
    );

    DXTexture(const DXTexture &)            = delete;
    DXTexture &operator=(const DXTexture &) = delete;
    DXTexture(DXTexture &&)                 = default;
    DXTexture &operator=(DXTexture &&)      = default;

    ~DXTexture() = default;

private:
    std::string m_name{};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture{};
    D3D12_STATIC_SAMPLER_DESC              m_sampler{};
};
