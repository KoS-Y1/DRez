//
// Created by y1 on 2026-05-07.
//

#include "Mesh.h"

void Mesh::CreateIndexView() {
    auto GetIndexFormat = [](int32_t indexType) -> DXGI_FORMAT {
        if (indexType == 0) {
            return DXGI_FORMAT_R16_UINT;
        }
        return DXGI_FORMAT_R32_UINT;
    };

    const auto &idx   = m_mesh.triangleMesh.indices;
    m_indexBufferView = D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = m_gltfBufferAddress + idx.offset,
        .SizeInBytes    = idx.count * idx.byteStride,
        .Format         = GetIndexFormat(m_mesh.indexType),
    };
}