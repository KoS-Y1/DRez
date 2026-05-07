//
// Created by y1 on 2026-04-28.
//

#include "DXBuffer.h"

#include <directx/d3dx12.h>

#include "DXApp.h"
#include "Debug.h"

DXBuffer::DXBuffer(const DXApp &app, D3D12_HEAP_TYPE heapType, D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES states, uint64_t size, std::string name)
    : m_name(std::move(name)) {
    CD3DX12_HEAP_PROPERTIES heapProperties{heapType};
    auto                    desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    HRESULT result               = app.GetDevice()->CreateCommittedResource(&heapProperties, flags, &desc, states, nullptr, IID_PPV_ARGS(&m_buffer));
    DebugCheckCritical(SUCCEEDED(result), "Failed to create buffer {}, error {:x}", m_name, result);

    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
        // Whole size
        D3D12_RANGE range{0, 0};

        result = m_buffer->Map(0, &range, &m_mappedData);
        DebugCheckCritical(SUCCEEDED(result), "Failed to map buffer {}, error {:b}", m_name, result);
    }
}

DXBuffer::~DXBuffer() {
    m_buffer->Unmap(0, nullptr);
}

void DXBuffer::Upload(uint64_t size, const void *data) {
    memcpy(m_mappedData, data, size);
}