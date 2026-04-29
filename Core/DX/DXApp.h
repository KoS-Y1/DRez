//
// Created by y1 on 2026-04-26.
//

#pragma once

#include "DXGraphicsPipeline.h"

#include <array>
#include <mutex>

#include <directx/d3d12.h>
#include <directx/d3dx12_core.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

// TODO: delete following
#include "DXBuffer.h"

class DXApp {
public:
    static constexpr uint32_t kMaxFramesInFlight{2};

    struct FrameInfo {
        ID3D12GraphicsCommandList *commandList;
    };

public:
    DXApp() = delete;
    explicit DXApp(HWND hwnd);

    DXApp(const DXApp &)            = delete;
    DXApp &operator=(const DXApp &) = delete;
    DXApp(DXApp &&)                 = delete;
    DXApp &operator=(DXApp &&)      = delete;

    ~DXApp();

    FrameInfo BeginFrame();
    void      EndFrame();

    // TODO: remove later
    void Run();

    [[nodiscard]] ID3D12Device *GetDevice() const { return m_device.Get(); }

public:
    [[nodiscard]] DXGraphicsPipeline CreateGraphicsPipeline(std::string_view inputFile) { return DXGraphicsPipeline(*this, inputFile); }

private:
    static constexpr DXGI_FORMAT kPresentFormat{DXGI_FORMAT_R8G8B8A8_UNORM};
    static constexpr uint64_t    kPointOneSecond{100000000};

    Microsoft::WRL::ComPtr<ID3D12Device>                                              m_device{};
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>                                        m_commandQueue{};
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kMaxFramesInFlight>    m_commandAllocators{};
    std::array<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>, kMaxFramesInFlight> m_commandLists{};

    uint32_t m_frameIndex{0};

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_immediateCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_immediateCommandList;

    void ResetCommand(ID3D12CommandAllocator *commandAllocator, ID3D12GraphicsCommandList *commandList);

    uint64_t SignalQueue();
    void     SubmitToQueue(ID3D12GraphicsCommandList *commandList);

    template<class Func>
    void ImmediateSubmit(Func &&func) {
        std::scoped_lock<std::mutex> lock(m_immediateMutes);

        ResetCommand(m_immediateCommandAllocator.Get(), m_immediateCommandList.Get());

        func(m_immediateCommandList.Get());

        SubmitToQueue(m_immediateCommandList.Get());

        WaitForFence(SignalQueue());
    }

private:
    // Swapchain
    Microsoft::WRL::ComPtr<IDXGISwapChain3>                                m_swapchain{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>                           m_rtvHeap{};
    uint32_t                                                               m_rtvDescriptorSize{};
    uint32_t                                                               m_backBufferIndex{};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> m_renderTargets{};

    void AcquireNextFrame();

private:
    // Synchronization
    std::array<uint64_t, kMaxFramesInFlight> m_fenceValues{};
    uint64_t                                 m_nextFenceValue{0};
    Microsoft::WRL::ComPtr<ID3D12Fence>      m_fence{};
    HANDLE                                   m_fenceEvent{};

    std::mutex m_immediateMutes{};

    void WaitForFence(uint64_t fenceValue, uint64_t timeout = kPointOneSecond);

    // TODO: testing
    DXGraphicsPipeline       m_gfx;
    CD3DX12_VIEWPORT         m_viewport{};
    CD3DX12_RECT             m_rect{};
    DXBuffer                 m_vertexBuffer{};
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
};
