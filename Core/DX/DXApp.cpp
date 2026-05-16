//
// Created by y1 on 2026-04-26.
//

#include "DXApp.h"

#include <algorithm>
#include <ranges>
#include <string>

#include <directx/d3dx12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl3.h>

#include "Debug.h"
#include "global_io.slang"

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <numeric>

using Microsoft::WRL::ComPtr;

namespace {
HWND GetHWND(SDL_Window *window) {
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    HWND             hwnd       = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

    DebugCheckCritical(hwnd != nullptr, "Failed to get window handle");
    return hwnd;
}
} // namespace

DXApp::DXApp(SDL_Window *window)
    : m_window(window) {
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

    HWND hwnd = GetHWND(m_window);
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
            .NumDescriptors = kMaxRenderTargets,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };

        result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create render target view descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = kMaxDepthStencils;
        result              = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create depth stencil view descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 10000;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        result              = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        desc.NumDescriptors = 64;
        result              = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imguiHeap.heap));
        DebugCheckCritical(SUCCEEDED(result), "Failed to create ImGui Descriptor heap, error 0x{:x}", static_cast<uint32_t>(result));
        m_imguiHeap.descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_imguiHeap.freeIndices.resize(64);
        std::iota(m_imguiHeap.freeIndices.begin(), m_imguiHeap.freeIndices.end(), 0u);
        std::reverse(m_imguiHeap.freeIndices.begin(), m_imguiHeap.freeIndices.end());
        m_imguiHeap.startCpu = m_imguiHeap.heap->GetCPUDescriptorHandleForHeapStart();
        m_imguiHeap.startGpu = m_imguiHeap.heap->GetGPUDescriptorHandleForHeapStart();
    }

    // Swapchain images
    {
        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
            result = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_swapchainImages[i]));
            DebugCheckCritical(SUCCEEDED(result), "Failed to get swapchain image #{}, error 0x{:x}", i, static_cast<uint32_t>(result));
        }
    }

    // Mipmap generation pipeline
    m_mipmapPipeline = CreateComputePipeline("../Assets/Shaders/mipmap.json");

    // ImGui
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io     = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplSDL3_InitForD3D(m_window);

        ImGui_ImplDX12_InitInfo dx12InitInfo;
        dx12InitInfo.Device            = m_device.Get();
        dx12InitInfo.CommandQueue      = m_commandQueue.Get();
        dx12InitInfo.NumFramesInFlight = kMaxFramesInFlight, dx12InitInfo.RTVFormat = kPresentFormat;
        dx12InitInfo.DSVFormat            = DXGI_FORMAT_UNKNOWN;
        dx12InitInfo.UserData             = &m_imguiHeap;
        dx12InitInfo.SrvDescriptorHeap    = m_imguiHeap.heap.Get();
        dx12InitInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE *cpu, D3D12_GPU_DESCRIPTOR_HANDLE *gpu) {
            static_cast<ImGuiHeap *>(info->UserData)->Allocate(info, cpu, gpu);
        };
        dx12InitInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo *info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
            static_cast<ImGuiHeap *>(info->UserData)->Free(info, cpu, gpu);
        };
        ImGui_ImplDX12_Init(&dx12InitInfo);
    }
}

DXApp::~DXApp() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL3_Shutdown();

    WaitForFence(SignalQueue());
    CloseHandle(m_fenceEvent);
}

FrameInfo DXApp::BeginFrame() {
    WaitForFence(m_fenceValues[m_frameIndex]);
    ResetCommand(m_commandAllocators[m_frameIndex].Get(), m_commandLists[m_frameIndex].Get());

    AcquireNextFrame();

    const std::vector<ID3D12DescriptorHeap *> heaps{m_descriptorHeap.Get()};
    m_commandLists[m_frameIndex]->SetDescriptorHeaps(heaps.size(), heaps.data());

    return FrameInfo{.commandList = m_commandLists[m_frameIndex].Get(), .frameIndex = m_frameIndex, .frameCount = m_frameCount};
}

