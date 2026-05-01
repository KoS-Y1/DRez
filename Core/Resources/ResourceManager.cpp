//
// Created by y1 on 2026-04-30.
//

#include "ResourceManager.h"

#include <directx/d3dx12.h>

#include "DXApp.h"
#include "DXUtils.h"
#include "Debug.h"
#include "FileSystem.h"
#include "GltfLoader.h"
#include "ThreadPool.h"

void ResourceManager::Init(DXApp &app) {
    // Load gltf
    {
        const std::vector<Key> keys = drez::file_system::GetFilesWithExtension("../Assets/Models", ".gltf");

        for (const auto &key: keys) {
            LoadGltf(app, key);
        }
    }

    const D3D12_SAMPLER_DESC defaultSampler{
        .Filter         = D3D12_FILTER_MIN_MAG_MIP_POINT,
        .AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias     = 0.0f,
        .MaxAnisotropy  = 1,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
        .MinLOD         = 0.0f,
        .MaxLOD         = D3D12_FLOAT32_MAX,
    };

    // Load and create skybox textures and material
    // {
    //     std::vector<Key> keys = drez::file_system::GetFilesWithExtension("../Assets/Models/Skybox", ".png");
    //     m_textures.reserve(m_textures.size() + keys.size());
    //     m_textureLookup.reserve(m_textures.size());
    //     for (const auto &key: keys) {
    //         const uint32_t handle = static_cast<uint32_t>(m_textures.size());
    //
    //         const std::optional<std::tuple<int, int, unsigned char *>> imageData = drez::file_system::LoadImage(key);
    //
    //         DebugCheckCritical(imageData.has_value(), "Failed to load skybox image {}", key);
    //
    //         auto width  = static_cast<uint32_t>(std::get<0>(imageData.value()));
    //         auto height = static_cast<uint32_t>(std::get<1>(imageData.value()));
    //
    //         m_textures.emplace_back(
    //             app,
    //             width,
    //             height,
    //             VK_FORMAT_R8G8B8A8_UNORM,
    //             sizeof(unsigned char) * 4,
    //             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    //             VK_IMAGE_ASPECT_COLOR_BIT,
    //             std::get<2>(imageData.value()),
    //             defaultSampler,
    //             key
    //         );
    //         m_textureLookup.emplace(key, handle);
    //
    //         if (key.find("brdf_lut") != std::string::npos) {
    //             m_skyboxMaterial.brdfLut = static_cast<int32_t>(handle);
    //         } else if (key.find("irradiance") != std::string::npos) {
    //             m_skyboxMaterial.irradianceTexture = static_cast<int32_t>(handle);
    //         } else if (key.find("specular") != std::string::npos) {
    //             m_skyboxMaterial.specularTexture = static_cast<int32_t>(handle);
    //         } else {
    //             m_skyboxMaterial.skyboxTexture = static_cast<int32_t>(handle);
    //         }
    //     }
    // }


    // Mesh buffer
    {
        m_meshBuffer = app.CreateBuffer(
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON,
            std::span<shader_io::MeshInfo>(m_meshInfos).size_bytes(),
            "mesh_buffer"
        );

        DXBuffer stagingBuffer = app.CreateBuffer(
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_HEAP_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            std::span<shader_io::MeshInfo>(m_meshInfos).size_bytes(),
            "staging_buffer_mesh"
        );

        const D3D12_SUBRESOURCE_DATA data{
            .pData      = m_meshInfos.data(),
            .RowPitch   = static_cast<LONG_PTR>(std::span<shader_io::MeshInfo>(m_meshInfos).size_bytes()),
            .SlicePitch = static_cast<LONG_PTR>(std::span<shader_io::MeshInfo>(m_meshInfos).size_bytes()),
        };
        app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_meshBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_meshBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            );
            commandList->ResourceBarrier(1, &barrier);
        });
    }

    // Material buffer
    {
        m_materialBuffer = app.CreateBuffer(
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON,
            std::span<shader_io::MaterialInfo>(m_materials).size_bytes(),
            "material_buffer"
        );

        DXBuffer stagingBuffer = app.CreateBuffer(
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_HEAP_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            std::span<shader_io::MaterialInfo>(m_materials).size_bytes(),
            "staging_buffer_material"
        );

        const D3D12_SUBRESOURCE_DATA data{
            .pData      = m_materials.data(),
            .RowPitch   = static_cast<LONG_PTR>(std::span<shader_io::MaterialInfo>(m_materials).size_bytes()),
            .SlicePitch = static_cast<LONG_PTR>(std::span<shader_io::MaterialInfo>(m_materials).size_bytes()),
        };

        app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_materialBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_materialBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            );
            commandList->ResourceBarrier(1, &barrier);
        });
    }
}

