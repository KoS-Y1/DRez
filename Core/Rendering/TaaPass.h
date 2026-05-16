//
// Created by y1 on 2026-05-16.
//

#pragma once

#include <span>

#include "Pass.h"

#include "global_io.slang"

class TaaPass : public Pass {
public:
    TaaPass(
        DXApp                        &dxApp,
        std::string_view              inputFile,
        std::span<const DXTexture>    taaTextures,
        const DXTexture              &deferredTexture,
        const DXTexture              &velocityTexture,
        const DXTexture              &depthTexture,
        uint32_t                      width,
        uint32_t                      height,
        const shader_io::TaaUniforms &taaUniforms
    );

protected:
    void TransitionBarriers(const DrawContext &context) override;
    void BindResources(const DrawContext &context) override;
    void Record(const DrawContext &context) override;

private:
    std::span<const DXTexture> m_taaTextures;
    const DXTexture           &m_deferredTexture;
    const DXTexture           &m_velocityTexture;
    const DXTexture           &m_depthTexture;

    uint32_t m_width;
    uint32_t m_height;

    const shader_io::TaaUniforms m_taaUniforms;
};
