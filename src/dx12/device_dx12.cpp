// src/dx12/device_dx12.cpp
// DX12Backend — device lifecycle, adapter enumeration, feature queries.
// Phase A: Init, Shutdown, EnumerateAdapters, CreateDevice, DestroyDevice, GetFeatureLevel,
//          SupportsFeature, GetNativeDevice/Context.
// All other methods return DXB_E_NOT_SUPPORTED (will be implemented in Phases B–C).
#include "device_dx12.hpp"
#include <d3d12sdklayers.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstring>
#include <cstdio>
#include "../common/error.hpp"
#include "../common/log.hpp"

// d3d12 / dxgi / d3dcompiler linked via CMake target_link_libraries

using Microsoft::WRL::ComPtr;

namespace {
    // Helper: flush all GPU work before destroying device-owned objects.
    void WaitForGPU(dxb::DeviceObjectDX12* dev) {
        if (!dev || !dev->fence || !dev->cmd_queue) return;
        UINT64 flush_val = (dev->fence_values[0] > dev->fence_values[1]
                            ? dev->fence_values[0] : dev->fence_values[1]) + 1ULL;
        dev->cmd_queue->Signal(dev->fence.Get(), flush_val);
        if (dev->fence->GetCompletedValue() < flush_val) {
            dev->fence->SetEventOnCompletion(flush_val, dev->fence_event);
            WaitForSingleObject(dev->fence_event, INFINITE);
        }
        dev->fence_values[0] = dev->fence_values[1] = flush_val;
    }

    // Helper: resource barrier transition (used by render phase too; duplicated here
    //         for the device file; render_dx12.cpp will have its own copy).
    void TransitionBarrier(ID3D12GraphicsCommandList* list,
                           ID3D12Resource* resource,
                           D3D12_RESOURCE_STATES from,
                           D3D12_RESOURCE_STATES to) {
        D3D12_RESOURCE_BARRIER b = {};
        b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.Transition.pResource   = resource;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = from;
        b.Transition.StateAfter  = to;
        list->ResourceBarrier(1, &b);
    }
} // anonymous namespace

