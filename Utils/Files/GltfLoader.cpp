//
// Created by y1 on 2025-09-22.
//

#include "GltfLoader.h"

#include <fastgltf/core.hpp>
#include <stb_image.h>

#include "Debug.h"

#include <ranges>

namespace drez::file_system {
std::optional<fastgltf::Asset> LoadGltf(std::string_view path) {
    fastgltf::Expected<fastgltf::GltfDataBuffer> dataBuffer = fastgltf::GltfDataBuffer::FromPath(path);
    if (dataBuffer.error() != fastgltf::Error::None) {
        DebugWarning("Failed to load gltf buffer from {}", path.data());
        return std::nullopt;
    }

    constexpr fastgltf::Options options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Parser parser{};

    std::filesystem::path               filePath{path};
    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(dataBuffer.get(), filePath.parent_path(), options);

    if (asset.error() != fastgltf::Error::None) {
        return std::nullopt;
    }

    return std::move(asset.get());
}

std::optional<shader_io::MeshInfo> LoadMesh(const fastgltf::Asset &asset, const fastgltf::Mesh &mesh, uint32_t gltfBufferIndex) {
    // Lambda for element byte size calculation
    auto getElementByteSize = [](fastgltf::ComponentType type) -> uint32_t {
        return type == fastgltf::ComponentType::UnsignedShort ? 2U :
               type == fastgltf::ComponentType::UnsignedInt   ? 4U :
               type == fastgltf::ComponentType::Float         ? 4U :
                                                                0U;
    };

    // Lambda for type size calculation
    auto getTypeSize = [](fastgltf::AccessorType type) -> uint32_t {
        return type == fastgltf::AccessorType::Vec2 ? 2U :
               type == fastgltf::AccessorType::Vec3 ? 3U :
               type == fastgltf::AccessorType::Vec4 ? 4U :
               type == fastgltf::AccessorType::Mat2 ? 4U * 2U :
               type == fastgltf::AccessorType::Mat3 ? 4U * 3U :
               type == fastgltf::AccessorType::Mat4 ? 4U * 4U :
                                                      0U;
    };

    // Lambda for extracting attributes, like positions, normals, etc
    auto extractAttribute =
        [&asset, &getTypeSize, &getElementByteSize](std::string_view name, const fastgltf::Primitive &primitive, shader_io::BufferView &bufferView) {
            if (primitive.findAttribute(name) == primitive.attributes.cend()) {
                bufferView.offset = -1;
                return;
            }

            const fastgltf::Accessor &acc = asset.accessors[primitive.findAttribute(name)->accessorIndex];
            DebugCheckCritical(acc.bufferViewIndex.has_value(), "Accessor does not have buffer view");
            const fastgltf::BufferView &bv = asset.bufferViews[acc.bufferViewIndex.value()];
            DebugCheckCritical(acc.componentType == fastgltf::ComponentType::Float, "Accessor type must be float");
            bufferView = {
                .offset     = static_cast<uint32_t>(bv.byteOffset + acc.byteOffset),
                .count      = static_cast<uint32_t>(acc.count),
                .byteStride = bv.byteStride.has_value() ? static_cast<uint32_t>(bv.byteStride.value()) :
                                                          getTypeSize(acc.type) * getElementByteSize(acc.componentType)
            };
        };

    if (mesh.primitives.empty()) {
        DebugError("{} has no primitives", mesh.name);
        return std::nullopt;
    }

    const fastgltf::Primitive &primitive = mesh.primitives.front();

    if (!primitive.indicesAccessor.has_value()) {
        DebugWarning("Primitive indices accessor not found");
        return std::nullopt;
    }
    const fastgltf::Accessor &accessor = asset.accessors[primitive.indicesAccessor.value()];
    if (!accessor.bufferViewIndex.has_value()) {
        DebugWarning("Accessor buffer view index not found");
        return std::nullopt;
    }
    const fastgltf::BufferView &bufferView = asset.bufferViews[accessor.bufferViewIndex.value()];

    shader_io::MeshInfo meshInfo{
        .gltfBufferIndex = gltfBufferIndex,
        .indexType       = accessor.componentType == fastgltf::ComponentType::UnsignedShort ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
    };
    meshInfo.triangleMesh.indices = {
        .offset = static_cast<uint32_t>(bufferView.byteOffset + accessor.byteOffset),
        .count  = static_cast<uint32_t>(accessor.count),
        .byteStride =
            static_cast<uint32_t>(bufferView.byteStride.has_value() ? bufferView.byteStride.value() : getElementByteSize(accessor.componentType))
    };

    extractAttribute("POSITION", primitive, meshInfo.triangleMesh.positions);
    extractAttribute("NORMAL", primitive, meshInfo.triangleMesh.normals);
    extractAttribute("TEXCOORD_0", primitive, meshInfo.triangleMesh.texCoords);
    extractAttribute("TANGENT", primitive, meshInfo.triangleMesh.tangents);

    return meshInfo;
}

std::optional<std::tuple<int, int, unsigned char *>> LoadImage(std::string_view path, const fastgltf::Asset &asset, const fastgltf::Image &image) {
    return std::visit(
        fastgltf::visitor{
            [&path](const fastgltf::sources::URI &uri) -> std::optional<std::tuple<int, int, unsigned char *>> {
                std::filesystem::path parentPath = std::filesystem::path(path).parent_path();

                std::filesystem::path fullPath = parentPath / uri.uri.fspath();

                return LoadImage(fullPath.string());
            },
            [](const fastgltf::sources::Vector &vector) -> std::optional<std::tuple<int, int, unsigned char *>> {
                int width{};
                int height{};
                int channels{};

                unsigned char *data = stbi_load_from_memory(
                    reinterpret_cast<const unsigned char *>(vector.bytes.data()),
                    static_cast<int>(vector.bytes.size()),
                    &width,
                    &height,
                    &channels,
                    STBI_rgb_alpha
                );

                if (!data) {
                    DebugWarning("Failed to load image");
                    return std::nullopt;
                }

                return std::make_tuple(width, height, data);
            },
            [&asset](const fastgltf::sources::BufferView &bufferView) {
                const fastgltf::BufferView &bv     = asset.bufferViews[bufferView.bufferViewIndex];
                const fastgltf::Buffer     &buffer = asset.buffers[bv.bufferIndex];
                return std::visit(
                    fastgltf::visitor{
                        [&bv](const fastgltf::sources::Vector &vector) -> std::optional<std::tuple<int, int, unsigned char *>> {
                            int width{};
                            int height{};
                            int channels{};

                            unsigned char *data = stbi_load_from_memory(
                                reinterpret_cast<const unsigned char *>(vector.bytes.data() + bv.byteOffset),
                                static_cast<int>(bv.byteLength),
                                &width,
                                &height,
                                &channels,
                                STBI_rgb_alpha
                            );

                            if (!data) {
                                DebugWarning("Failed to load image");
                                return std::nullopt;
                            }

                            return std::make_tuple(width, height, data);
                        },
                        [](auto &arg) -> std::optional<std::tuple<int, int, unsigned char *>> { return std::nullopt; }
                    },

                    buffer.data
                );
            },

            [](auto &arg) -> std::optional<std::tuple<int, int, unsigned char *>> {
                DebugWarning("Invalid image data");
                return std::nullopt;
            }
        },
        image.data
    );
}

std::optional<std::tuple<int, int, unsigned char *>> LoadImage(const std::string &path) {
    int width{};
    int height{};
    int channels{};

    DebugInfo("Loading image from {}", path);
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
        DebugWarning("Failed to load image from {}", path);
        return std::nullopt;
    }
    return std::make_tuple(width, height, data);
}

