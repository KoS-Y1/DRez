//
// Created by y1 on 2026-04-28.
//

#pragma once

#include <string>

#include <directx/d3d12.h>
#include <wrl/client.h>

class DXApp;

class DXBuffer {
public:
    DXBuffer() = default;
    DXBuffer(const DXApp &app, D3D12_HEAP_TYPE heapType, D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES states, uint64_t size, std::string name);

    DXBuffer(const DXBuffer &)            = delete;
    DXBuffer &operator=(const DXBuffer &) = delete;

    DXBuffer(DXBuffer &&)            = default;
    DXBuffer &operator=(DXBuffer &&) = default;

    ~DXBuffer() = default;

    [[nodiscard]] ID3D12Resource *GetBuffer() const { return m_buffer.Get(); }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_buffer->GetGPUVirtualAddress(); }

private:
    std::string                            m_name{};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;
};