namespace dxb {

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

DXBResult DX12Backend::Init() {
    if (m_initialized) return DXB_OK;

    // Try debug factory first; fall back to release factory.
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
                                    IID_PPV_ARGS(m_factory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        hr = CreateDXGIFactory2(0,
                                IID_PPV_ARGS(m_factory.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            SetLastErrorFromHR(hr, "CreateDXGIFactory2 (DX12)");
            return MapHRESULT(hr);
        }
    }

    m_initialized = true;
    DXB_LOG_INFO("DX12Backend initialized");
    return DXB_OK;
}

void DX12Backend::Shutdown() {
    m_factory.Reset();
    m_initialized = false;
    DXB_LOG_INFO("DX12Backend shutdown");
}

// ---------------------------------------------------------------------------
// Adapter enumeration (two-call pattern)
// ---------------------------------------------------------------------------

DXBResult DX12Backend::EnumerateAdapters(DXBAdapterInfo* buf, uint32_t* count) {
    if (!count) {
        SetLastError(DXB_E_INVALID_ARG, "EnumerateAdapters: count must not be null");
        return DXB_E_INVALID_ARG;
    }
    if (!m_initialized) {
        SetLastError(DXB_E_NOT_INITIALIZED, "EnumerateAdapters: backend not initialized");
        return DXB_E_NOT_INITIALIZED;
    }

    // Collect hardware adapters, then WARP.
    std::vector<ComPtr<IDXGIAdapter1>> adapters;

    // Try IDXGIFactory6 for GPU-preference ordering.
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(m_factory.As(&factory6))) {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> a;
            HRESULT hr = factory6->EnumAdapterByGpuPreference(
                i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(a.ReleaseAndGetAddressOf()));
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) break;
            // Skip software adapters from the hardware list (WARP added separately below).
            DXGI_ADAPTER_DESC1 d = {};
            a->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            adapters.push_back(a);
        }
    } else {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> a;
            HRESULT hr = m_factory->EnumAdapters1(i, a.ReleaseAndGetAddressOf());
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) break;
            DXGI_ADAPTER_DESC1 d = {};
            a->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            adapters.push_back(a);
        }
    }

    // Always include WARP as the last entry.
    {
        ComPtr<IDXGIAdapter1> warp;
        if (SUCCEEDED(m_factory->EnumWarpAdapter(IID_PPV_ARGS(warp.ReleaseAndGetAddressOf())))) {
            adapters.push_back(warp);
        }
    }

    uint32_t available = static_cast<uint32_t>(adapters.size());

    // First call: query count.
    if (!buf) {
        *count = available;
        return DXB_OK;
    }

    // Second call: fill buffer.
    uint32_t to_fill = (*count < available) ? *count : available;
    for (uint32_t i = 0; i < to_fill; ++i) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapters[i]->GetDesc1(&desc);

        DXBAdapterInfo& info       = buf[i];
        info.struct_version        = DXBRIDGE_STRUCT_VERSION;
        info.index                 = i;
        info.dedicated_video_mem   = desc.DedicatedVideoMemory;
        info.dedicated_system_mem  = desc.DedicatedSystemMemory;
        info.shared_system_mem     = desc.SharedSystemMemory;
        info.is_software           = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? 1u : 0u;

        WideCharToMultiByte(CP_UTF8, 0,
                            desc.Description, -1,
                            info.description, sizeof(info.description),
                            nullptr, nullptr);
        info.description[sizeof(info.description) - 1] = '\0';
    }
    *count = to_fill;
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// CreateDevice
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreateDevice(const DXBDeviceDesc* desc, DXBDevice* out) {
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateDevice: null argument");
        return DXB_E_INVALID_ARG;
    }
    if (!m_initialized) {
        SetLastError(DXB_E_NOT_INITIALIZED, "CreateDevice: not initialized");
        return DXB_E_NOT_INITIALIZED;
    }

    *out = DXBRIDGE_NULL_HANDLE;

    // 1. Optional: enable debug layer BEFORE D3D12CreateDevice.
    bool debug_enabled = false;
    if (desc->flags & DXB_DEVICE_FLAG_DEBUG) {
        ComPtr<ID3D12Debug> debug_ctrl;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_ctrl)))) {
            debug_ctrl->EnableDebugLayer();
            debug_enabled = true;
            DXB_LOG_INFO("DX12 debug layer enabled");
        } else {
            DXB_LOG_WARN("DX12 debug layer unavailable (install Graphics Tools)");
        }
    }

    // 2. Select adapter.
    ComPtr<IDXGIAdapter1> adapter1;
    // adapter_index == 0 → high-performance GPU via IDXGIFactory6 if available.
    {
        ComPtr<IDXGIFactory6> factory6;
        if (desc->adapter_index == 0 && SUCCEEDED(m_factory.As(&factory6))) {
            factory6->EnumAdapterByGpuPreference(
                0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter1.ReleaseAndGetAddressOf()));
        }
    }
    if (!adapter1) {
        // Enumerate hardware adapters in order, skipping software ones.
        uint32_t hw_idx    = 0;
        uint32_t target    = desc->adapter_index;
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> a;
            if (FAILED(m_factory->EnumAdapters1(i, a.ReleaseAndGetAddressOf()))) break;
            DXGI_ADAPTER_DESC1 d = {};
            a->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (hw_idx == target) { adapter1 = a; break; }
            ++hw_idx;
        }
    }
    if (!adapter1) {
        // Fall back to WARP (software adapter).
        m_factory->EnumWarpAdapter(IID_PPV_ARGS(adapter1.ReleaseAndGetAddressOf()));
    }

    // 3. Create D3D12 device.
    auto* obj = new DeviceObjectDX12();
    {
        ComPtr<IDXGIAdapter> adapter;
        if (adapter1) adapter1.As(&adapter);
        HRESULT hr = D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&obj->device));
        if (FAILED(hr)) {
            delete obj;
            SetLastErrorFromHR(hr, "D3D12CreateDevice");
            return MapHRESULT(hr);
        }
    }
    obj->debug_enabled = debug_enabled;
    obj->feature_level = D3D_FEATURE_LEVEL_12_0;

    // Store adapter as IDXGIAdapter.
    if (adapter1) adapter1.As(&obj->adapter);

    // 4. Command queue (DIRECT).
    {
        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
        qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        HRESULT hr = obj->device->CreateCommandQueue(&qd, IID_PPV_ARGS(&obj->cmd_queue));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateCommandQueue"); return MapHRESULT(hr); }
    }

    // 5. Command allocators (one per frame).
    for (int i = 0; i < 2; ++i) {
        HRESULT hr = obj->device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&obj->cmd_allocators[i]));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateCommandAllocator"); return MapHRESULT(hr); }
    }

    // 6. Command list — starts Recording; Close() immediately.
    {
        HRESULT hr = obj->device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            obj->cmd_allocators[0].Get(), nullptr,
            IID_PPV_ARGS(&obj->cmd_list));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateCommandList"); return MapHRESULT(hr); }
        obj->cmd_list->Close(); // put into Closed state; BeginFrame will Reset()
    }

    // 7. Fence + Win32 event.
    {
        HRESULT hr = obj->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&obj->fence));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateFence"); return MapHRESULT(hr); }
        obj->fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!obj->fence_event) { delete obj; SetLastError(DXB_E_INTERNAL, "CreateEventW failed"); return DXB_E_INTERNAL; }
        obj->fence_values[0] = obj->fence_values[1] = 0;
    }

    // 8. Descriptor heaps.
    // RTV heap — CPU-only, 16 slots.
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = 16;
        hd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = obj->device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&obj->rtv_heap.heap));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateDescriptorHeap (RTV)"); return MapHRESULT(hr); }
        obj->rtv_heap.capacity = 16;
        obj->rtv_heap.Init(obj->device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }
    // DSV heap — CPU-only, 8 slots.
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        hd.NumDescriptors = 8;
        hd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = obj->device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&obj->dsv_heap.heap));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateDescriptorHeap (DSV)"); return MapHRESULT(hr); }
        obj->dsv_heap.capacity = 8;
        obj->dsv_heap.Init(obj->device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }

    // 9. Empty root signature (allows input assembler; no CBV/SRV slots in Phase 1).
    {
        D3D12_ROOT_SIGNATURE_DESC rsd = {};
        rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> sig_blob, error_blob;
        HRESULT hr = D3D12SerializeRootSignature(
            &rsd, D3D_ROOT_SIGNATURE_VERSION_1,
            &sig_blob, &error_blob);
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "D3D12SerializeRootSignature"); return MapHRESULT(hr); }
        hr = obj->device->CreateRootSignature(
            0,
            sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(),
            IID_PPV_ARGS(&obj->empty_root_sig));
        if (FAILED(hr)) { delete obj; SetLastErrorFromHR(hr, "CreateRootSignature"); return MapHRESULT(hr); }
    }

    *out = m_devices.Alloc(obj, ObjectType::Device);
    DXB_LOG_INFO("DX12 device created (feature level 0x%04X)", (unsigned)obj->feature_level);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyDevice
