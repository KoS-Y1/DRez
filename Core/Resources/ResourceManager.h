//
// Created by y1 on 2026-04-30.
//

#pragma once

#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <directxmath.h>
#include <fastgltf/core.hpp>

#include "DXBUffer.h"
#include "DXShaderResourceView.h"
#include "DXTexture.h"
#include "Mesh.h"
#include "Singleton.h"

#include "resources_io.slang"

class DXApp;

class ResourceManager : public Singleton<ResourceManager> {
    using Key = std::string;

public:
    void Init(DXApp &app);

    [[nodiscard]] uint32_t GetMeshHandle(const Key &key) const;
    [[nodiscard]] uint32_t GetMaterialHandle(const Key &key) const;

    [[nodiscard]] const DXBuffer &GetGltfBuffer(uint32_t handle) const { return m_gltfBuffers[handle]; }

    [[nodiscard]] std::span<const DXTexture> GetAllTextures() const { return m_textures; }

    [[nodiscard]] const Mesh &GetMesh(uint32_t handle) const { return m_meshes[handle]; }

    [[nodiscard]] const shader_io::MaterialInfo &GetMaterialInfo(uint32_t handle) const { return m_materialInfo[handle]; }

    [[nodiscard]] std::string_view GetMaterialKey(uint32_t handle) const { return m_materialKeys[handle]; }

    [[nodiscard]] uint32_t GetMeshesBindlessIndex() const { return m_meshInfoBufferSrv.GetIndex(); }

    [[nodiscard]] uint32_t GetMaterialsBindlessIndex() const { return m_materialInfoBufferSrv.GetIndex(); }

    [[nodiscard]] uint32_t GetInstancesBindlessIndex() const { return m_instanceInfoBufferSrv.GetIndex(); }

    [[nodiscard]] uint32_t GetInstanceCount() const { return static_cast<uint32_t>(m_instanceInfo.size()); }

    [[nodiscard]] const shader_io::InstanceInfo &GetInstanceInfo(uint32_t index) const { return m_instanceInfo[index]; }

    [[nodiscard]] uint32_t GetSkyboxBindlessIndex() const { return m_skyboxSrv.GetIndex(); }

    [[nodiscard]] uint32_t GetIrradianceBindlessIndex() const { return m_irradianceSrv.GetIndex(); }

    [[nodiscard]] uint32_t GetSpecularBindlessIndex() const { return m_specularSrv.GetIndex(); }

    [[nodiscard]] uint32_t GetBrdfLutBindlessIndex() const { return m_brdfLutSrv.GetIndex(); }

protected:
    ResourceManager()  = default;
    ~ResourceManager() = default;

private:
    // GLTF data
    std::vector<DXBuffer>             m_gltfBuffers;
    std::vector<DXShaderResourceView> m_gltfBufferSrvs;

    // Mesh
    std::unordered_map<Key, uint32_t> m_meshLookup;
    std::vector<Mesh>                 m_meshes;
    std::vector<shader_io::MeshInfo>  m_meshInfos;
    DXBuffer                          m_meshInfoBuffer;
    DXShaderResourceView              m_meshInfoBufferSrv;

    // Texture
    std::unordered_map<Key, uint32_t> m_textureLookup;
    std::vector<DXTexture>            m_textures;
    std::vector<DXShaderResourceView> m_textureSrvs;

    // Material
    std::unordered_map<Key, uint32_t>    m_materialLookup;
    std::vector<shader_io::MaterialInfo> m_materialInfo;
    std::vector<std::string>             m_materialKeys;
    DXBuffer                             m_materialInfoBuffer;
    DXShaderResourceView                 m_materialInfoBufferSrv;

    // Instance
    std::unordered_map<Key, uint32_t>    m_instanceLookup;
    std::vector<shader_io::InstanceInfo> m_instanceInfo;
    DXBuffer                             m_instanceInfoBuffer;
    DXShaderResourceView                 m_instanceInfoBufferSrv;

    // Skybox / IBL
    DXTexture            m_skyboxTexture;
    DXShaderResourceView m_skyboxSrv;
    DXTexture            m_irradianceTexture;
    DXShaderResourceView m_irradianceSrv;
    DXTexture            m_specularTexture;
    DXShaderResourceView m_specularSrv;
    DXTexture            m_brdfLutTexture;
    DXShaderResourceView m_brdfLutSrv;

    std::mutex m_meshMutex{};
    std::mutex m_textureMutex{};

    void LoadGltf(DXApp &state, const std::string &fileName);

    // Scene walk
    void EmitNodeInstances(
        const fastgltf::Asset                          &asset,
        size_t                                          nodeIndex,
        const DirectX::XMMATRIX                        &parentTransform,
        const std::vector<std::vector<uint32_t>>       &meshHandlesPerGltfMesh,
        const std::vector<std::vector<int>>            &primMaterialIndicesPerGltfMesh,
        const std::vector<uint32_t>                    &gltfMaterialToManagerHandle
    );
};
