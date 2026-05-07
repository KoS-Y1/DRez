//
// Created by y1 on 2026-05-07.
//

#include "Mesh.h"

Mesh::Mesh(const shader_io::MeshInfo &mesh, uint32_t gltfHandle, std::string name)
    : m_mesh(mesh)
    , m_gltfHandle(gltfHandle)
    , m_name(std::move(name)) {
    auto GetIndexFormat = [](int32_t indexType) -> DXGI_FORMAT {
        if (indexType == 0) {
            return DXGI_FORMAT_R16_UINT;
        }
        return DXGI_FORMAT_R32_UINT;
    };

    const auto &idx   = m_mesh.triangleMesh.indices;
    m_indexBufferView = D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(m_mesh.buffer) + idx.offset,
        .SizeInBytes    = idx.count * idx.byteStride,
        .Format         = GetIndexFormat(m_mesh.indexType),
    };
}
