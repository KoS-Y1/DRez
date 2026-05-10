//
// Created by y1 on 2026-05-10.
//

#pragma once

#include <cstdint>

#include <directx/d3d12.h>

class DXApp;

class DXUnorderedAccessView {
public:
    DXUnorderedAccessView() = default;
    DXUnorderedAccessView(DXApp &app, ID3D12Resource *resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc);

    DXUnorderedAccessView(const DXUnorderedAccessView &)            = delete;
    DXUnorderedAccessView &operator=(const DXUnorderedAccessView &) = delete;
    DXUnorderedAccessView(DXUnorderedAccessView &&)                 = default;
    DXUnorderedAccessView &operator=(DXUnorderedAccessView &&)      = default;

    ~DXUnorderedAccessView() = default;

    [[nodiscard]] uint32_t GetIndex() const { return m_index; }

private:
    uint32_t m_index{};
};
