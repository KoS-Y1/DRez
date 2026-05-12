//
// Created by y1 on 2026-04-30.
//

#include "ResourceManager.h"

#include <algorithm>
#include <ranges>

#include <directx/d3dx12.h>
#include <directxmath.h>
#include <fastgltf/types.hpp>

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

    // Load and create skybox / IBL textures
    {
        const std::vector<Key> keys = drez::file_system::GetFilesWithExtension("../Assets/Models/Skybox", ".png");

        for (const auto &key: keys) {
            const std::optional<std::tuple<int, int, unsigned char *>> imageData = drez::file_system::LoadImage(key);

            DebugCheckCritical(imageData.has_value(), "Failed to load skybox image {}", key);

            const auto width  = static_cast<uint32_t>(std::get<0>(imageData.value()));
            const auto height = static_cast<uint32_t>(std::get<1>(imageData.value()));

            DXTexture texture = app.CreateTexture(
                width,
                height,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_HEAP_FLAG_NONE,
                shader_io::SamplerType::Nearest,
                key
            );
            texture.Upload(app, std::get<2>(imageData.value()));

            const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
                .Format                  = texture.GetFormat(),
                .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2D               = {.MipLevels = 1},
            };
            DXShaderResourceView srv = app.CreateDXShaderResourceView(texture.GetResource(), desc);

            if (key.find("brdf_lut") != std::string::npos) {
                m_brdfLutTexture = std::move(texture);
                m_brdfLutSrv     = std::move(srv);
            } else if (key.find("irradiance") != std::string::npos) {
                m_irradianceTexture = std::move(texture);
                m_irradianceSrv     = std::move(srv);
            } else if (key.find("specular") != std::string::npos) {
                m_specularTexture = std::move(texture);
                m_specularSrv     = std::move(srv);
            } else {
                m_skyboxTexture = std::move(texture);
                m_skyboxSrv     = std::move(srv);
            }
        }
    }


    // Mesh buffer
    {
        const uint64_t size = std::span<shader_io::MeshInfo>(m_meshInfos).size_bytes();

        m_meshInfoBuffer = app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, size, "mesh_buffer");

        DXBuffer stagingBuffer =
            app.CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, size, "staging_buffer_mesh");

        const D3D12_SUBRESOURCE_DATA data{
            .pData      = m_meshInfos.data(),
            .RowPitch   = static_cast<LONG_PTR>(size),
            .SlicePitch = static_cast<LONG_PTR>(size),
        };
        app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_meshInfoBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_meshInfoBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = DXGI_FORMAT_UNKNOWN,
            .ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer                  = {
                                        .FirstElement        = 0,
                                        .NumElements         = static_cast<uint32_t>(m_meshInfos.size()),
                                        .StructureByteStride = sizeof(shader_io::MeshInfo),
                                        .Flags               = D3D12_BUFFER_SRV_FLAG_NONE,
                                        },
        };
        m_meshInfoBufferSrv = app.CreateDXShaderResourceView(m_meshInfoBuffer.GetBuffer(), desc);
    }

    // Index views
    std::ranges::for_each(m_meshes, [](Mesh &mesh) { mesh.CreateIndexView(); });


    // Material buffer
    {
        const uint64_t size = std::span<shader_io::MaterialInfo>(m_materialInfo).size_bytes();

        m_materialInfoBuffer = app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, size, "material_buffer");

        DXBuffer stagingBuffer =
            app.CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, size, "staging_buffer_material");

        const D3D12_SUBRESOURCE_DATA data{
            .pData      = m_materialInfo.data(),
            .RowPitch   = static_cast<LONG_PTR>(size),
            .SlicePitch = static_cast<LONG_PTR>(size),
        };

        app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_materialInfoBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_materialInfoBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = DXGI_FORMAT_UNKNOWN,
            .ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer                  = {
                                        .FirstElement        = 0,
                                        .NumElements         = static_cast<uint32_t>(m_materialInfo.size()),
                                        .StructureByteStride = sizeof(shader_io::MaterialInfo),
                                        .Flags               = D3D12_BUFFER_SRV_FLAG_NONE,
                                        },
        };
        m_materialInfoBufferSrv = app.CreateDXShaderResourceView(m_materialInfoBuffer.GetBuffer(), desc);
    }

    // Instance buffer
    {
        const uint64_t size = std::span<shader_io::InstanceInfo>(m_instanceInfo).size_bytes();

        m_instanceInfoBuffer = app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, size, "instance_buffer");

        DXBuffer stagingBuffer =
            app.CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, size, "staging_buffer_instance");

        const D3D12_SUBRESOURCE_DATA data{
            .pData      = m_instanceInfo.data(),
            .RowPitch   = static_cast<LONG_PTR>(size),
            .SlicePitch = static_cast<LONG_PTR>(size),
        };

        app.ImmediateSubmit([this, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, m_instanceInfoBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_instanceInfoBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = DXGI_FORMAT_UNKNOWN,
            .ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer                  = {
                                        .FirstElement        = 0,
                                        .NumElements         = static_cast<uint32_t>(m_instanceInfo.size()),
                                        .StructureByteStride = sizeof(shader_io::InstanceInfo),
                                        .Flags               = D3D12_BUFFER_SRV_FLAG_NONE,
                                        },
        };
        m_instanceInfoBufferSrv = app.CreateDXShaderResourceView(m_instanceInfoBuffer.GetBuffer(), desc);
    }
}

