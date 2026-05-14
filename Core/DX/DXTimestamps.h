//
// Created by y1 on 2026-05-14.
//

#pragma once

#include <array>
#include <string>
#include <vector>

#include <directx/d3d12.h>
#include <wrl/client.h>

#include "DxApp.h"

class DXTimestamps {
public:
    static constexpr uint32_t kQueriesPerPass = 2;

    struct PassTime {
        std::string name;
        float       milliseconds;
    };

    DXTimestamps() = default;
    DXTimestamps(DXApp &app, uint32_t maxPasses);

    DXTimestamps(const DXTimestamps &)            = delete;
    DXTimestamps &operator=(const DXTimestamps &) = delete;
    DXTimestamps(DXTimestamps &&)                 = default;
    DXTimestamps &operator=(DXTimestamps &&)      = default;

    ~DXTimestamps() = default;

    // Read previous results and reset per-frame state; call after BeginFrame's fence wait.
    void BeginFrame(uint32_t frameIndex);

    // Resolve this frame's queries into the readback buffer; call before submitting the command list.
    void EndFrame(ID3D12GraphicsCommandList *commandList);

    void BeginPass(ID3D12GraphicsCommandList *commandList, std::string_view name);
    void EndPass(ID3D12GraphicsCommandList *commandList);

    [[nodiscard]] const std::vector<PassTime> &GetTimings() const { return m_timings; }

private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap{};
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_readback{};
    const uint64_t                         *m_mappedData{};
    uint64_t                                m_frequency{};

    uint32_t m_maxPasses{};
    uint32_t m_queriesPerFrame{};
    uint32_t m_frameIndex{};
    uint32_t m_passesThisFrame{};

    std::vector<std::string>                                        m_currentPassNames{};
    std::array<std::vector<std::string>, DXApp::kMaxFramesInFlight> m_slotPassNames{};
    std::array<bool, DXApp::kMaxFramesInFlight>                     m_slotValid{};

    std::vector<PassTime> m_timings{};
};
