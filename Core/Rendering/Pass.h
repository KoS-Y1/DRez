//
// Created by y1 on 2026-05-14.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include <directx/d3d12.h>

#include "DXApp.h"
#include "DXComputePipeline.h"
#include "DXGraphicsPipeline.h"
#include "DXTimestamps.h"

// Per-frame context threaded through every pass
struct DrawContext {
    ID3D12GraphicsCommandList *commandList{};
    DXTimestamps              *timestamps{};
    uint32_t                   frameIndex{};
};

enum class PipelineType : uint8_t {
    Graphics,
    Compute,
};

class Pass {
public:
    Pass(const Pass &)            = delete;
    Pass &operator=(const Pass &) = delete;
    Pass(Pass &&)                 = delete;
    Pass &operator=(Pass &&)      = delete;
    virtual ~Pass()               = default;

    void Execute(const DrawContext &context);

protected:
    Pass(DXApp &dxApp, std::string_view inputFile);

    // Resource state transitions before the pass body
    virtual void TransitionBarriers(const DrawContext &context) = 0;
    // Root constants / descriptor tables / render targets / vertex+index buffers / viewport+scissor
    virtual void BindResources(const DrawContext &context) = 0;
    // Draw or Dispatch
    virtual void Record(const DrawContext &context) = 0;
    // Optional post-work transitions (e.g. return write-target back to a read state)
    virtual void FinalizeBarriers(const DrawContext &context) {}

    DXApp       &m_app;
    std::string  m_name;
    PipelineType m_type;

private:
    void BindPipeline(ID3D12GraphicsCommandList *commandList);

    std::variant<DXGraphicsPipeline, DXComputePipeline> m_pipeline;
};
