//
// Created by y1 on 2026-04-26.
//

#pragma once


#include <array>
#include <atomic>
#include <mutex>

#include <directx/d3d12.h>
#include <dxgi1_4.h>
#include <imgui_impl_dx12.h>
#include <wrl/client.h>

#include "DXBuffer.h"
#include "DXComputePipeline.h"
#include "DXGraphicsPipeline.h"
#include "DXShaderResourceView.h"
#include "DXTexture.h"
#include "DXUnorderedAccessView.h"

#include <directx/d3dx12_root_signature.h>
#include <vector>

struct SDL_Window;

struct FrameInfo {
    ID3D12GraphicsCommandList *commandList;
    const uint32_t             frameIndex;
};

class DXApp {
public:
    static constexpr uint32_t kMaxFramesInFlight{2};

public:
    DXApp() = delete;
    explicit DXApp(SDL_Window *window);

    DXApp(const DXApp &)            = delete;
    DXApp &operator=(const DXApp &) = delete;
    DXApp(DXApp &&)                 = delete;
    DXApp &operator=(DXApp &&)      = delete;

    ~DXApp();

    FrameInfo BeginFrame();
    void      EndFrame();
    void      CopyToPresentImage(ID3D12Resource *resource);

    void WaitForGpu() { WaitForFence(SignalQueue()); }

    template<class Func>
    void ImmediateSubmit(Func &&func) {
        std::scoped_lock<std::mutex> lock(m_immediateMutes);

        ResetCommand(m_immediateCommandAllocator.Get(), m_immediateCommandList.Get());

        func(m_immediateCommandList.Get());

        SubmitToQueue(m_immediateCommandList.Get());

        WaitForFence(SignalQueue());
    }

    [[nodiscard]] uint32_t GetWindowWidth() const { return m_width; }

    [[nodiscard]] uint32_t GetWindowHeight() const { return m_height; }

    [[nodiscard]] ID3D12Device *GetDevice() const { return m_device.Get(); }

    [[nodiscard]] ID3D12CommandQueue *GetCommandQueue() const { return m_commandQueue.Get(); }

    [[nodiscard]] ID3D12DescriptorHeap *GetImGuiDescriptorHeap() const { return m_imguiHeap.heap.Get(); }

public:
    [[nodiscard]] DXGraphicsPipeline CreateGraphicsPipeline(std::string_view inputFile) { return DXGraphicsPipeline(*this, inputFile); }

    [[nodiscard]] DXComputePipeline CreateComputePipeline(std::string_view name) { return DXComputePipeline(*this, name); }

    [[nodiscard]] DXBuffer CreateBuffer(
        D3D12_HEAP_TYPE       heapType,
        D3D12_HEAP_FLAGS      flags,
        D3D12_RESOURCE_STATES states,
        uint64_t              size,
        std::string           name
    ) {
        return DXBuffer{*this, heapType, flags, states, size, name};
    }

    [[nodiscard]] DXTexture CreateTexture(
        uint64_t               width,
        uint32_t               height,
        DXGI_FORMAT            format,
        D3D12_RESOURCE_FLAGS   resourceFlags,
        D3D12_HEAP_FLAGS       heapFlags,
        shader_io::SamplerType samplerType,
        std::string            name,
        DXGI_FORMAT            clearFormat = DXGI_FORMAT_UNKNOWN,
        uint16_t               mipLevels   = 1
    ) {
        return DXTexture{*this, width, height, format, resourceFlags, heapFlags, samplerType, name, clearFormat, mipLevels};
    }

    [[nodiscard]] DXShaderResourceView CreateDXShaderResourceView(ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC &desc) {
        return DXShaderResourceView{*this, resource, desc};
    }

    [[nodiscard]] DXUnorderedAccessView CreateDXUnorderedAccessView(ID3D12Resource *resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc) {
        return DXUnorderedAccessView{*this, resource, desc};
    }

public:
    // Resource
    void CreateRenderTargetView(ID3D12Resource *resource, int32_t index);
    void CreateDepthStencilView(ID3D12Resource *resource, int32_t index);
    void CreateDepthStencilView(ID3D12Resource *resource, int32_t index, const D3D12_DEPTH_STENCIL_VIEW_DESC &desc);
    void CreateShaderResourceView(ID3D12Resource *resource, int32_t index, const D3D12_SHADER_RESOURCE_VIEW_DESC &desc);
    void CreateUnorderedAccessView(ID3D12Resource *resource, int32_t index, const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc);

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetViewHandle(int32_t index);
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewHandle(int32_t index);
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(int32_t index);

    uint32_t AllocateBindlessIndex() { return m_nextBindlessIndex.fetch_add(1, std::memory_order_relaxed); }

public:
    // Batch upload texture
    void                   BeginBatchUpload(uint64_t totalBytes);
    void                   BatchedTextureUpload(const DXTexture &texture, const void *data);
    void                   BatchedTextureFlush();
    [[nodiscard]] uint64_t GetTextureUploadSize(uint64_t width, uint32_t height, DXGI_FORMAT format) const;

    void GenerateMipmaps(DXTexture &texture, uint32_t srcSrvIndex);

private:
    static constexpr uint64_t    kPointOneSecond{100000000};
    static constexpr DXGI_FORMAT kPresentFormat{DXGI_FORMAT_R8G8B8A8_UNORM};

    SDL_Window *m_window{};
    uint32_t    m_width{};
    uint32_t    m_height{};

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


private:
    // Resources
    static constexpr uint32_t kMaxRenderTargets = 16;
    static constexpr uint32_t kMaxDepthStencils = 2;

    Microsoft::WRL::ComPtr<IDXGISwapChain3>                                m_swapchain{};
    uint32_t                                                               m_backBufferIndex{};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> m_swapchainImages{};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap{};
    uint32_t                                     m_rtvDescriptorSize{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap{};
    uint32_t                                     m_dsvDescriptorSize{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap{};
    uint32_t                                     m_descriptorSize{};
    std::atomic<uint32_t>                        m_nextBindlessIndex{0};

    struct ImGuiHeap {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap{};
        uint32_t                                     descriptorSize{};
        std::vector<int32_t>                         freeIndices{};
        D3D12_CPU_DESCRIPTOR_HANDLE                  startCpu;
        D3D12_GPU_DESCRIPTOR_HANDLE                  startGpu;

        void Allocate(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *gpuHandle);
        void Free(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
    };

    ImGuiHeap m_imguiHeap{};

    bool m_resourcesBound{false};

    void AcquireNextFrame();

private:
    // Synchronization
    std::array<uint64_t, kMaxFramesInFlight> m_fenceValues{};
    uint64_t                                 m_nextFenceValue{0};
    Microsoft::WRL::ComPtr<ID3D12Fence>      m_fence{};
    HANDLE                                   m_fenceEvent{};

    std::mutex m_immediateMutes{};

    void WaitForFence(uint64_t fenceValue, uint64_t timeout = kPointOneSecond);

private:
    // Batch upload
    struct PendingCopy {
        ID3D12Resource                    *dstResource{};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    };

    DXBuffer                 m_uploadBuffer{};
    std::atomic<uint64_t>    m_uploadOffset{0};
    std::mutex               m_batchUploadMutex{};
    std::vector<PendingCopy> m_pendingCopies{};

    // Mipmap generation
    DXComputePipeline m_mipmapPipeline{};
};
