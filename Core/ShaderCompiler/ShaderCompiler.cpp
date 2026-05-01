//
// Created by y1 on 2026-04-26.
//

#include "ShaderCompiler.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>

#include <simdjson.h>
#include <slang.h>

#include "Debug.h"
#include "JsonFile.h"

namespace {
constexpr uint64_t kDefaultArrayDescriptorCount = 1024;

using DescriptorRangeTypeParser = std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> (*)(slang::VariableLayoutReflection *);

const std::unordered_map<SlangResourceAccess, D3D12_DESCRIPTOR_RANGE_TYPE> DescriptorRangeAccessMap{
    {SLANG_RESOURCE_ACCESS_READ,       D3D12_DESCRIPTOR_RANGE_TYPE_SRV},
    {SLANG_RESOURCE_ACCESS_READ_WRITE, D3D12_DESCRIPTOR_RANGE_TYPE_UAV},
};

const std::unordered_map<slang::TypeReflection::Kind, DescriptorRangeTypeParser> DescriptorRangeTypeMap{
    {slang::TypeReflection::Kind::ConstantBuffer,
     [](slang::VariableLayoutReflection *) -> std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> {
         return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
     }                                                                                                                                                       },
    {slang::TypeReflection::Kind::SamplerState,
     [](slang::VariableLayoutReflection *) -> std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> { return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; }                     },
    {slang::TypeReflection::Kind::Resource,
     [](slang::VariableLayoutReflection *variable) -> std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> {
         slang::TypeLayoutReflection *typeLayout = variable->getTypeLayout();
         const auto                   pair       = DescriptorRangeAccessMap.find(typeLayout->getResourceAccess());
         if (pair == DescriptorRangeAccessMap.end()) {
             return std::nullopt;
         }
         return pair->second;
     }                                                                                                                                                       },
    {slang::TypeReflection::Kind::Array,
     [](slang::VariableLayoutReflection *variable) -> std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> {
         slang::TypeReflection            *type = variable->getType();
         const slang::TypeReflection::Kind kind = type->getElementType()->getKind();
         const auto                        pair = DescriptorRangeTypeMap.find(kind);
         if (pair == DescriptorRangeTypeMap.end()) {
             return std::nullopt;
         }
         return pair->second(variable);
     }                                                                                                                                                       },
};

std::optional<CD3DX12_DESCRIPTOR_RANGE1> ParseDescriptorRange(slang::VariableLayoutReflection *variable) {
    slang::TypeLayoutReflection * const layout = variable->getTypeLayout();
    const slang::TypeReflection::Kind   kind   = layout->getKind();

    const auto pair = DescriptorRangeTypeMap.find(kind);
    if (pair == DescriptorRangeTypeMap.end()) {
        return std::nullopt;
    }
    const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> rangeType = pair->second(variable);
    if (!rangeType.has_value()) {
        return std::nullopt;
    }

    uint32_t descriptorCount = 1;
    if (kind == slang::TypeReflection::Kind::Array) {
        const size_t elementCount = variable->getType()->getElementCount();
        descriptorCount           = static_cast<uint32_t>(std::max<uint64_t>(kDefaultArrayDescriptorCount, elementCount));
    }

    const slang::ParameterCategory category     = variable->getCategory();
    const uint32_t                 bindingIndex = variable->getBindingIndex();
    const uint32_t                 spaceIndex   = variable->getBindingSpace(category);

    CD3DX12_DESCRIPTOR_RANGE1 range{};
    range.Init(rangeType.value(), descriptorCount, bindingIndex, spaceIndex);
    return range;
}

D3D12_SHADER_VISIBILITY ShaderStageSlangToD3D12Visibility(SlangStage stage) {
    switch (stage) {
    case SLANG_STAGE_VERTEX:
        return D3D12_SHADER_VISIBILITY_VERTEX;
    case SLANG_STAGE_FRAGMENT:
        return D3D12_SHADER_VISIBILITY_PIXEL;
    case SLANG_STAGE_GEOMETRY:
        return D3D12_SHADER_VISIBILITY_GEOMETRY;
    case SLANG_STAGE_HULL:
        return D3D12_SHADER_VISIBILITY_HULL;
    case SLANG_STAGE_DOMAIN:
        return D3D12_SHADER_VISIBILITY_DOMAIN;
    case SLANG_STAGE_AMPLIFICATION:
        return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
    case SLANG_STAGE_MESH:
        return D3D12_SHADER_VISIBILITY_MESH;
    default:
        return D3D12_SHADER_VISIBILITY_ALL;
    }
}

D3D12_SHADER_VISIBILITY ResolveVisibility(
    const std::vector<Slang::ComPtr<slang::IMetadata>> &entryMetadata,
    const std::vector<SlangStage>                      &entryStages,
    slang::ParameterCategory                            category,
    uint32_t                                            spaceIndex,
    uint32_t                                            bindingIndex
) {
    SlangStage usedStage = SLANG_STAGE_NONE;
    unsigned   usedCount = 0;
    for (size_t i = 0; i < entryMetadata.size(); ++i) {
        if (!entryMetadata[i]) {
            continue;
        }
        bool used = false;
        entryMetadata[i]->isParameterLocationUsed(static_cast<SlangParameterCategory>(category), spaceIndex, bindingIndex, used);
        if (used) {
            usedStage = entryStages[i];
            ++usedCount;
        }
    }
    if (usedCount == 1) {
        return ShaderStageSlangToD3D12Visibility(usedStage);
    }
    return D3D12_SHADER_VISIBILITY_ALL;
}

} // namespace

