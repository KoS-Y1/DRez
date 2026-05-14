//
// Created by y1 on 2026-05-14.
//

#pragma once

#include "DXApp.h"
#include "DXTexture.h"
#include "Pass.h"

#include "global_io.slang"

class BlitPass : public Pass {
public:
    BlitPass(
        DXApp                          &dxApp,
        std::string_view                inputFile,
        const DXTexture                &deferredTexture,
        const DXTexture                &composedTexture,
        uint32_t                        width,
        uint32_t                        height,
        const shader_io::BlitUniforms  &blitUniforms
    );

protected:
    void TransitionBarriers(const DrawContext &context) override;
    void BindResources(const DrawContext &context) override;
    void Record(const DrawContext &context) override;

private:
    const DXTexture               &m_deferredTexture;
    const DXTexture               &m_composedTexture;
    uint32_t                       m_width;
    uint32_t                       m_height;
    const shader_io::BlitUniforms &m_blitUniforms;
};
