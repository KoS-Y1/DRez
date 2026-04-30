//
// Created by y1 on 2025-09-22.
//

#pragma once

#include <directx/d3d12.h>
#include <fastgltf/core.hpp>

#include "resources_io.slang"

namespace drez::file_system {

std::optional<fastgltf::Asset>     LoadGltf(std::string_view path);
std::optional<shader_io::MeshInfo> LoadMesh(const fastgltf::Asset &asset, const fastgltf::Mesh &mesh, D3D12_GPU_VIRTUAL_ADDRESS bufferAddress);
std::optional<std::tuple<int, int, unsigned char *>> LoadImage(std::string_view path, const fastgltf::Asset &asset, const fastgltf::Image &image);
std::optional<std::tuple<int, int, unsigned char *>> LoadImage(const std::string &path); // Load pure image
D3D12_SAMPLER_DESC                                   LoadSampler(const fastgltf::Sampler &sampler);
shader_io::MaterialInfo                              LoadMaterial(const fastgltf::Material &material);
} // namespace drez::file_system