//
// Created by y1 on 2026-05-14.
//

#pragma once

#include <span>

#include <directx/d3dx12.h>
#include <directxmath.h>

#include "DXApp.h"
#include "DXTexture.h"
#include "Pass.h"

#include "global_io.slang"

class ShadowPass : public Pass {
public:
    ShadowPass(
        DXApp                                     &dxApp,
        std::string_view                           inputFile,
        const DXTexture                           &shadowMap,
        int32_t                                    shadowMapDsvOffset,
        uint32_t                                   shadowMapSize,
        const shader_io::ShadowUniforms           &shadowUniforms
    );

protected:
    void TransitionBarriers(const DrawContext &context) override;
    void BindResources(const DrawContext &context) override;
    void Record(const DrawContext &context) override;
    void FinalizeBarriers(const DrawContext &context) override;

private:
    const DXTexture                            &m_shadowMap;
    int32_t                                     m_shadowMapDsvOffset;
    CD3DX12_VIEWPORT                            m_viewport;
    CD3DX12_RECT                                m_scissor;
    const shader_io::ShadowUniforms            &m_shadowUniforms;
};
