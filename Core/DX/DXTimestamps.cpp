//
// Created by y1 on 2026-05-14.
//

#include "DXTimestamps.h"

#include <directx/d3dx12.h>

#include "DXDebug.h"
#include "Debug.h"

DXTimestamps::DXTimestamps(DXApp &app, uint32_t maxPasses)
    : m_maxPasses(maxPasses)
    , m_queriesPerFrame(maxPasses * kQueriesPerPass) {
    const uint32_t totalQueries = m_queriesPerFrame * DXApp::kMaxFramesInFlight;

    // Query heap
    const D3D12_QUERY_HEAP_DESC heapDesc{
        .Type  = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
        .Count = totalQueries,
    };
    HRESULT result = app.GetDevice()->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap));
    DebugCheckCritical(SUCCEEDED(result), "Failed to create timestamp query heap, error 0x{:x}", static_cast<uint32_t>(result));
    drez::dx::debug::SetObjectName(m_queryHeap.Get(), "timestamp_query_heap");

    // Readback buffer
    const uint64_t                bufferSize = totalQueries * sizeof(uint64_t);
    const CD3DX12_HEAP_PROPERTIES heapProps{D3D12_HEAP_TYPE_READBACK};
    const auto                    desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    result                             = app.GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_readback)
    );
    DebugCheckCritical(SUCCEEDED(result), "Failed to create timestamp readback buffer, error 0x{:x}", static_cast<uint32_t>(result));
    drez::dx::debug::SetObjectName(m_readback.Get(), "timestamp_readback_buffer");

    void             *mapped{};
    const D3D12_RANGE readRange{0, bufferSize};
    result = m_readback->Map(0, &readRange, &mapped);
    DebugCheckCritical(SUCCEEDED(result), "Failed to map timestamp readback buffer, error 0x{:x}", static_cast<uint32_t>(result));
    m_mappedData = static_cast<const uint64_t *>(mapped);

    // Frequency
    result = app.GetCommandQueue()->GetTimestampFrequency(&m_frequency);
    DebugCheckCritical(SUCCEEDED(result), "Failed to get timestamp frequency, error 0x{:x}", static_cast<uint32_t>(result));
}

void DXTimestamps::BeginFrame(uint32_t frameIndex) {
    m_frameIndex      = frameIndex;
    m_passesThisFrame = 0;
    m_currentPassNames.clear();
    m_timings.clear();

    // Read back previous results for this slot (resolved kMaxFramesInFlight frames ago, fence-completed by now)
    if (!m_slotValid[frameIndex]) {
        return;
    }

    const std::vector<std::string> &names      = m_slotPassNames[frameIndex];
    const uint64_t                 *slotData   = m_mappedData + frameIndex * m_queriesPerFrame;
    const double                    msPerTick  = 1000.0 / static_cast<double>(m_frequency);

    m_timings.reserve(names.size());
    for (uint32_t i = 0; i < names.size(); ++i) {
        const uint64_t begin = slotData[i * kQueriesPerPass];
        const uint64_t end   = slotData[i * kQueriesPerPass + 1];
        const float    ms    = static_cast<float>(static_cast<double>(end - begin) * msPerTick);
        m_timings.push_back({names[i], ms});
    }
}

void DXTimestamps::EndFrame(ID3D12GraphicsCommandList *commandList) {
    const uint32_t queryOffset = m_frameIndex * m_queriesPerFrame;
    const uint32_t queryCount  = m_passesThisFrame * kQueriesPerPass;
    if (queryCount == 0) {
        return;
    }

    commandList->ResolveQueryData(
        m_queryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        queryOffset,
        queryCount,
        m_readback.Get(),
        queryOffset * sizeof(uint64_t)
    );

    m_slotPassNames[m_frameIndex] = m_currentPassNames;
    m_slotValid[m_frameIndex]     = true;
}

void DXTimestamps::BeginPass(ID3D12GraphicsCommandList *commandList, std::string_view name) {
    if (m_passesThisFrame >= m_maxPasses) {
        return;
    }
    const uint32_t queryIdx = m_frameIndex * m_queriesPerFrame + m_passesThisFrame * kQueriesPerPass;
    commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIdx);
    m_currentPassNames.emplace_back(name);
}

void DXTimestamps::EndPass(ID3D12GraphicsCommandList *commandList) {
    if (m_passesThisFrame >= m_maxPasses) {
        return;
    }
    const uint32_t queryIdx = m_frameIndex * m_queriesPerFrame + m_passesThisFrame * kQueriesPerPass + 1;
    commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIdx);
    ++m_passesThisFrame;
}