uint32_t ResourceManager::GetMeshHandle(const Key &key) const {
    auto pair = m_meshLookup.find(key);
    DebugCheckCritical(pair != m_meshLookup.end(), "Mesh {} does not exist", key);
    return pair->second;
}

uint32_t ResourceManager::GetMaterialHandle(const Key &key) const {
    auto pair = m_materialLookup.find(key);
    DebugCheckCritical(pair != m_materialLookup.end(), "Material {} does not exist", key);
    return pair->second;
}

void ResourceManager::LoadGltf(DXApp &app, const std::string &fileName) {
    DebugInfo("Start loading gltf from {}", fileName);

    const std::optional<fastgltf::Asset> asset = drez::file_system::LoadGltf(fileName);

    DebugCheckCritical(asset.has_value(), "Failed to load GLTF asset from {}", fileName);

    std::string gltfName = drez::file_system::GetFileNameWithoutExtension(fileName);

    // Create buffer to store GLTF data and upload it to GPU
    DXBuffer             gltfBuffer;
    DXShaderResourceView gltfBufferSrv;
    {
        uint64_t               bufferSize;
        DXBuffer               stagingBuffer;
        D3D12_SUBRESOURCE_DATA data;

        std::visit(
            fastgltf::visitor{
                [&app, &bufferSize, &stagingBuffer, &data, &gltfName](const fastgltf::sources::Array &array) {
                    bufferSize    = array.bytes.size_bytes();
                    stagingBuffer = app.CreateBuffer(
                        D3D12_HEAP_TYPE_UPLOAD,
                        D3D12_HEAP_FLAG_NONE,
                        D3D12_RESOURCE_STATE_GENERIC_READ,
                        bufferSize,
                        "staging_buffer_gltf" + gltfName
                    );
                    data.pData      = array.bytes.data();
                    data.RowPitch   = bufferSize;
                    data.SlicePitch = bufferSize;
                },
                [&app, &bufferSize, &stagingBuffer, &data, &gltfName](const fastgltf::sources::ByteView &view) {
                    bufferSize    = view.bytes.size_bytes();
                    stagingBuffer = app.CreateBuffer(
                        D3D12_HEAP_TYPE_UPLOAD,
                        D3D12_HEAP_FLAG_NONE,
                        D3D12_RESOURCE_STATE_GENERIC_READ,
                        bufferSize,
                        "staging_buffer_gltf" + gltfName
                    );
                    data.pData      = view.bytes.data();
                    data.RowPitch   = bufferSize;
                    data.SlicePitch = bufferSize;
                },
                [&fileName](const auto &arg) { DebugCheckCritical(false, "Failed to load buffer data from {}", fileName); }
            },
            asset->buffers[0].data
        );

        gltfBuffer =
            app.CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, bufferSize, "gltf_buffer" + gltfName);

        app.ImmediateSubmit([&gltfBuffer, &stagingBuffer, &data](ID3D12GraphicsCommandList *commandList) {
            UpdateSubresources<1>(commandList, gltfBuffer.GetBuffer(), stagingBuffer.GetBuffer(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                gltfBuffer.GetBuffer(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer                  = {
                                        .FirstElement        = 0,
                                        .NumElements         = static_cast<uint32_t>(gltfBuffer.GetBufferSize() / sizeof(uint32_t)),
                                        .StructureByteStride = 0,
                                        .Flags               = D3D12_BUFFER_SRV_FLAG_RAW,
                                        },
        };
        gltfBufferSrv = app.CreateDXShaderResourceView(gltfBuffer.GetBuffer(), desc);
    }

    const uint32_t                  gltfBufferIndex   = gltfBufferSrv.GetIndex();
    const D3D12_GPU_VIRTUAL_ADDRESS gltfBufferAddress = gltfBuffer.GetGPUVirtualAddress();
    m_gltfBuffers.push_back(std::move(gltfBuffer));
    m_gltfBufferSrvs.push_back(std::move(gltfBufferSrv));

    // Per-gltf-mesh handle and material-index tables, populated by the mesh-load tasks below
    std::vector<std::vector<uint32_t>> meshHandlesPerGltfMesh(asset->meshes.size());
    std::vector<std::vector<int>>      primMaterialIndicesPerGltfMesh(asset->meshes.size());

    std::ranges::for_each(std::views::iota(size_t{0}, asset->meshes.size()), [&](size_t meshIndex) {
        ThreadPool::GetInstance().Enqueue([&, meshIndex]() {
            const fastgltf::Mesh                           &mesh       = asset->meshes[meshIndex];
            std::vector<drez::file_system::LoadedPrimitive> primitives = drez::file_system::LoadMesh(asset.value(), mesh, gltfBufferIndex);

            DebugCheckCritical(!primitives.empty(), "Failed to load mesh data from {} {}", fileName, mesh.name);

            std::scoped_lock<std::mutex> lk{m_meshMutex};

            auto &handles = meshHandlesPerGltfMesh[meshIndex];
            auto &matIdxs = primMaterialIndicesPerGltfMesh[meshIndex];
            handles.reserve(primitives.size());
            matIdxs.reserve(primitives.size());

            std::ranges::for_each(std::views::iota(size_t{0}, primitives.size()), [&](size_t p) {
                Key key = std::string(mesh.name) + "_p" + std::to_string(p);
                while (m_meshLookup.contains(key)) {
                    key += "_copy";
                }

                const uint32_t handle = static_cast<uint32_t>(m_meshes.size());
                m_meshLookup.emplace(key, handle);
                m_meshes.emplace_back(primitives[p].meshInfo, key, gltfBufferAddress);
                m_meshInfos.emplace_back(primitives[p].meshInfo);
                handles.push_back(handle);
                matIdxs.push_back(primitives[p].materialIndex);
            });
        });
    });

    // Load samplers
    std::vector<shader_io::SamplerType> infoSamplers(asset->samplers.size());
    for (size_t i = 0; i < asset->samplers.size(); ++i) {
        ThreadPool::GetInstance().Enqueue([&, i]() { infoSamplers[i] = drez::file_system::LoadSampler(asset->samplers[i]); });
    }

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

    // Per-texture upload sizes (placement-aligned). Used to partition into batches that each fit in one upload heap.
    std::vector<uint64_t> textureUploadSizes(asset->textures.size(), 0);
    std::ranges::for_each(std::views::iota(size_t{0}, asset->textures.size()), [&](size_t i) {
        if (!asset->textures[i].imageIndex.has_value()) {
            return;
        }
        const size_t   imageIndex = asset->textures[i].imageIndex.value();
        const uint64_t width      = static_cast<uint64_t>(std::get<0>(images[imageIndex]));
        const uint32_t height     = static_cast<uint32_t>(std::get<1>(images[imageIndex]));
        textureUploadSizes[i]     = app.GetTextureUploadSize(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    });

    // Load and create textures
    std::vector<DXTexture>            textures(asset->textures.size());
    std::vector<DXShaderResourceView> textureSrvs(asset->textures.size());
    std::vector<uint32_t>             textureBindlessIndices(asset->textures.size());

    const auto stageTexture = [&](size_t i) {
        const fastgltf::Texture &texture = asset->textures[i];
        DebugCheckCritical(texture.imageIndex.has_value(), "{} in {} does not have an image index", texture.name, fileName);

        size_t imageIndex = texture.imageIndex.value();

        // Reserve a unique key under the lock so concurrent threads see the collision; release before heavy GPU work.
        Key key;
        {
            std::scoped_lock<std::mutex> lk{m_textureMutex};
            key = imageNames[imageIndex];
            if (m_textureLookup.contains(key)) {
                key += "_copy";
            }
            m_textureLookup.emplace(key, UINT32_MAX);
        }

        auto width  = static_cast<uint32_t>(std::get<0>(images[imageIndex]));
        auto height = static_cast<uint32_t>(std::get<1>(images[imageIndex]));

        const uint16_t mipLevels = std::floor(std::log2(std::max(width, height))) + 1;

        textures[i] = app.CreateTexture(
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, // For generating mipmaps
            D3D12_HEAP_FLAG_NONE,
            texture.samplerIndex.has_value() ? infoSamplers[texture.samplerIndex.value()] : shader_io::SamplerType::Nearest,
            key,
            DXGI_FORMAT_UNKNOWN,
            mipLevels
        );
        app.BatchedTextureUpload(textures[i], std::get<2>(images[imageIndex]));

        const D3D12_SHADER_RESOURCE_VIEW_DESC desc{
            .Format                  = textures[i].GetFormat(),
            .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D               = {.MipLevels = textures[i].GetMipLevels()},
        };
        textureSrvs[i]            = app.CreateDXShaderResourceView(textures[i].GetResource(), desc);
        textureBindlessIndices[i] = textureSrvs[i].GetIndex();

        {
            std::scoped_lock<std::mutex> lk{m_textureMutex};
            m_textureLookup[key] = textureBindlessIndices[i];
        }
    };

    // D3D12 caps a single CPU-visible buffer at ~4 GB; chunk under that so each batch's shared upload heap fits.
    constexpr uint64_t kMaxBatchBytes = 2ULL * 1024 * 1024 * 1024;

    size_t batchStart = 0;
    while (batchStart < asset->textures.size()) {
        size_t   batchEnd   = batchStart;
        uint64_t batchBytes = 0;
        while (batchEnd < asset->textures.size()) {
            const uint64_t next = batchBytes + textureUploadSizes[batchEnd];
            if (next > kMaxBatchBytes && batchBytes > 0) {
                break;
            }
            batchBytes = next;
            ++batchEnd;
        }

        if (batchBytes > 0) {
            app.BeginBatchUpload(batchBytes);
        }
        std::ranges::for_each(std::views::iota(batchStart, batchEnd), [&](size_t i) {
            ThreadPool::GetInstance().Enqueue([&, i]() { stageTexture(i); });
        });
        ThreadPool::GetInstance().WaitIdle();
        if (batchBytes > 0) {
            app.BatchedTextureFlush();

            std::ranges::for_each(std::views::iota(batchStart, batchEnd), [&](size_t i) {
                app.GenerateMipmaps(textures[i], textureBindlessIndices[i]);
            });
        }

        batchStart = batchEnd;
    }

    m_textures.insert(m_textures.end(), std::make_move_iterator(textures.begin()), std::make_move_iterator(textures.end()));
    m_textureSrvs.insert(m_textureSrvs.end(), std::make_move_iterator(textureSrvs.begin()), std::make_move_iterator(textureSrvs.end()));

    // Translate gltf-local texture index to bindless heap index
    const auto toBindlessIndex = [&textureBindlessIndices](int32_t index) -> int32_t {
        if (index == -1) {
            return -1;
        }
        return static_cast<int32_t>(textureBindlessIndices[index]);
    };

    // Resolve sampler type for a gltf-local texture index
    const auto toSamplerType = [&asset, &infoSamplers](int32_t index) -> shader_io::SamplerType {
        if (index == -1) {
            return shader_io::SamplerType::Nearest;
        }
        const fastgltf::Texture &texture = asset->textures[index];
        return texture.samplerIndex.has_value() ? infoSamplers[texture.samplerIndex.value()] : shader_io::SamplerType::Nearest;
    };

    // Load materials
    std::vector<uint32_t> gltfMaterialToManagerHandle(asset->materials.size(), 0);
    m_materialInfo.reserve(m_textures.size() + asset->materials.size());
    m_materialKeys.reserve(asset->materials.size() + asset->textures.size());
    std::ranges::for_each(std::views::iota(size_t{0}, asset->materials.size()), [&](size_t i) {
        ThreadPool::GetInstance().Enqueue([&, i]() {
            const fastgltf::Material &material     = asset->materials[i];
            shader_io::MaterialInfo   materialInfo = drez::file_system::LoadMaterial(material);

            std::scoped_lock<std::mutex> lk{m_textureMutex};

            auto pair = m_materialLookup.find(std::string(material.name));
            Key  key{material.name};
            if (pair != m_materialLookup.end()) {
                key += "_copy";
            }

            materialInfo.albedoSamplerType   = toSamplerType(materialInfo.albedoTextureIndex);
            materialInfo.ormSamplerType      = toSamplerType(materialInfo.ormTextureIndex);
            materialInfo.emissiveSamplerType = toSamplerType(materialInfo.emissiveTextureIndex);
            materialInfo.normalSamplerType   = toSamplerType(materialInfo.normalTextureIndex);

            materialInfo.albedoTextureIndex   = toBindlessIndex(materialInfo.albedoTextureIndex);
            materialInfo.ormTextureIndex      = toBindlessIndex(materialInfo.ormTextureIndex);
            materialInfo.emissiveTextureIndex = toBindlessIndex(materialInfo.emissiveTextureIndex);
            materialInfo.normalTextureIndex   = toBindlessIndex(materialInfo.normalTextureIndex);

            const uint32_t materialHandle = static_cast<uint32_t>(m_materialInfo.size());
            m_materialLookup.emplace(key, materialHandle);
            m_materialInfo.push_back(materialInfo);
            m_materialKeys.push_back(key);
            gltfMaterialToManagerHandle[i] = materialHandle;
        });
    });

    ThreadPool::GetInstance().WaitIdle();

    // Scene walk
    const size_t sceneIndex = asset->defaultScene.has_value() ? asset->defaultScene.value() : 0;
    DebugCheckCritical(sceneIndex < asset->scenes.size(), "Default scene index out of range in {}", fileName);
    const fastgltf::Scene  &scene    = asset->scenes[sceneIndex];
    const DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
    std::ranges::for_each(scene.nodeIndices, [&](size_t rootNodeIndex) {
        EmitNodeInstances(
            asset.value(),
            rootNodeIndex,
            identity,
            meshHandlesPerGltfMesh,
            primMaterialIndicesPerGltfMesh,
            gltfMaterialToManagerHandle
        );
    });
}

void ResourceManager::EmitNodeInstances(
    const fastgltf::Asset                    &asset,
    size_t                                    nodeIndex,
    const DirectX::XMMATRIX                  &parentTransform,
    const std::vector<std::vector<uint32_t>> &meshHandlesPerGltfMesh,
    const std::vector<std::vector<int>>      &primMaterialIndicesPerGltfMesh,
    const std::vector<uint32_t>              &gltfMaterialToManagerHandle
) {
    const fastgltf::Node &node = asset.nodes[nodeIndex];

    // glTF node transforms compose in row-vector convention to match the rest of the codebase
    const DirectX::XMMATRIX localTransform = std::visit(
        fastgltf::visitor{
            [](const fastgltf::TRS &trs) -> DirectX::XMMATRIX {
                const DirectX::XMVECTOR T = DirectX::XMVectorSet(trs.translation[0], trs.translation[1], trs.translation[2], 0.0f);
                const DirectX::XMVECTOR R = DirectX::XMVectorSet(trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3]);
                const DirectX::XMVECTOR S = DirectX::XMVectorSet(trs.scale[0], trs.scale[1], trs.scale[2], 0.0f);
                return DirectX::XMMatrixScalingFromVector(S) * DirectX::XMMatrixRotationQuaternion(R) * DirectX::XMMatrixTranslationFromVector(T);
            },
            // glTF matrices are column-major; transpose into XMMATRIX row-major layout
            [](const fastgltf::math::fmat4x4 &mat) -> DirectX::XMMATRIX {
                return DirectX::XMMATRIX(
                    mat.col(0)[0],
                    mat.col(0)[1],
                    mat.col(0)[2],
                    mat.col(0)[3],
                    mat.col(1)[0],
                    mat.col(1)[1],
                    mat.col(1)[2],
                    mat.col(1)[3],
                    mat.col(2)[0],
                    mat.col(2)[1],
                    mat.col(2)[2],
                    mat.col(2)[3],
                    mat.col(3)[0],
                    mat.col(3)[1],
                    mat.col(3)[2],
                    mat.col(3)[3]
                );
            }
        },
        node.transform
    );

    const DirectX::XMMATRIX worldTransform = XMMatrixMultiply(localTransform, parentTransform);

    if (node.meshIndex.has_value()) {
        const size_t meshIdx = node.meshIndex.value();
        const auto  &handles = meshHandlesPerGltfMesh[meshIdx];
        const auto  &matIdxs = primMaterialIndicesPerGltfMesh[meshIdx];
        std::ranges::for_each(std::views::iota(size_t{0}, handles.size()), [&](size_t p) {
            if (matIdxs[p] < 0) {
                DebugWarning("Primitive {} of glTF mesh {} has no material; skipping instance", p, meshIdx);
                return;
            }

            shader_io::InstanceInfo info{};
            info.meshHandle     = handles[p];
            info.materialHandle = gltfMaterialToManagerHandle[matIdxs[p]];
            DirectX::XMStoreFloat4x4(&info.transform, worldTransform);
            // TODO: inverse transpose for non-uniform scale support
            DirectX::XMStoreFloat4x4(&info.normalMatrix, worldTransform);
            m_instanceInfo.push_back(info);
        });
    }

    std::ranges::for_each(node.children, [&](size_t childIdx) {
        EmitNodeInstances(asset, childIdx, worldTransform, meshHandlesPerGltfMesh, primMaterialIndicesPerGltfMesh, gltfMaterialToManagerHandle);
    });
}
