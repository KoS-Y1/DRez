//
// Created by y1 on 2026-05-14.
//

#pragma once

#include <array>
#include <vector>

#include <directx/d3dx12.h>

#include "DXApp.h"
#include "DXTexture.h"
#include "Pass.h"

#include "global_io.slang"

class GbufferPass : public Pass {
public:
    GbufferPass(
        DXApp                                                                  &dxApp,
        std::string_view                                                        inputFile,
        const std::vector<DXTexture>                                           &gbufferTextures,
        const std::vector<int32_t>                                             &gbufferRtvOffsets,
        int32_t                                                                 depthDsvOffset,
        uint32_t                                                                width,
        uint32_t                                                                height,
        const std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> &globalUniforms
    );

protected:
    void TransitionBarriers(const DrawContext &context) override;
    void BindResources(const DrawContext &context) override;
    void Record(const DrawContext &context) override;

private:
    const std::vector<DXTexture>                                           &m_gbufferTextures;
    const std::vector<int32_t>                                             &m_gbufferRtvOffsets;
    int32_t                                                                 m_depthDsvOffset;
    CD3DX12_VIEWPORT                                                        m_viewport;
    CD3DX12_RECT                                                            m_scissor;
    const std::array<shader_io::GlobalUniforms, DXApp::kMaxFramesInFlight> &m_globalUniforms;
};
