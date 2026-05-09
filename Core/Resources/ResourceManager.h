//
// Created by y1 on 2026-04-30.
//

#pragma once

#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "DXBUffer.h"
#include "DXTexture.h"
#include "Mesh.h"
#include "Singleton.h"

#include "resources_io.slang"

class DXApp;

class ResourceManager : public Singleton<ResourceManager> {
    using Key = std::string;

    struct ShaderResource {
        enum class ResourceType : uint8_t {
            eGltfBuffer = 0,
            eTexture,
            eUnused = 0xff
        };

        ResourceType     resourceType{ResourceType::eUnused};
        std::string_view name{};
        uint32_t         physicalIndex{};
        uint32_t         heapIndex{};

        ShaderResource() = default;

        ShaderResource(ResourceType resourceType, const std::string_view name, uint32_t physicalIndex, uint32_t heapIndex)
            : resourceType(resourceType)
            , name(name)
            , physicalIndex(physicalIndex)
            , heapIndex(heapIndex) {};
    };

public:
    void Init(DXApp &app);

    [[nodiscard]] uint32_t GetMeshHandle(const Key &key) const;
    [[nodiscard]] uint32_t GetMaterialHandle(const Key &key) const;

    [[nodiscard]] const DXBuffer &GetGltfBuffer(uint32_t handle) const { return m_gltfBuffers[handle]; }

    [[nodiscard]] std::span<const DXTexture> GetAllTextures() const { return m_textures; }

    [[nodiscard]] const Mesh &GetMesh(uint32_t handle) const { return m_meshes[handle]; }

    [[nodiscard]] const shader_io::MaterialInfo &GetMaterialInfo(uint32_t handle) const { return m_materialInfo[handle]; }

    [[nodiscard]] std::string_view GetMaterialKey(uint32_t handle) const { return m_materialKeys[handle]; }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetMeshInfoBufferAddress() const { return m_meshInfoBuffer.GetGPUVirtualAddress(); }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetMaterialBufferAddress() const { return m_materialInfoBuffer.GetGPUVirtualAddress(); }

    // [[nodiscard]] const shader_io::SkyboxMaterialInfo &GetSkyboxMaterial() const { return m_skyboxMaterial; }

protected:
    ResourceManager()  = default;
    ~ResourceManager() = default;

private:
    // GLTF data
    std::vector<DXBuffer> m_gltfBuffers;

    // Mesh
    std::unordered_map<Key, uint32_t> m_meshLookup;
    std::vector<Mesh>                 m_meshes;
    std::vector<shader_io::MeshInfo>  m_meshInfos;
    DXBuffer                          m_meshInfoBuffer;

    // Texture
    std::unordered_map<Key, uint32_t> m_textureLookup;
    std::vector<DXTexture>            m_textures;

    // Material
    std::unordered_map<Key, uint32_t>    m_materialLookup;
    std::vector<shader_io::MaterialInfo> m_materialInfo;
    std::vector<std::string>             m_materialKeys;
    DXBuffer                             m_materialInfoBuffer;

    // Instance
    std::unordered_map<Key, uint32_t>    m_instanceLookup;
    std::vector<shader_io::InstanceInfo> m_instanceInfo;
    DXBuffer                             m_instanceInfoBuffer;

    // Bindless resources
    std::unordered_map<Key, uint32_t> m_resourceLookup;
    std::vector<ShaderResource>       m_resources;

    // shader_io::SkyboxMaterialInfo m_skyboxMaterial;

    std::mutex m_meshMutex{};
    std::mutex m_textureMutex{};

    void LoadGltf(DXApp &state, const std::string &fileName);
};
