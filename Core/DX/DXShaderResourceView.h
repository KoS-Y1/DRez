//
// Created by y1 on 2026-05-10.
//

#pragma once

#include <cstdint>

#include <directx/d3d12.h>

class DXApp;

class DXShaderResourceView {
public:
    DXShaderResourceView() = default;
    DXShaderResourceView(DXApp &app, ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC &desc);

    DXShaderResourceView(const DXShaderResourceView &)            = delete;
    DXShaderResourceView &operator=(const DXShaderResourceView &) = delete;
    DXShaderResourceView(DXShaderResourceView &&)                 = default;
    DXShaderResourceView &operator=(DXShaderResourceView &&)      = default;

    ~DXShaderResourceView() = default;

    [[nodiscard]] uint32_t GetIndex() const { return m_index; }

private:
    uint32_t m_index{};
};
