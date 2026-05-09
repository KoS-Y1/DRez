//
// Created by y1 on 2026-05-07.
//

#pragma once

#include <string>

#include <directx/d3d12.h>

#include "resources_io.slang"

class DXBuffer;

class Mesh {
public:
    Mesh() = default;

    Mesh(const shader_io::MeshInfo &mesh,  std::string name)
        : m_mesh(mesh)
        , m_name(std::move(name)) {};

    Mesh(const Mesh &)            = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&)                 = default;
    Mesh &operator=(Mesh &&)      = default;

    ~Mesh() = default;

    void CreateIndexView();

    [[nodiscard]] std::string_view GetName() const { return m_name; }

    [[nodiscard]] const shader_io::MeshInfo &GetMesh() const { return m_mesh; }

    [[nodiscard]] const D3D12_INDEX_BUFFER_VIEW &GetIndexBufferView() const { return m_indexBufferView; }

private:
    shader_io::MeshInfo     m_mesh{};
    std::string             m_name{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
};
