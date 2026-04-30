//
// Created by y1 on 2026-04-26.
//

#include "ShaderCompiler.h"

#include <chrono>
#include <filesystem>

#include <simdjson.h>
#include <slang.h>

#include "Debug.h"
#include "JsonFile.h"

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
    for (SlangUInt i = 0; i < entryPointCount; ++i) {
        Slang::ComPtr<ISlangBlob> source;
        result           = linkedProgram->getEntryPointCode(static_cast<SlangInt>(i), 0, source.writeRef(), diagnostics.writeRef());
        SlangStage stage = layout->getEntryPointByIndex(i)->getStage();
        entrySources.emplace(stage, source);
    }


    const float totalMs = std::chrono::duration<float, std::milli>(clock::now() - overallStart).count();
    DebugInfo("Compile timings for {}: load={:.1f}ms compose={:.1f}ms link={:.1f}ms total={:.1f}ms", filePath, loadMs, composeMs, linkMs, totalMs);

    m_entryPoints.emplace(filePath, entrySources);
    m_shaderPrograms.emplace(filePath, linkedProgram);

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