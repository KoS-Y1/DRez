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

#include <span>

using Microsoft::WRL::ComPtr;

namespace {
struct WindowExtent {
    uint32_t width;
    uint32_t height;
};

WindowExtent GetWindowExtent(HWND hwnd) {
    RECT rect;
    DebugCheckCritical(GetClientRect(hwnd, &rect), "Failed to get window extent");
    return WindowExtent{.width = static_cast<uint32_t>(rect.right - rect.left), .height = static_cast<uint32_t>(rect.bottom - rect.top)};
}
} // namespace

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
        D3D12_COMMAND_QUEUE_DESC desc{.Type = D3D12_COMMAND_LIST_TYPE_DIRECT, .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE};
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

    WindowExtent extent = GetWindowExtent(hwnd);

    // Swapchain
    {
        DXGI_MODE_DESC bufferDesc{.Width = extent.width, .Height = extent.height, .Format = kPresentFormat};

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

    // Descriptor heaps for render target view
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = kMaxFramesInFlight,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };

        result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create render target view descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Frame resources
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvHeap->GetCPUDescriptorHandleForHeapStart()};

        // Create an RTV for each frame
        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
            result = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
            DebugCheckCritical(SUCCEEDED(result), "Failed to get buffer when creating RTV #{}, error 0x{:x}", i, static_cast<uint32_t>(result));
            m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    m_gfx      = DXGraphicsPipeline(*this, "../Assets/Shaders/test.json");
    m_viewport = CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height)};
    m_rect     = CD3DX12_RECT{0, 0, static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height)};
    // Vertex buffer
    {
        std::vector<VertexPT2D> vertices{
            VertexPT2D({-1.0f, -1.0f}, {0.0f, 0.0f}),
            VertexPT2D({1.0f, -1.0f}, {1.0f, 0.0f}),
            VertexPT2D({-1.0f, 1.0f}, {0.0f, 1.0f}),
            VertexPT2D({1.0f, 1.0f}, {1.0f, 1.0f}),
        };
        const uint32_t size = std::span<VertexPT2D>(vertices).size_bytes();

        ComPtr<ID3D12Resource>  stagingBuffer{};
        CD3DX12_HEAP_PROPERTIES heapProperties{D3D12_HEAP_TYPE_UPLOAD};
        auto                    bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        result                             = m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&stagingBuffer)
        );
        DebugCheckCritical(SUCCEEDED(result), "Failed to create staging buffer for vertex buffer, error 0x{:x}", static_cast<uint32_t>(result));


        heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        result         = m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)
        );
        DebugCheckCritical(SUCCEEDED(result), "Failed to create vertex buffer, error 0x{:x}", static_cast<uint32_t>(result));

        D3D12_SUBRESOURCE_DATA data{};
        data.pData      = reinterpret_cast<uint8_t *>(vertices.data());
        data.RowPitch   = size;
        data.SlicePitch = size;

        ImmediateSubmit([&](const ComPtr<ID3D12GraphicsCommandList> &commandList) {
            UpdateSubresources<1>(commandList.Get(), m_vertexBuffer.Get(), stagingBuffer.Get(), 0, 0, 1, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_vertexBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
            );
            commandList->ResourceBarrier(1, &barrier);
        });

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes  = sizeof(VertexPT2D);
        m_vertexBufferView.SizeInBytes    = size;
    }
}

DXApp::~DXApp() {
    CloseHandle(m_fenceEvent);
}

DXApp::FrameInfo DXApp::BeginFrame() {
    WaitForFence(m_fenceValues[m_frameIndex]);
    ResetCommand(m_commandAllocators[m_frameIndex], m_commandLists[m_frameIndex]);

    AcquireNextFrame();

    return FrameInfo{.commandList = m_commandLists[m_frameIndex]};
}

void DXApp::EndFrame() {
    SubmitToQueue(m_commandLists[m_frameIndex]);

    HRESULT result = m_swapchain->Present(1, 0);
    DebugCheckCritical(SUCCEEDED(result), "Failed to present, error 0x{:x}", static_cast<uint32_t>(result));

    m_fenceValues[m_frameIndex] = SignalQueue();
    m_frameIndex                = (m_frameIndex + 1) % kMaxFramesInFlight;
}

void DXApp::Run() {
    FrameInfo                          frameInfo   = BeginFrame();
    ComPtr<ID3D12GraphicsCommandList> &commandList = frameInfo.commandList;

    // TODO: render
    commandList->SetGraphicsRootSignature(m_gfx.GetRootSignature().Get());
    commandList->SetPipelineState(m_gfx.GetPipelineState().Get());
    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_rect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_backBufferIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        static_cast<int32_t>(m_backBufferIndex),
        static_cast<uint32_t>(m_rtvDescriptorSize)
    };
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const std::vector<float> clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor.data(), 0, nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->DrawInstanced(4, 1, 0, 0);


    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_backBufferIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    commandList->ResourceBarrier(1, &barrier2);

    EndFrame();
}

void DXApp::ResetCommand(ComPtr<ID3D12CommandAllocator> &commandAllocator, ComPtr<ID3D12GraphicsCommandList> &commandList) {
    HRESULT result = commandAllocator->Reset();
    DebugCheckCritical(SUCCEEDED(result), "Failed to reset command allocator, error 0x{:x}", static_cast<uint32_t>(result));

    result = commandList->Reset(commandAllocator.Get(), nullptr);
    DebugCheckCritical(SUCCEEDED(result), "Failed to reset command list, error 0x{:x}", static_cast<uint32_t>(result));
}

uint64_t DXApp::SignalQueue() {
    ++m_nextFenceValue;
    m_commandQueue->Signal(m_fence.Get(), m_nextFenceValue);

    return m_nextFenceValue;
}

void DXApp::SubmitToQueue(ComPtr<ID3D12GraphicsCommandList> &commandList) {
    HRESULT result = commandList->Close();
    DebugCheckCritical(SUCCEEDED(result), "Failed to close command list, error 0x{:x}", static_cast<uint32_t>(result));

    const std::vector<ID3D12CommandList *> lists{commandList.Get()};
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