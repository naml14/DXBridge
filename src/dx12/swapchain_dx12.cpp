// src/dx12/swapchain_dx12.cpp
// DX12Backend — swap chain, back buffer, RTV, DSV.
// Phase B: CreateSwapChain, DestroySwapChain, ResizeSwapChain,
//          GetBackBuffer, CreateRenderTargetView, DestroyRTV,
//          CreateDepthStencilView, DestroyDSV.
#include "device_dx12.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>

using Microsoft::WRL::ComPtr;

namespace {

static bool IsDeviceLostHRESULT(HRESULT hr) noexcept {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

static DXBResult GuardDeviceLost(dxb::DeviceObjectDX12* dev, const char* context) noexcept {
    if (!dev || !dev->device_lost) {
        return DXB_OK;
    }

    dxb::SetLastError(DXB_E_DEVICE_LOST, "%s: device lost", context);
    return DXB_E_DEVICE_LOST;
}

static DXBResult ReturnHRESULT(dxb::DeviceObjectDX12* dev,
                               HRESULT hr,
                               const char* context) noexcept {
    if (IsDeviceLostHRESULT(hr) && dev) {
        dev->device_lost = true;
        dxb::SetLastErrorFromHR(hr, context);
        return DXB_E_DEVICE_LOST;
    }

    dxb::SetLastErrorFromHR(hr, context);
    return dxb::MapHRESULT(hr);
}

// ---------------------------------------------------------------------------
// WaitForGPU — flush all frames in flight (used before ResizeBuffers)
// ---------------------------------------------------------------------------
static DXBResult WaitForGPU(dxb::DeviceObjectDX12* dev, const char* context) {
    if (!dev || !dev->fence || !dev->cmd_queue) {
        return DXB_OK;
    }

    UINT64 flush_val = (dev->fence_values[0] > dev->fence_values[1]
                        ? dev->fence_values[0] : dev->fence_values[1]) + 1ULL;
    HRESULT hr = dev->cmd_queue->Signal(dev->fence.Get(), flush_val);
    if (FAILED(hr)) {
        return ReturnHRESULT(dev, hr, context);
    }

    if (dev->fence->GetCompletedValue() < flush_val) {
        hr = dev->fence->SetEventOnCompletion(flush_val, dev->fence_event);
        if (FAILED(hr)) {
            return ReturnHRESULT(dev, hr, context);
        }
        WaitForSingleObject(dev->fence_event, INFINITE);
    }
    dev->fence_values[0] = dev->fence_values[1] = flush_val;
    return DXB_OK;
}

} // anonymous namespace

namespace dxb {

// ---------------------------------------------------------------------------
// CreateSwapChain
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreateSwapChain(DXBDevice device_handle,
                                        const DXBSwapChainDesc* desc,
                                        DXBSwapChain* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateSwapChain: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateSwapChain: invalid device handle");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CreateSwapChain");
    if (lost != DXB_OK) {
        return lost;
    }

    if (!desc->hwnd) {
        SetLastError(DXB_E_INVALID_ARG, "CreateSwapChain: hwnd is null");
        return DXB_E_INVALID_ARG;
    }

    uint32_t buf_count = (desc->buffer_count >= 2) ? desc->buffer_count : 2;
    if (desc->buffer_count > 3) {
        DXB_LOG_WARN("DX12 CreateSwapChain requested %u buffers; clamping to 3", desc->buffer_count);
    }
    if (buf_count > 3) buf_count = 3; // max 3 back buffers (array size limit)

    DXGI_SWAP_CHAIN_DESC1 sc_desc = {};
    sc_desc.Width       = desc->width;
    sc_desc.Height      = desc->height;
    sc_desc.Format      = DXBFormatToDXGI(desc->format);
    if (sc_desc.Format == DXGI_FORMAT_UNKNOWN) {
        SetLastError(DXB_E_INVALID_ARG, "CreateSwapChain: unsupported format value %u", (unsigned)desc->format);
        return DXB_E_INVALID_ARG;
    }
    sc_desc.SampleDesc  = { 1, 0 };
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = buf_count;
    sc_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc_desc.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // CRITICAL: DX12 swap chain takes cmd_queue, NOT ID3D12Device*
    ComPtr<IDXGISwapChain1> sc1;
    HRESULT hr = m_factory->CreateSwapChainForHwnd(
        dev->cmd_queue.Get(),       // command queue, not device!
        static_cast<HWND>(desc->hwnd),
        &sc_desc, nullptr, nullptr,
        sc1.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        return ReturnHRESULT(dev, hr, "CreateSwapChainForHwnd (DX12)");
    }

