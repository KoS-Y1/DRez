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

    ~DXBuffer();

    void Upload(uint64_t size, const void *data);
    void UploadRows(uint64_t dstOffset, uint32_t numRows, uint64_t srcRowPitch, uint64_t dstRowPitch, const void *data);

    [[nodiscard]] ID3D12Resource *GetBuffer() const { return m_buffer.Get(); }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return m_buffer->GetGPUVirtualAddress(); }

    [[nodiscard]] std::string_view GetName() const { return m_name; }

    [[nodiscard]] uint64_t GetBufferSize() const { return m_bufferSize; }

    [[nodiscard]] const void *GetMappedData() const { return m_mappedData; }

private:
    std::string                            m_name{};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;
    uint64_t                               m_bufferSize{};

    void *m_mappedData{};
};