ShaderCompiler::ShaderCompiler() {
    slang::createGlobalSession(m_globalSession.writeRef());

    const slang::TargetDesc target{
        .format  = SLANG_DXIL,
        .profile = m_globalSession->findProfile("sm_6_3"),
    };

#if defined(NDEBUG)
    constexpr int kDebugInfo = 0;
#else
    constexpr int kDebugInfo = 1;
#endif

    std::vector<slang::CompilerOptionEntry> options{
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, kDebugInfo}},
    };

    const std::vector<slang::PreprocessorMacroDesc> macros{
        {.name = "SLANG_PUBLIC", .value = "public"},
    };

    const slang::SessionDesc session{
        .targets                  = &target,
        .targetCount              = 1,
        .searchPaths              = &kShaderSearchPath,
        .searchPathCount          = 1,
        .preprocessorMacros       = macros.data(),
        .preprocessorMacroCount   = static_cast<uint32_t>(macros.size()),
        .compilerOptionEntries    = options.data(),
        .compilerOptionEntryCount = static_cast<uint32_t>(options.size()),
    };
    m_globalSession->createSession(session, m_session.writeRef());

    // Preload every shader
    {
        const JsonFile json{kLoadFile};
        const auto     keys = json.Get<std::vector<std::string>>("shaders");

        for (const auto &key: keys) {
            DebugCheckCritical(Compile(key), "Shader Compiler preloading, failed to load {}", key);
        }
    }
}

const std::unordered_map<SlangStage, Slang::ComPtr<ISlangBlob>> &ShaderCompiler::GetEntryPoints(const std::string &filePath) const {
    const auto pair = m_entryPoints.find(filePath);
    DebugCheckCritical(pair != m_entryPoints.end(), "Failed to get entry points from {}", filePath);
    return pair->second;
}

std::vector<CD3DX12_ROOT_PARAMETER1> ShaderCompiler::GetRootParameters(const std::string &filePath) const {
    const auto pair = m_rootParameters.find(filePath);
    DebugCheckCritical(pair != m_rootParameters.end(), "Failed to get root parameters from {}", filePath);
    return pair->second;
}

