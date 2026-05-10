//
// Created by y1 on 2026-04-26.
//

#include "DXApp.h"

#include <string>

#include <directx/d3dx12.h>
#include <dxgi.h>
#include <dxgi1_6.h>

#include "Debug.h"

// TODO: delete
#include "VertexFormats.h"


using Microsoft::WRL::ComPtr;

DXApp::DXApp(HWND hwnd) {
#if defined(_DEBUG)
    // Enable debug layer
    {
        ComPtr<ID3D12Debug> debugController;
        HRESULT             result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
        DebugCheckCritical(SUCCEEDED(result), "Failed to get debug interface, error 0x{:x}", static_cast<uint32_t>(result));
        debugController->EnableDebugLayer();

        ComPtr<ID3D12Debug1> debug1;
        if (SUCCEEDED(debugController.As(&debug1))) {
            debug1->SetEnableGPUBasedValidation(FALSE);
            debug1->SetEnableSynchronizedCommandQueueValidation(TRUE);
        }
    }
#endif

    ComPtr<IDXGIFactory1> factory1;
    HRESULT               result = CreateDXGIFactory1(IID_PPV_ARGS(&factory1));
    DebugCheckCritical(SUCCEEDED(result), "Failed to create factory 1, error 0x{:x}", static_cast<uint32_t>(result));

    // Create device
    {
        ComPtr<IDXGIFactory6> factory6;
        result = factory1->QueryInterface(IID_PPV_ARGS(&factory6));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create factory 6, error 0x{:x}", static_cast<uint32_t>(result));

        ComPtr<IDXGIAdapter1> adapter;
        for (uint32_t i = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); i++) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            // Skip WARP
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)))) {
                const int   len = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
                std::string adapterName(len > 0 ? len - 1 : 0, '\0');
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName.data(), len, nullptr, nullptr);
                DebugInfo("Using adapter {} ", adapterName);
                break;
            }
        }
        DebugCheckCritical(m_device, "Failed to create device");


        // Hook debug layer to logger
#if defined(_DEBUG)
        ComPtr<ID3D12InfoQueue1> infoQueue1;
        if (SUCCEEDED(m_device.As(&infoQueue1))) {
            DWORD cookie{0};
            infoQueue1->RegisterMessageCallback(
                [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void *) {
                    if (severity <= D3D12_MESSAGE_SEVERITY_ERROR)
                        DebugError("[D3D12] {}", description);
                    else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
                        DebugWarning("[D3D12] {}", description);
                    else
                        DebugInfo("[D3D12] {}", description);
                },
                D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                nullptr,
                &cookie
            );
            spdlog::flush_on(spdlog::level::err);
        }

        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_device.As(&infoQueue))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        }


#endif
    }


    // Command queue, per-frame allocators and command lists
    {
        const D3D12_COMMAND_QUEUE_DESC desc{.Type = D3D12_COMMAND_LIST_TYPE_DIRECT, .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE};
        result = m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create command queue, error 0x{:x}", static_cast<uint32_t>(result));

        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
            result = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]));
            DebugCheckCritical(SUCCEEDED(result), "Failed to create command allocator #{}, error 0x{:x}", i, static_cast<uint32_t>(result));

            result =
                m_device
                    ->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[i].Get(), nullptr, IID_PPV_ARGS(&m_commandLists[i]));
            DebugCheckCritical(SUCCEEDED(result), "Failed to create command list #{}, error 0x{:x}", i, static_cast<uint32_t>(result));

            result = m_commandLists[i]->Close();
            DebugCheckCritical(SUCCEEDED(result), "Failed to close command list #{}, error 0x{:x}", i, static_cast<uint32_t>(result));
        }

        // Immediate command allocator and list
        result = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_immediateCommandAllocator));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create immediate command allocator, error 0x{:x}", static_cast<uint32_t>(result));

        result = m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_immediateCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_immediateCommandList)
        );
        DebugCheckCritical(SUCCEEDED(result), "Failed to create command list, error 0x{:x}", static_cast<uint32_t>(result));

        result = m_immediateCommandList->Close();
        DebugCheckCritical(SUCCEEDED(result), "Failed to close immediate command list, error 0x{:x}", static_cast<uint32_t>(result));
    }

    // Synchronization objects
    {
        result = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create fence, error 0x{:x}", static_cast<uint32_t>(result));

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        DebugCheckCritical(m_fenceEvent, "Failed to create fence, error 0x{:x}", static_cast<uint32_t>(HRESULT_FROM_WIN32(GetLastError())));
    }

    // Get window extent
    {
        RECT rect;
        DebugCheckCritical(GetClientRect(hwnd, &rect), "Failed to get window extent");
        m_width  = static_cast<uint32_t>(rect.right - rect.left);
        m_height = static_cast<uint32_t>(rect.bottom - rect.top);
    }

    // Swapchain
    {
        const DXGI_MODE_DESC bufferDesc{.Width = m_width, .Height = m_height, .Format = kPresentFormat};

        DXGI_SWAP_CHAIN_DESC swapchainDesc{
            .BufferDesc   = bufferDesc,
            .SampleDesc   = {.Count = 1},
            .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount  = kMaxFramesInFlight,
            .OutputWindow = hwnd,
            .Windowed     = true,
            .SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD
        };

        ComPtr<IDXGISwapChain> swapchain{};
        result = factory1->CreateSwapChain(m_commandQueue.Get(), &swapchainDesc, &swapchain);
        DebugCheckCritical(SUCCEEDED(result), "Failed to create swapchain, error 0x{:x}", static_cast<uint32_t>(result));
        result = swapchain.As(&m_swapchain);
        DebugCheckCritical(SUCCEEDED(result), "Failed to return IDXGISwapChain3 swapchain, error 0x{:x}", static_cast<uint32_t>(result));

        // Disable fullscreen
        result = factory1->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        DebugCheckCritical(SUCCEEDED(result), "Failed to make window association, error 0x{:x}", static_cast<uint32_t>(result));
    }

    // Descriptor heaps
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = kRenderTargetCount,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };

        result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create render target view descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        result    = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create depth stencil view descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1024;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        result              = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Swapchain images
    {
        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
            result = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_swapchainImages[i]));
            DebugCheckCritical(SUCCEEDED(result), "Failed to get swapchain image #{}, error 0x{:x}", i, static_cast<uint32_t>(result));
        }
    }
}