void DXApp::EndFrame() {
    SubmitToQueue(m_commandLists[m_frameIndex].Get());

    HRESULT result = m_swapchain->Present(1, 0);
    DebugCheckCritical(SUCCEEDED(result), "Failed to present, error 0x{:x}", static_cast<uint32_t>(result));

    m_fenceValues[m_frameIndex] = SignalQueue();
    m_frameIndex                = (m_frameIndex + 1) % kMaxFramesInFlight;
    ++m_frameCount;
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

void DXApp::CreateDepthStencilView(ID3D12Resource *resource, const int32_t index, const D3D12_DEPTH_STENCIL_VIEW_DESC &desc) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetDepthStencilViewHandle(index);
    m_device->CreateDepthStencilView(resource, &desc, dsvHandle);
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

uint64_t DXApp::GetTextureUploadSize(uint64_t width, uint32_t height, DXGI_FORMAT format) const {
    const D3D12_RESOURCE_DESC desc{
        .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width            = width,
        .Height           = height,
        .DepthOrArraySize = 1,
        .MipLevels        = 1,
        .Format           = format,
        .SampleDesc       = {.Count = 1, .Quality = 0},
    };
    uint64_t totalBytes{};
    m_device->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &totalBytes);

    // Offsets into the shared upload buffer must be D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT-aligned.
    return (totalBytes + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & ~uint64_t{D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1};
}

void DXApp::BeginBatchUpload(uint64_t totalBytes) {
    m_uploadBuffer =
        CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, totalBytes, "shared_upload_buffer");
    m_uploadOffset.store(0, std::memory_order_relaxed);
    m_pendingCopies.clear();
}

void DXApp::BatchedTextureUpload(const DXTexture &texture, const void *data) {
    const D3D12_RESOURCE_DESC desc = texture.GetResource()->GetDesc();

    // GPU-required layout: row pitch aligned to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256).
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint32_t                           numRows{};
    uint64_t                           rowSizeInBytes{};
    uint64_t                           totalBytes{};
    m_device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    // Sub-allocate from the shared heap. Reserve aligned size so every offset stays placement-aligned.
    const uint64_t alignedSize = (totalBytes + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1) & ~uint64_t{D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1};
    const uint64_t offset      = m_uploadOffset.fetch_add(alignedSize, std::memory_order_relaxed);
    footprint.Offset           = offset;

    // Source rows are tightly packed; dst rows are aligned to footprint.Footprint.RowPitch.
    m_uploadBuffer.UploadRows(offset, numRows, rowSizeInBytes, footprint.Footprint.RowPitch, data);

    std::scoped_lock<std::mutex> lock(m_batchUploadMutex);
    m_pendingCopies.emplace_back(texture.GetResource(), footprint);
}