bool ShaderCompiler::Compile(const std::string &filePath) {
    DebugInfo("Loading shader from file {}", filePath);
    m_diagnosticMessage.clear();

    using clock             = std::chrono::steady_clock;
    const auto overallStart = clock::now();
    auto       phaseStart   = overallStart;
    auto       lapMs        = [&phaseStart]() {
        const auto  now = clock::now();
        const float ms  = std::chrono::duration<float, std::milli>(now - phaseStart).count();
        phaseStart      = now;
        return ms;
    };

    std::filesystem::path path{filePath};
    const std::string     fileName    = path.filename().string();
    const std::string     moduleName  = path.stem().string();
    const std::string     slangSource = drez::file_system::Read(filePath);

    Slang::ComPtr<slang::IBlob> diagnostics;

    // Load module
    Slang::ComPtr<slang::IModule> module;
    module = m_session->loadModuleFromSourceString(moduleName.c_str(), fileName.c_str(), slangSource.c_str(), diagnostics.writeRef());
    LogAndAppendDiagnostics(diagnostics);
    const float loadMs = lapMs();

    if (!module) {
        return false;
    }

    // Entry point shader reflection
    const SlangInt32                               definedEntryPointCount = module->getDefinedEntryPointCount();
    std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints(definedEntryPointCount);
    std::vector<slang::IComponentType *>           components;
    components.reserve(definedEntryPointCount + 1);
    components.push_back(module);
    for (SlangInt32 i = 0; i < definedEntryPointCount; ++i) {
        module->getDefinedEntryPoint(i, entryPoints[i].writeRef());
        components.push_back(entryPoints[i]);
    }

    // Compose modules with entry points
    Slang::ComPtr<slang::IComponentType> composedProgram;
    SlangResult                          result = m_session->createCompositeComponentType(
        components.data(),
        static_cast<int64_t>(components.size()),
        composedProgram.writeRef(),
        diagnostics.writeRef()
    );
    LogAndAppendDiagnostics(diagnostics);
    const float composeMs = lapMs();
    if (SLANG_FAILED(result) || !composedProgram) {
        return false;
    }

    // From composite component to linked program
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    result = composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef());
    LogAndAppendDiagnostics(diagnostics);
    const float linkMs = lapMs();
    if (SLANG_FAILED(result) || !linkedProgram) {
        return false;
    }

    slang::ProgramLayout                                     *layout = linkedProgram->getLayout();
    std::unordered_map<SlangStage, Slang::ComPtr<ISlangBlob>> entrySources;
    const SlangUInt                                           entryPointCount = layout->getEntryPointCount();
    std::vector<Slang::ComPtr<slang::IMetadata>>              entryMetadata(entryPointCount);
    std::vector<SlangStage>                                   entryStages(entryPointCount);
    for (SlangUInt i = 0; i < entryPointCount; ++i) {
        Slang::ComPtr<ISlangBlob> source;
        result                 = linkedProgram->getEntryPointCode(static_cast<SlangInt>(i), 0, source.writeRef(), diagnostics.writeRef());
        const SlangStage stage = layout->getEntryPointByIndex(i)->getStage();
        entrySources.emplace(stage, source);

        linkedProgram->getEntryPointMetadata(static_cast<SlangInt>(i), 0, entryMetadata[i].writeRef(), diagnostics.writeRef());
        LogAndAppendDiagnostics(diagnostics);
        entryStages[i] = stage;
    }

    slang::VariableLayoutReflection * const globalVariableLayout = layout->getGlobalParamsVarLayout();
    slang::TypeLayoutReflection * const     globalTypeLayout     = globalVariableLayout->getTypeLayout();
    const uint32_t                          fieldCount           = globalTypeLayout->getFieldCount();

    std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;
    std::vector<CD3DX12_ROOT_PARAMETER1>   rootParameters;
    ranges.reserve(fieldCount);
    rootParameters.reserve(fieldCount);

    for (uint32_t i = 0; i < fieldCount; ++i) {
        slang::VariableLayoutReflection * const  variable = globalTypeLayout->getFieldByIndex(i);
        const std::optional<CD3DX12_DESCRIPTOR_RANGE1> range = ParseDescriptorRange(variable);
        if (!range.has_value()) {
            DebugError("Shader {} has unknown descriptor type at field {}.", filePath, i);
            continue;
        }

        const slang::ParameterCategory category     = variable->getCategory();
        const uint32_t                 bindingIndex = variable->getBindingIndex();
        const uint32_t                 spaceIndex   = variable->getBindingSpace(category);
        const D3D12_SHADER_VISIBILITY  visibility   = ResolveVisibility(entryMetadata, entryStages, category, spaceIndex, bindingIndex);

        ranges.push_back(range.value());
        CD3DX12_ROOT_PARAMETER1 rootParam{};
        rootParam.InitAsDescriptorTable(1, &ranges.back(), visibility);
        rootParameters.push_back(rootParam);
    }

    const float totalMs = std::chrono::duration<float, std::milli>(clock::now() - overallStart).count();
    DebugInfo("Compile timings for {}: load={:.1f}ms compose={:.1f}ms link={:.1f}ms total={:.1f}ms", filePath, loadMs, composeMs, linkMs, totalMs);

    m_entryPoints.emplace(filePath, std::move(entrySources));
    m_shaderPrograms.emplace(filePath, linkedProgram);
    m_descriptorRanges.emplace(filePath, std::move(ranges));
    m_rootParameters.emplace(filePath, std::move(rootParameters));

    return true;
}

void ShaderCompiler::LogAndAppendDiagnostics(slang::IBlob *diagnostics) {
    if (diagnostics) {
        const char *message = static_cast<const char *>(diagnostics->getBufferPointer());
        DebugInfo("\n{}\n", message);

        if (m_diagnosticMessage.empty()) {
            m_diagnosticMessage += '\n';
        }
        m_diagnosticMessage += message;
    }
}