    // Disable Alt+Enter fullscreen shortcut
    m_factory->MakeWindowAssociation(static_cast<HWND>(desc->hwnd),
                                     DXGI_MWA_NO_ALT_ENTER);

    auto* sc_obj = new SwapChainObjectDX12();

    // QI to IDXGISwapChain3 (required for GetCurrentBackBufferIndex)
    hr = sc1->QueryInterface(IID_PPV_ARGS(&sc_obj->swapchain));
    if (FAILED(hr)) {
        delete sc_obj;
        return ReturnHRESULT(dev, hr, "QueryInterface IDXGISwapChain3");
    }

    sc_obj->buffer_count  = buf_count;
    sc_obj->width         = desc->width;
    sc_obj->height        = desc->height;
    sc_obj->format        = desc->format;
    sc_obj->device_handle = device_handle;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[3] = {};
    if (!dev->rtv_heap.TryAllocCPUBlock(buf_count, rtv_handles)) {
        delete sc_obj;
        SetLastError(DXB_E_OUT_OF_MEMORY, "CreateSwapChain: RTV descriptor heap exhausted");
        return DXB_E_OUT_OF_MEMORY;
    }

    // Acquire back buffers + create RTVs in device's rtv_heap
    for (uint32_t i = 0; i < buf_count; ++i) {
        hr = sc_obj->swapchain->GetBuffer(i, IID_PPV_ARGS(&sc_obj->back_buffers[i]));
        if (FAILED(hr)) {
            delete sc_obj;
            return ReturnHRESULT(dev, hr, "IDXGISwapChain3::GetBuffer");
        }
        sc_obj->rtv_handles[i] = rtv_handles[i];
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format        = sc_desc.Format;
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        dev->device->CreateRenderTargetView(
            sc_obj->back_buffers[i].Get(), &rtv_desc, sc_obj->rtv_handles[i]);
    }

    // Store format on device for PSO creation
    dev->current_rt_format = sc_desc.Format;

    // Initial frame index
    dev->frame_index = sc_obj->swapchain->GetCurrentBackBufferIndex();

    uint64_t h = m_swap_chains.Alloc(sc_obj, ObjectType::SwapChain);
    dev->active_sc = h;
    *out = h;

    DXB_LOG_INFO("DX12 swap chain created (%ux%u, %u buffers)", desc->width, desc->height, buf_count);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroySwapChain
// ---------------------------------------------------------------------------

void DX12Backend::DestroySwapChain(DXBSwapChain sc) {
    auto* sc_obj = m_swap_chains.Get(sc, ObjectType::SwapChain);
    if (!sc_obj) return;

    // Flush GPU before releasing back buffers
    auto* dev = m_devices.Get(sc_obj->device_handle, ObjectType::Device);
    if (dev) {
        (void)WaitForGPU(dev, "DestroySwapChain: WaitForGPU");
        if (dev->active_sc == sc) dev->active_sc = DXBRIDGE_NULL_HANDLE;
    }

    // Release back buffer resources
    for (uint32_t i = 0; i < sc_obj->buffer_count; ++i) {
        sc_obj->back_buffers[i].Reset();
    }

    m_swap_chains.Free(sc, ObjectType::SwapChain);
    delete sc_obj;
}

// ---------------------------------------------------------------------------
// ResizeSwapChain
// ---------------------------------------------------------------------------

DXBResult DX12Backend::ResizeSwapChain(DXBSwapChain sc, uint32_t w, uint32_t h) {
    auto* sc_obj = m_swap_chains.Get(sc, ObjectType::SwapChain);
    if (!sc_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "ResizeSwapChain: invalid handle");
        return DXB_E_INVALID_HANDLE;
    }

