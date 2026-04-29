//
// Created by y1 on 2026-04-26.
//

#pragma once

#include <string>
#include <unordered_map>

#include <slang-com-ptr.h>

#include "Singleton.h"

class ShaderCompiler : public Singleton<ShaderCompiler> {
public:
    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler &)            = delete;
    ShaderCompiler(ShaderCompiler &&)                 = delete;
    ShaderCompiler &operator=(const ShaderCompiler &) = delete;
    ShaderCompiler &operator=(ShaderCompiler &&)      = delete;

    ~ShaderCompiler() = default;

    [[nodiscard]] const std::unordered_map<SlangStage, Slang::ComPtr<ISlangBlob>> &GetEntryPoints(const std::string &filePath) const;


private:
    static constexpr const char *kShaderSearchPath{"../Assets/Shaders/"};
    static constexpr const char *kLoadFile{"../Assets/Shaders/Shaders.json"};

    Slang::ComPtr<slang::IGlobalSession> m_globalSession{};
    Slang::ComPtr<slang::ISession>       m_session{};

    std::unordered_map<std::string, std::unordered_map<SlangStage, Slang::ComPtr<ISlangBlob>>> m_entryPoints{};
    std::unordered_map<std::string, Slang::ComPtr<slang::IComponentType>>                      m_shaderPrograms{};

    // TODO: extract signature

    std::string m_diagnosticMessage;

    bool Compile(const std::string &filePath);

    void LogAndAppendDiagnostics(slang::IBlob *diagnostics);
};