// ---------------------------------------------------------------------------

void DX12Backend::DestroyDevice(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return;

    // Flush GPU before releasing any GPU objects.
    WaitForGPU(obj);

    if (obj->fence_event) {
        CloseHandle(obj->fence_event);
        obj->fence_event = nullptr;
    }

    m_devices.Free(device, ObjectType::Device);
    delete obj;
    DXB_LOG_INFO("DX12 device destroyed");
}

// ---------------------------------------------------------------------------
// GetFeatureLevel
// ---------------------------------------------------------------------------

uint32_t DX12Backend::GetFeatureLevel(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return 0;
    return static_cast<uint32_t>(obj->feature_level);
}

// ---------------------------------------------------------------------------
// SupportsFeature
// ---------------------------------------------------------------------------

uint32_t DX12Backend::SupportsFeature(uint32_t flag) {
    if (flag & DXB_FEATURE_DX12) return 1u;
    return 0u;
}

// ---------------------------------------------------------------------------
// Native escape hatches
// ---------------------------------------------------------------------------

void* DX12Backend::GetNativeDevice(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return nullptr;
    // Caller must AddRef/Release if they store it.
    ID3D12Device* raw = obj->device.Get();
    if (raw) raw->AddRef();
    return static_cast<void*>(raw);
}

void* DX12Backend::GetNativeContext(DXBDevice /*device*/) {
    // DX12 has no immediate device context — return nullptr.
    return nullptr;
}

void DX12Backend::SetLogCallback(DXBLogCallback cb, void* ud) {
    m_log_cb   = cb;
    m_log_data = ud;
}

// Phase B/C methods are implemented in:
//   swapchain_dx12.cpp — CreateSwapChain, DestroySwapChain, ResizeSwapChain, GetBackBuffer,
//                        CreateRenderTargetView, DestroyRTV, CreateDepthStencilView, DestroyDSV
//   buffer_dx12.cpp    — CreateBuffer, UploadData, DestroyBuffer
//   shader_dx12.cpp    — CompileShader, DestroyShader
//   pipeline_dx12.cpp  — CreateInputLayout, DestroyInputLayout, CreatePipelineState, DestroyPipeline
//   render_dx12.cpp    — BeginFrame, SetRenderTargets, ClearRenderTarget, ClearDepthStencil,
//                        SetViewport, SetPipeline, SetVertexBuffer, SetIndexBuffer,
//                        SetConstantBuffer, Draw, DrawIndexed, EndFrame, Present

} // namespace dxb