    auto* dev = m_devices.Get(sc_obj->device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "ResizeSwapChain: device gone");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "ResizeSwapChain");
    if (lost != DXB_OK) {
        return lost;
    }

    // GAP 1: must not resize while a frame is in progress (between BeginFrame and EndFrame)
    if (dev->frame_in_progress) {
        DXB_LOG_WARN("DX12 ResizeSwapChain rejected while frame %u is in progress", dev->frame_index);
        SetLastError(DXB_E_INVALID_STATE, "ResizeSwapChain: called while frame is in progress");
        return DXB_E_INVALID_STATE;
    }

    // 1. Flush ALL frames — CRITICAL before ResizeBuffers
    DXBResult wait_result = WaitForGPU(dev, "ResizeSwapChain: WaitForGPU");
    if (wait_result != DXB_OK) {
        return wait_result;
    }

    // 2. Release ALL back buffer ComPtr references — REQUIRED before ResizeBuffers
    //    Any remaining ref → DXGI_ERROR_INVALID_CALL
    for (uint32_t i = 0; i < sc_obj->buffer_count; ++i) {
        sc_obj->back_buffers[i].Reset();
    }

    // 3. ResizeBuffers (0 = preserve buffer count, DXGI_FORMAT_UNKNOWN = preserve format)
    HRESULT hr = sc_obj->swapchain->ResizeBuffers(
        0, w, h,
        DXGI_FORMAT_UNKNOWN,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr)) {
        return ReturnHRESULT(dev, hr, "IDXGISwapChain3::ResizeBuffers");
    }
    sc_obj->width  = w;
    sc_obj->height = h;

    // 4. Re-acquire back buffers and recreate RTVs in existing slots
    for (uint32_t i = 0; i < sc_obj->buffer_count; ++i) {
        hr = sc_obj->swapchain->GetBuffer(i, IID_PPV_ARGS(&sc_obj->back_buffers[i]));
        if (FAILED(hr)) {
            return ReturnHRESULT(dev, hr, "GetBuffer after ResizeBuffers");
        }
        // Reuse existing RTV handle slots (bump allocator can't free; slots remain valid)
        dev->device->CreateRenderTargetView(
            sc_obj->back_buffers[i].Get(), nullptr, sc_obj->rtv_handles[i]);
    }

    // 5. Update frame index
    dev->frame_index = sc_obj->swapchain->GetCurrentBackBufferIndex();

    DXB_LOG_INFO("DX12 swap chain resized to %ux%u", w, h);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// GetBackBuffer — returns a non-owning RenderTargetObjectDX12 for current frame
// ---------------------------------------------------------------------------

DXBResult DX12Backend::GetBackBuffer(DXBSwapChain sc, DXBRenderTarget* out) {
    if (!out) {
        SetLastError(DXB_E_INVALID_ARG, "GetBackBuffer: null out");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* sc_obj = m_swap_chains.Get(sc, ObjectType::SwapChain);
    if (!sc_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "GetBackBuffer: invalid swap chain");
        return DXB_E_INVALID_HANDLE;
    }

    auto* dev = m_devices.Get(sc_obj->device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "GetBackBuffer: device gone");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "GetBackBuffer");
    if (lost != DXB_OK) {
        return lost;
    }

    // Non-owning reference to the current back buffer
    auto* rt_obj          = new RenderTargetObjectDX12();
    rt_obj->resource      = sc_obj->back_buffers[dev->frame_index].Get();
    rt_obj->buffer_index  = dev->frame_index;
    rt_obj->sc_handle     = sc;

    *out = m_render_targets.Alloc(rt_obj, ObjectType::RenderTarget);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// CreateRenderTargetView — for back-buffer RT: reuse pre-built RTV in swap chain
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreateRenderTargetView(DXBDevice device_handle,
                                               DXBRenderTarget rt_handle,
                                               DXBRTV* out)
{
    if (!out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateRenderTargetView: null out");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateRenderTargetView: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CreateRenderTargetView");
    if (lost != DXB_OK) {
        return lost;
    }

    auto* rt = m_render_targets.Get(rt_handle, ObjectType::RenderTarget);
    if (!rt) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateRenderTargetView: invalid render target");
        return DXB_E_INVALID_HANDLE;
    }

    // Retrieve the swap chain to get the pre-built RTV handle
    auto* sc = m_swap_chains.Get(rt->sc_handle, ObjectType::SwapChain);
    if (!sc) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateRenderTargetView: swap chain gone");
        return DXB_E_INVALID_HANDLE;
    }

    auto* rtv_obj         = new RTVObjectDX12();
    rtv_obj->cpu_handle   = sc->rtv_handles[rt->buffer_index]; // reuse pre-built slot
    rtv_obj->resource     = rt->resource;                       // non-owning
    rtv_obj->device_handle = device_handle;

    *out = m_rtvs.Alloc(rtv_obj, ObjectType::RTV);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyRTV — DX12 owns no COM refs in RTVObjectDX12 (resource owned by SC)
