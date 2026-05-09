//
// Created by y1 on 2026-05-07.
//

#include "Mesh.h"

void Mesh::CreateIndexView() {
    const auto &idx   = m_mesh.triangleMesh.indices;
    m_indexBufferView = D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = m_gltfBufferAddress + idx.offset,
        .SizeInBytes    = idx.count * idx.byteStride,
        .Format         = static_cast<DXGI_FORMAT>(m_mesh.indexType),
    };
}