void DXApp::BatchedTextureFlush() {
    if (m_pendingCopies.empty()) {
        return;
    }

    ImmediateSubmit([this](ID3D12GraphicsCommandList *commandList) {
        std::vector<CD3DX12_RESOURCE_BARRIER> toCopyDest(m_pendingCopies.size());

        std::ranges::transform(m_pendingCopies, toCopyDest.begin(), [](const PendingCopy &p) {
            return CD3DX12_RESOURCE_BARRIER::Transition(p.dstResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        });

        commandList->ResourceBarrier(static_cast<uint32_t>(toCopyDest.size()), toCopyDest.data());

        ID3D12Resource *sharedSrc = m_uploadBuffer.GetBuffer();
        std::ranges::for_each(m_pendingCopies, [commandList, sharedSrc](const PendingCopy &p) {
            const CD3DX12_TEXTURE_COPY_LOCATION dst{p.dstResource, 0};
            const CD3DX12_TEXTURE_COPY_LOCATION src{sharedSrc, p.footprint};
            commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        });
    });

    m_pendingCopies.clear();
    m_uploadBuffer = {};
    m_uploadOffset.store(0, std::memory_order_relaxed);
}

void DXApp::GenerateMipmaps(DXTexture &texture, uint32_t srcSrvIndex) {
    const uint16_t mipLevels = texture.GetMipLevels();
    if (mipLevels <= 1) {
        return;
    }

    // One UAV per non-base mip slice
    std::vector<DXUnorderedAccessView> mipUavs;
    mipUavs.reserve(mipLevels - 1);
    std::ranges::for_each(std::views::iota(uint16_t{1}, mipLevels), [&](uint16_t m) {
        const D3D12_UNORDERED_ACCESS_VIEW_DESC desc{
            .Format        = texture.GetFormat(),
            .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
            .Texture2D     = {.MipSlice = m, .PlaneSlice = 0},
        };
        mipUavs.push_back(CreateDXUnorderedAccessView(texture.GetResource(), desc));
    });

    ImmediateSubmit([&](ID3D12GraphicsCommandList *commandList) {
        const std::vector<ID3D12DescriptorHeap *> heaps{m_descriptorHeap.Get()};
        commandList->SetDescriptorHeaps(static_cast<uint32_t>(heaps.size()), heaps.data());

        commandList->SetComputeRootSignature(m_mipmapPipeline.GetRootSignature());
        commandList->SetPipelineState(m_mipmapPipeline.GetPipelineState());

        // Initial barriers: mip 0 → sample state, mips 1..N-1 → UAV
        std::vector<CD3DX12_RESOURCE_BARRIER> initBarriers;
        initBarriers.reserve(mipLevels);
        initBarriers.push_back(
            CD3DX12_RESOURCE_BARRIER::Transition(
                texture.GetResource(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                0
            )
        );
        std::ranges::for_each(std::views::iota(uint16_t{1}, mipLevels), [&](uint16_t m) {
            initBarriers.push_back(
                CD3DX12_RESOURCE_BARRIER::Transition(texture.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, m)
            );
        });
        commandList->ResourceBarrier(static_cast<uint32_t>(initBarriers.size()), initBarriers.data());

        // Dispatch one downsample per mip pair: read mip i, write mip i+1
        const uint64_t baseWidth  = texture.GetWidth();
        const uint32_t baseHeight = texture.GetHeight();
        std::ranges::for_each(std::views::iota(uint16_t{0}, static_cast<uint16_t>(mipLevels - 1)), [&](uint16_t i) {
            const uint32_t dstMip    = static_cast<uint32_t>(i + 1);
            const uint32_t dstWidth  = std::max<uint32_t>(1, static_cast<uint32_t>(baseWidth) >> dstMip);
            const uint32_t dstHeight = std::max<uint32_t>(1, baseHeight >> dstMip);

            const shader_io::MipmapUniforms uniforms{
                .srcIndex    = srcSrvIndex,
                .dstIndex    = mipUavs[i].GetIndex(),
                .srcMipLevel = i,
                .padding     = 0,
            };
            commandList->SetComputeRoot32BitConstants(0, sizeof(uniforms) / sizeof(uint32_t), &uniforms, 0);

            const uint32_t groupX = static_cast<uint32_t>(std::ceil(static_cast<float>(dstWidth) / shader_io::kMipmapThreadX));
            const uint32_t groupY = static_cast<uint32_t>(std::ceil(static_cast<float>(dstHeight) / shader_io::kMipmapThreadY));
            commandList->Dispatch(groupX, groupY, 1);

            // Hand off the just-written mip to the next iteration as a sample source
            const auto b = CD3DX12_RESOURCE_BARRIER::Transition(
                texture.GetResource(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                dstMip
            );
            commandList->ResourceBarrier(1, &b);
        });

        // All mips end in NON_PIXEL_SHADER_RESOURCE; transition to PIXEL_SHADER_RESOURCE for rendering
        std::vector<CD3DX12_RESOURCE_BARRIER> finalBarriers;
        finalBarriers.reserve(mipLevels);
        std::ranges::for_each(std::views::iota(uint16_t{0}, mipLevels), [&](uint16_t m) {
            finalBarriers.push_back(
                CD3DX12_RESOURCE_BARRIER::Transition(
                    texture.GetResource(),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    m
                )
            );
        });
        commandList->ResourceBarrier(static_cast<uint32_t>(finalBarriers.size()), finalBarriers.data());
    });
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
}

void DXApp::WaitForFence(uint64_t fenceValue, uint64_t timeout) {
    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);

        WaitForSingleObject(m_fenceEvent, timeout);
    }
}

void DXApp::ImGuiHeap::Allocate(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *gpuHandle) {
    DebugCheckCritical(freeIndices.size() > 0, "ImGui descriptor heap does not have free index");
    int32_t idx = freeIndices.back();
    freeIndices.pop_back();
    cpuHandle->ptr = startCpu.ptr + idx * descriptorSize;
    gpuHandle->ptr = startGpu.ptr + idx * descriptorSize;
}

void DXApp::ImGuiHeap::Free(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
    int32_t cpuIdx = static_cast<int32_t>((cpuHandle.ptr - startCpu.ptr) / descriptorSize);
    int32_t gpuIdx = static_cast<int32_t>(gpuHandle.ptr - startGpu.ptr) / descriptorSize;
    DebugCheckCritical(cpuIdx == gpuIdx, "Imgui descriptor heap CPU and GPU indices do not match when freeing");
    freeIndices.push_back(cpuIdx);
}