// ---------------------------------------------------------------------------

void DX12Backend::DestroyRTV(DXBRTV rtv) {
    auto* obj = m_rtvs.Get(rtv, ObjectType::RTV);
    if (!obj) return;
    m_rtvs.Free(rtv, ObjectType::RTV);
    delete obj;
}

// ---------------------------------------------------------------------------
// CreateDepthStencilView — committed resource in default heap
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreateDepthStencilView(DXBDevice device_handle,
                                               const DXBDepthStencilDesc* desc,
                                               DXBDSV* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateDepthStencilView: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateDepthStencilView: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CreateDepthStencilView");
    if (lost != DXB_OK) {
        return lost;
    }

    DXGI_FORMAT dsv_format = DXBFormatToDXGI(desc->format);
    if (dsv_format == DXGI_FORMAT_UNKNOWN) {
        SetLastError(DXB_E_INVALID_ARG, "CreateDepthStencilView: unsupported format value %u", (unsigned)desc->format);
        return DXB_E_INVALID_ARG;
    }

    // Create depth texture as committed resource in default heap
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Alignment          = 0;
    rd.Width              = desc->width;
    rd.Height             = desc->height;
    rd.DepthOrArraySize   = 1;
    rd.MipLevels          = 1;
    rd.Format             = dsv_format;
    rd.SampleDesc         = { 1, 0 };
    rd.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rd.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE cv = {};
    cv.Format                  = dsv_format;
    cv.DepthStencil.Depth      = 1.0f;
    cv.DepthStencil.Stencil    = 0;

    auto* dsv_obj = new DSVObjectDX12();
    HRESULT hr = dev->device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &cv,
        IID_PPV_ARGS(&dsv_obj->resource));
    if (FAILED(hr)) {
        delete dsv_obj;
        return ReturnHRESULT(dev, hr, "CreateCommittedResource (depth texture)");
    }

    if (!dev->dsv_heap.TryAllocCPU(&dsv_obj->cpu_handle)) {
        delete dsv_obj;
        SetLastError(DXB_E_OUT_OF_MEMORY, "CreateDepthStencilView: DSV descriptor heap exhausted");
        return DXB_E_OUT_OF_MEMORY;
    }
    dsv_obj->width         = desc->width;
    dsv_obj->height        = desc->height;
    dsv_obj->device_handle = device_handle;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format        = dsv_format;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Flags         = D3D12_DSV_FLAG_NONE;
    dev->device->CreateDepthStencilView(dsv_obj->resource.Get(), &dsv_desc, dsv_obj->cpu_handle);

    *out = m_dsvs.Alloc(dsv_obj, ObjectType::DSV);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyDSV — releases the committed depth texture resource
// ---------------------------------------------------------------------------

void DX12Backend::DestroyDSV(DXBDSV dsv) {
    auto* obj = m_dsvs.Get(dsv, ObjectType::DSV);
    if (!obj) return;
    // Flush GPU on the owning device before releasing the resource
    auto* dev = m_devices.Get(obj->device_handle, ObjectType::Device);
    if (dev) (void)WaitForGPU(dev, "DestroyDSV: WaitForGPU");
    m_dsvs.Free(dsv, ObjectType::DSV);
    delete obj; // ComPtr<ID3D12Resource> released here
}

} // namespace dxb