shader_io::SamplerType LoadSampler(const fastgltf::Sampler &sampler) {
    switch (sampler.magFilter.value_or(fastgltf::Filter::Nearest)) {
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
        return shader_io::SamplerType::Linear;

    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
    default:
        return shader_io::SamplerType::Nearest;
    }
}

shader_io::MaterialInfo LoadMaterial(const fastgltf::Material &material) {
    shader_io::MaterialInfo result{};

    if (material.pbrData.baseColorTexture.has_value()) {
        result.albedoTextureIndex = static_cast<int32_t>(material.pbrData.baseColorTexture->textureIndex);
    } else {
        result.albedoFactor.x = material.pbrData.baseColorFactor.x();
        result.albedoFactor.y = material.pbrData.baseColorFactor.y();
        result.albedoFactor.z = material.pbrData.baseColorFactor.z();
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
        result.ormTextureIndex = static_cast<int32_t>(material.pbrData.metallicRoughnessTexture->textureIndex);
    } else {
        result.metallicFactor  = material.pbrData.metallicFactor;
        result.roughnessFactor = material.pbrData.roughnessFactor;
    }

    if (material.emissiveTexture.has_value()) {
        result.emissiveTextureIndex = static_cast<int32_t>(material.emissiveTexture->textureIndex);
    } else {
        result.emissiveFactor.x = material.emissiveFactor.x();
        result.emissiveFactor.y = material.emissiveFactor.y();
        result.emissiveFactor.z = material.emissiveFactor.z();
    }
    if (material.normalTexture.has_value()) {
        result.normalTextureIndex = static_cast<int32_t>(material.normalTexture->textureIndex);
    }

    return result;
}
} // namespace drez::file_system