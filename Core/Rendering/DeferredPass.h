//
// Created by y1 on 2026-05-14.
//

#pragma once

#include <span>

#include "DXApp.h"
#include "DXTexture.h"
#include "Pass.h"

#include "global_io.slang"

class DeferredPass : public Pass {
public:
    DeferredPass(
        DXApp                             &dxApp,
        std::string_view                   inputFile,
        std::span<const DXTexture>         gbufferTextures,
        const DXTexture                   &deferredTexture,
        uint32_t                           width,
        uint32_t                           height,
        const shader_io::DeferredUniforms &deferredUniforms
    );

protected:
    void TransitionBarriers(const DrawContext &context) override;
    void BindResources(const DrawContext &context) override;
    void Record(const DrawContext &context) override;

private:
    std::span<const DXTexture>         m_gbufferTextures;
    const DXTexture                   &m_deferredTexture;
    uint32_t                           m_width;
    uint32_t                           m_height;
    const shader_io::DeferredUniforms &m_deferredUniforms;
};