uint32_t ResourceManager::GetMeshHandle(const Key &key) const {
    auto pair = m_meshLookup.find(key);
    DebugCheckCritical(pair != m_meshLookup.end(), "{} not exists", key);
    return pair->second;
}

uint32_t ResourceManager::GetMaterialHandle(const Key &key) const {
    auto pair = m_materialLookup.find(key);
    DebugCheckCritical(pair != m_materialLookup.end(), "{} not exists", key);
    return pair->second;
}

void ResourceManager::LoadGltf(DXApp &app, const std::string &fileName) {
    const std::optional<fastgltf::Asset> asset = drez::file_system::LoadGltf(fileName);

    DebugCheckCritical(asset.has_value(), "Failed to load GLTF asset from {}", fileName);

    // Create buffer to store GLTF data and upload it to GPU
    uint32_t gltfHandle = static_cast<uint32_t>(m_gltfBuffers.size());
    DXBuffer gltfBuffer;
    {
        uint64_t               bufferSize;
        DXBuffer               stagingBuffer;
        D3D12_SUBRESOURCE_DATA data;

        std::visit(
            fastgltf::visitor{
                [&app, &bufferSize, &stagingBuffer, &data](const fastgltf::sources::Array &array) {
                    bufferSize    = array.bytes.size_bytes();
                    stagingBuffer = app.CreateBuffer(
                        D3D12_HEAP_TYPE_UPLOAD,
                        D3D12_HEAP_FLAG_NONE,
                        D3D12_RESOURCE_STATE_GENERIC_READ,
                        bufferSize,
                        "staging_buffer_gltf"
                    );
                    data.pData      = array.bytes.data();
                    data.RowPitch   = bufferSize;
                    data.SlicePitch = bufferSize;
                },
                [&app, &bufferSize, &stagingBuffer, &data](const fastgltf::sources::ByteView &view) {
                    bufferSize    = view.bytes.size_bytes();
                    stagingBuffer = app.CreateBuffer(
                        D3D12_HEAP_TYPE_UPLOAD,
                        D3D12_HEAP_FLAG_NONE,
                        D3D12_RESOURCE_STATE_GENERIC_READ,
                        bufferSize,
                        "staging_buffer_gltf"
                    );
                    data.pData      = view.bytes.data();
                    data.RowPitch   = bufferSize;
                    data.SlicePitch = bufferSize;
                },
                [&fileName](const auto &arg) { DebugCheckCritical(false, "Failed to load buffer data from {}", fileName); }
            },
            asset->buffers[0].data
        );

        gltfBuffer = app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, bufferSize, "gltf_buffer");

        app.ImmediateSubmit([&gltfBuffer, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, gltfBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                gltfBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            );
            commandList->ResourceBarrier(1, &barrier);
        });
    }

    const D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = gltfBuffer.GetGPUVirtualAddress();
    // Load meshes
    for (size_t meshIndex = 0; meshIndex < asset->meshes.size(); meshIndex++) {
        ThreadPool::GetInstance().Enqueue([&, meshIndex]() {
            const fastgltf::Mesh              &mesh     = asset->meshes[meshIndex];
            std::optional<shader_io::MeshInfo> meshInfo = drez::file_system::LoadMesh(asset.value(), mesh, bufferAddress);

            DebugCheckCritical(meshInfo.has_value(), "Failed to load mesh data from {} {}", fileName, mesh.name);

            std::scoped_lock<std::mutex> lk{m_meshMutex};

            auto pair = m_meshLookup.find(std::string(mesh.name));
            Key  key{mesh.name};
            if (pair != m_meshLookup.end()) {
                key += "_copy";
            }

            uint32_t meshHandle = static_cast<uint32_t>(m_meshes.size());
            m_meshLookup.emplace(key, meshHandle);
            m_meshes.emplace_back(meshInfo.value(), gltfHandle, key);
            m_meshInfos.emplace_back(meshInfo.value());

            DebugInfo("Mesh {} is loaded successfully", key);
        });
    }

    // Load samplers
    std::vector<D3D12_SAMPLER_DESC> infoSamplers(asset->samplers.size());
    for (size_t i = 0; i < asset->samplers.size(); ++i) {
        ThreadPool::GetInstance().Enqueue([&, i]() { infoSamplers[i] = drez::file_system::LoadSampler(asset->samplers[i]); });
    }

    const D3D12_SAMPLER_DESC defaultSampler{
        .Filter         = D3D12_FILTER_MIN_MAG_MIP_POINT,
        .AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias     = 0.0f,
        .MaxAnisotropy  = 1,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
        .MinLOD         = 0.0f,
        .MaxLOD         = D3D12_FLOAT32_MAX,
    };

    // Load images
    std::vector<std::tuple<int, int, unsigned char *>> images(asset->images.size());
    std::vector<std::string>                           imageNames(asset->images.size());
    for (size_t i = 0; i < asset->images.size(); i++) {
        ThreadPool::GetInstance().Enqueue([&, i]() {
            const fastgltf::Image                               &image     = asset->images[i];
            std::optional<std::tuple<int, int, unsigned char *>> imageData = drez::file_system::LoadImage(fileName, asset.value(), image);
            std::string                                          name{};

            if (!image.name.empty()) {
                name = image.name;
            } else {
                std::visit(
                    fastgltf::visitor{
                        [&name](const fastgltf::sources::URI &uri) { name = uri.uri.fspath().string(); },
                        [&name, &fileName](const auto &arg) { name = std::string(fileName); }
                    },
                    image.data
                );
            }

            DebugCheckCritical(imageData.has_value(), "Failed to load image data from {}, image {}", fileName, name);

            images[i]     = std::move(imageData.value());
            imageNames[i] = std::move(name);
        });
    }

    ThreadPool::GetInstance().WaitIdle();

    const uint32_t textureOffset = static_cast<uint32_t>(m_textures.size());

    // Load and create textures
    std::vector<DXTexture> textures(asset->textures.size());
    for (size_t i = 0; i < asset->textures.size(); ++i) {
        ThreadPool::GetInstance().Enqueue([&, i]() {
            const fastgltf::Texture &texture = asset->textures[i];
            DebugCheckCritical(texture.imageIndex.has_value(), "{} in {} does not have an image index", texture.name, fileName);

            size_t imageIndex = texture.imageIndex.value();

            std::scoped_lock<std::mutex> lk{m_textureMutex};

            auto pair = m_textureLookup.find(imageNames[imageIndex]);
            Key  key{imageNames[imageIndex]};

            if (pair != m_textureLookup.end()) {
                key += "_copy";
            }

            auto width  = static_cast<uint32_t>(std::get<0>(images[imageIndex]));
            auto height = static_cast<uint32_t>(std::get<1>(images[imageIndex]));

            m_textureLookup.emplace(key, textureOffset + static_cast<uint32_t>(i));
            textures[i] = app.CreateTexture(
                width,
                height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                drez::dx_utils::GetFormatSize(DXGI_FORMAT_R8G8B8A8_UNORM),
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_HEAP_FLAG_NONE,
                texture.samplerIndex.has_value() ? infoSamplers[texture.samplerIndex.value()] : defaultSampler,
                std::get<2>(images[imageIndex]),
                key
            );

            DebugInfo("Texture {} is loaded successfully", key);
        });
    }

    ThreadPool::GetInstance().WaitIdle();
    m_textures.insert(m_textures.end(), std::make_move_iterator(textures.begin()), std::make_move_iterator(textures.end()));

    // Lambda for add texture offset to current gltf texture index
    const auto addTextureOffset = [textureOffset](int32_t index) -> int32_t {
        if (index == -1) {
            return -1;
        }

        return static_cast<int32_t>(textureOffset) + index;
    };

    // Load materials
    m_materials.reserve(m_textures.size() + asset->materials.size());
    m_materialKeys.reserve(asset->materials.size() + asset->textures.size());
    for (size_t i = 0; i < asset->materials.size(); i++) {
        ThreadPool::GetInstance().Enqueue([&, i]() {
            const fastgltf::Material &material     = asset->materials[i];
            shader_io::MaterialInfo   materialInfo = drez::file_system::LoadMaterial(material);

            std::scoped_lock<std::mutex> lk{m_textureMutex};

            auto pair = m_materialLookup.find(std::string(material.name));
            Key  key{material.name};
            if (pair != m_materialLookup.end()) {
                key += "_copy";
            }

            materialInfo.albedoTextureIndex   = addTextureOffset(materialInfo.albedoTextureIndex);
            materialInfo.ormTextureIndex      = addTextureOffset(materialInfo.ormTextureIndex);
            materialInfo.emissiveTextureIndex = addTextureOffset(materialInfo.emissiveTextureIndex);
            materialInfo.normalTextureIndex   = addTextureOffset(materialInfo.normalTextureIndex);

            m_materialLookup.emplace(key, static_cast<uint32_t>(m_materials.size()));
            m_materials.push_back(materialInfo);
            m_materialKeys.push_back(key);

            DebugInfo("Material {} is loaded successfully", key);
        });
    }

    m_gltfBuffers.push_back(std::move(gltfBuffer));

    ThreadPool::GetInstance().WaitIdle();
}