DXApp::~DXApp() {
    CloseHandle(m_fenceEvent);
}

FrameInfo DXApp::BeginFrame() {
    WaitForFence(m_fenceValues[m_frameIndex]);
    ResetCommand(m_commandAllocators[m_frameIndex].Get(), m_commandLists[m_frameIndex].Get());

    AcquireNextFrame();

    const std::vector<ID3D12DescriptorHeap *> heaps{m_descriptorHeap.Get()};
    m_commandLists[m_frameIndex]->SetDescriptorHeaps(heaps.size(), heaps.data());

    return FrameInfo{.commandList = m_commandLists[m_frameIndex].Get(), .frameIndex = m_frameIndex};
}

void DXApp::EndFrame() {
    SubmitToQueue(m_commandLists[m_frameIndex].Get());

    HRESULT result = m_swapchain->Present(1, 0);
    DebugCheckCritical(SUCCEEDED(result), "Failed to present, error 0x{:x}", static_cast<uint32_t>(result));

    m_fenceValues[m_frameIndex] = SignalQueue();
    m_frameIndex                = (m_frameIndex + 1) % kMaxFramesInFlight;
}

void DXApp::CopyToPresentImage(ID3D12Resource *resource) {
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_swapchainImages[m_backBufferIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        m_commandLists[m_frameIndex]->ResourceBarrier(1, &barrier);
    }

    m_commandLists[m_frameIndex]->CopyResource(m_swapchainImages[m_backBufferIndex].Get(), resource);

    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_swapchainImages[m_backBufferIndex].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PRESENT
        );
        m_commandLists[m_frameIndex]->ResourceBarrier(1, &barrier);
    }
}

void DXApp::CreateRenderTargetView(ID3D12Resource *resource, const int32_t index) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetRenderTargetViewHandle(index);
    m_device->CreateRenderTargetView(resource, nullptr, rtvHandle);
}

void DXApp::CreateDepthStencilView(ID3D12Resource *resource, const int32_t index) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDepthStencilViewHandle(index);
    m_device->CreateDepthStencilView(resource, nullptr, dsvHandle);
}

void DXApp::CreateShaderResourceView(ID3D12Resource *resource, int32_t index, const D3D12_SHADER_RESOURCE_VIEW_DESC &desc) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = GetDescriptorHandle(index);
    m_device->CreateShaderResourceView(resource, &desc, handle);
}

void DXApp::CreateUnorderedAccessView(ID3D12Resource *resource, int32_t index, const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = GetDescriptorHandle(index);
    m_device->CreateUnorderedAccessView(resource, nullptr, &desc, handle);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DXApp::GetRenderTargetViewHandle(int32_t index) {
    return {m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), index, m_rtvDescriptorSize};
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DXApp::GetDepthStencilViewHandle(int32_t index) {
    return {m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), index, m_dsvDescriptorSize};
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DXApp::GetDescriptorHandle(int32_t index) {
    return {m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize};
}

void DXApp::ResetCommand(ID3D12CommandAllocator *commandAllocator, ID3D12GraphicsCommandList *commandList) {
    HRESULT result = commandAllocator->Reset();
    DebugCheckCritical(SUCCEEDED(result), "Failed to reset command allocator, error 0x{:x}", static_cast<uint32_t>(result));

    result = commandList->Reset(commandAllocator, nullptr);
    DebugCheckCritical(SUCCEEDED(result), "Failed to reset command list, error 0x{:x}", static_cast<uint32_t>(result));
}

uint64_t DXApp::SignalQueue() {
    ++m_nextFenceValue;
    m_commandQueue->Signal(m_fence.Get(), m_nextFenceValue);

    return m_nextFenceValue;
}

void DXApp::SubmitToQueue(ID3D12GraphicsCommandList *commandList) {
    HRESULT result = commandList->Close();
    DebugCheckCritical(SUCCEEDED(result), "Failed to close command list, error 0x{:x}", static_cast<uint32_t>(result));

    const std::vector<ID3D12CommandList *> lists{commandList};
    m_commandQueue->ExecuteCommandLists(lists.size(), lists.data());
}

void DXApp::AcquireNextFrame() {
    m_backBufferIndex = m_swapchain->GetCurrentBackBufferIndex();

    // TODO: check widow size changes and resize render target
}

void DXApp::WaitForFence(uint64_t fenceValue, uint64_t timeout) {
    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);

        WaitForSingleObject(m_fenceEvent, timeout);
    }
}