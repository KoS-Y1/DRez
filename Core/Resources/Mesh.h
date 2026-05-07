//
// Created by y1 on 2026-05-07.
//

#pragma once

#include <string>

#include <directx/d3d12.h>

#include "resources_io.slang"

class Mesh {
public:
    Mesh() = default;

    Mesh(const shader_io::MeshInfo &mesh, uint32_t gltfHandle, std::string name);


    Mesh(const Mesh &)            = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&)                 = default;
    Mesh &operator=(Mesh &&)      = default;

    ~Mesh() = default;

    [[nodiscard]] std::string_view GetName() const { return m_name; }

    [[nodiscard]] const shader_io::MeshInfo &GetMesh() const { return m_mesh; }

    [[nodiscard]] uint32_t GetGltfHandle() const { return m_gltfHandle; }

    [[nodiscard]] const D3D12_INDEX_BUFFER_VIEW &GetIndexBufferView() const { return m_indexBufferView; }

private:
    shader_io::MeshInfo m_mesh{};
    uint32_t            m_gltfHandle{};
    std::string         m_name{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
};
