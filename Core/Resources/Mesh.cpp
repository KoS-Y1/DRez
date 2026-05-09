//
// Created by y1 on 2026-05-07.
//

#include "Mesh.h"

#include "DXBuffer.h"
#include "ResourceManager.h"

void Mesh::CreateIndexView() {
    auto GetIndexFormat = [](int32_t indexType) -> DXGI_FORMAT {
        if (indexType == 0) {
            return DXGI_FORMAT_R16_UINT;
        }
        return DXGI_FORMAT_R32_UINT;
    };

    const auto     &idx        = m_mesh.triangleMesh.indices;
    const DXBuffer &gltfBuffer = ResourceManager::GetInstance().GetGltfBuffer(m_mesh.gltfHandle);
    m_indexBufferView          = D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = gltfBuffer.GetGPUVirtualAddress() + idx.offset,
        .SizeInBytes    = idx.count * idx.byteStride,
        .Format         = GetIndexFormat(m_mesh.indexType),
    };
}