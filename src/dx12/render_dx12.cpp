// src/dx12/render_dx12.cpp
// DX12Backend — render command recording, frame lifecycle, present.
// Phase C.4: BeginFrame, SetRenderTargets, ClearRenderTarget, ClearDepthStencil,
//            SetViewport, SetPipeline, SetVertexBuffer, SetIndexBuffer,
//            SetConstantBuffer, Draw, DrawIndexed, EndFrame, Present.
#include "device_dx12.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

namespace dxb {

static bool IsDeviceLostHRESULT(HRESULT hr) noexcept {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

static DXBResult GuardDeviceLost(DeviceObjectDX12* dev, const char* context) noexcept {
    if (!dev->device_lost) {
        return DXB_OK;
    }

    SetLastError(DXB_E_DEVICE_LOST, "%s: device lost", context);
    return DXB_E_DEVICE_LOST;
}

static DXBResult ReturnHRESULT(DeviceObjectDX12* dev,
                               HRESULT hr,
                               const char* context) noexcept {
    if (IsDeviceLostHRESULT(hr)) {
        if (dev) {
            dev->device_lost = true;
        }
        SetLastErrorFromHR(hr, context);
        return DXB_E_DEVICE_LOST;
    }

    SetLastErrorFromHR(hr, context);
    return MapHRESULT(hr);
}

// ---------------------------------------------------------------------------
// Internal helper — resource barrier transition
// (Defined here for render_dx12.cpp; swapchain_dx12.cpp uses its own local copy)
// ---------------------------------------------------------------------------
static void TransitionBarrier(ID3D12GraphicsCommandList* list,
                               ID3D12Resource* resource,
                               D3D12_RESOURCE_STATES from,
                               D3D12_RESOURCE_STATES to) noexcept
{
    D3D12_RESOURCE_BARRIER b = {};
    b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    b.Transition.pResource   = resource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter  = to;
    list->ResourceBarrier(1, &b);
}

// ---------------------------------------------------------------------------
// BeginFrame
// ---------------------------------------------------------------------------

DXBResult DX12Backend::BeginFrame(DXBDevice dev_handle) {
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "BeginFrame: invalid device handle");
        return DXB_E_INVALID_HANDLE;
    }

    // GAP 2: sticky device-lost state — no further GPU work allowed
    DXBResult lost = GuardDeviceLost(dev, "BeginFrame");
    if (lost != DXB_OK) {
        return lost;
    }

    if (dev->frame_in_progress) {
        DXB_LOG_WARN("DX12 BeginFrame rejected because frame %u is already active", dev->frame_index);
        SetLastError(DXB_E_INVALID_STATE, "BeginFrame: frame already in progress");
        return DXB_E_INVALID_STATE;
    }

    uint32_t fi = dev->frame_index;

    // 1. CPU-side fence wait: ensure the previous work on this frame slot is done
    if (dev->fence->GetCompletedValue() < dev->fence_values[fi]) {
        HRESULT hr = dev->fence->SetEventOnCompletion(dev->fence_values[fi], dev->fence_event);
        if (FAILED(hr)) {
            return ReturnHRESULT(dev, hr, "BeginFrame: fence SetEventOnCompletion");
        }
        WaitForSingleObject(dev->fence_event, INFINITE);
    }

    // 2. Reset allocator for this frame slot
    HRESULT hr = dev->cmd_allocators[fi]->Reset();
    if (FAILED(hr)) {
        return ReturnHRESULT(dev, hr, "BeginFrame: cmd_allocator Reset");
    }

    // 3. Reset command list — re-enters Recording state with no pipeline set
    hr = dev->cmd_list->Reset(dev->cmd_allocators[fi].Get(), nullptr);
    if (FAILED(hr)) {
        return ReturnHRESULT(dev, hr, "BeginFrame: cmd_list Reset");
    }

    // 4. Transition back buffer: PRESENT → RENDER_TARGET (if a swap chain is active)
    if (dev->active_sc != DXBRIDGE_NULL_HANDLE) {
        auto* sc = m_swap_chains.Get(dev->active_sc, ObjectType::SwapChain);
        if (sc && sc->back_buffers[fi]) {
            TransitionBarrier(dev->cmd_list.Get(),
                              sc->back_buffers[fi].Get(),
                              D3D12_RESOURCE_STATE_PRESENT,
                              D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
    }

    // GAP 1: mark frame in progress — ResizeSwapChain will reject during this window
    dev->frame_in_progress = true;
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// SetRenderTargets
// ---------------------------------------------------------------------------

DXBResult DX12Backend::SetRenderTargets(DXBDevice dev_handle, DXBRTV rtv, DXBDSV dsv) {
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetRenderTargets: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "SetRenderTargets");
    if (lost != DXB_OK) {
        return lost;
    }

    auto* rtv_obj = m_rtvs.Get(rtv, ObjectType::RTV);
    if (!rtv_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetRenderTargets: invalid RTV");
        return DXB_E_INVALID_HANDLE;
    }

    if (dsv != DXBRIDGE_NULL_HANDLE) {
        auto* dsv_obj = m_dsvs.Get(dsv, ObjectType::DSV);
        if (!dsv_obj) {
            SetLastError(DXB_E_INVALID_HANDLE, "SetRenderTargets: invalid DSV");
            return DXB_E_INVALID_HANDLE;
        }
        dev->cmd_list->OMSetRenderTargets(1, &rtv_obj->cpu_handle, FALSE, &dsv_obj->cpu_handle);
    } else {
        dev->cmd_list->OMSetRenderTargets(1, &rtv_obj->cpu_handle, FALSE, nullptr);
    }

    return DXB_OK;
}

// ---------------------------------------------------------------------------
// ClearRenderTarget
// ---------------------------------------------------------------------------

DXBResult DX12Backend::ClearRenderTarget(DXBRTV rtv, const float rgba[4]) {
    auto* rtv_obj = m_rtvs.Get(rtv, ObjectType::RTV);
    if (!rtv_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "ClearRenderTarget: invalid RTV");
        return DXB_E_INVALID_HANDLE;
    }

    // RTVObjectDX12 stores the device handle so we can find the command list
    auto* dev = m_devices.Get(rtv_obj->device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "ClearRenderTarget: device gone");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "ClearRenderTarget");
    if (lost != DXB_OK) {
        return lost;
    }

    dev->cmd_list->ClearRenderTargetView(rtv_obj->cpu_handle, rgba, 0, nullptr);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// ClearDepthStencil
// ---------------------------------------------------------------------------

DXBResult DX12Backend::ClearDepthStencil(DXBDSV dsv, float depth, uint8_t stencil) {
    auto* dsv_obj = m_dsvs.Get(dsv, ObjectType::DSV);
    if (!dsv_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "ClearDepthStencil: invalid DSV");
        return DXB_E_INVALID_HANDLE;
    }

    auto* dev = m_devices.Get(dsv_obj->device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "ClearDepthStencil: device gone");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "ClearDepthStencil");
    if (lost != DXB_OK) {
        return lost;
    }

    dev->cmd_list->ClearDepthStencilView(
        dsv_obj->cpu_handle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        depth,
        stencil,
        0, nullptr);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// SetViewport
// ---------------------------------------------------------------------------

DXBResult DX12Backend::SetViewport(DXBDevice dev_handle, const DXBViewport* vp) {
    if (!vp) {
        SetLastError(DXB_E_INVALID_ARG, "SetViewport: null viewport");
        return DXB_E_INVALID_ARG;
    }

    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetViewport: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "SetViewport");
    if (lost != DXB_OK) {
        return lost;
    }

    D3D12_VIEWPORT d3d_vp = {};
    d3d_vp.TopLeftX = vp->x;
    d3d_vp.TopLeftY = vp->y;
    d3d_vp.Width    = vp->width;
    d3d_vp.Height   = vp->height;
    d3d_vp.MinDepth = vp->min_depth;
    d3d_vp.MaxDepth = vp->max_depth;

    // Always set a matching scissor rect (full viewport)
    D3D12_RECT scissor = {
        0, 0,
        static_cast<LONG>(vp->x + vp->width),
        static_cast<LONG>(vp->y + vp->height)
    };

    dev->cmd_list->RSSetViewports(1, &d3d_vp);
    dev->cmd_list->RSSetScissorRects(1, &scissor);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// SetPipeline
// ---------------------------------------------------------------------------

DXBResult DX12Backend::SetPipeline(DXBDevice dev_handle, DXBPipeline pipeline_handle) {
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetPipeline: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "SetPipeline");
    if (lost != DXB_OK) {
        return lost;
    }

    auto* pso_obj = m_pipelines.Get(pipeline_handle, ObjectType::Pipeline);
    if (!pso_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetPipeline: invalid pipeline");
        return DXB_E_INVALID_HANDLE;
    }

    dev->cmd_list->SetGraphicsRootSignature(pso_obj->root_sig);
    dev->cmd_list->SetPipelineState(pso_obj->pso.Get());
    // Fine-grained topology (e.g. D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
    // Different from PrimitiveTopologyType in PSO desc (TRIANGLE/LINE/POINT)
    dev->cmd_list->IASetPrimitiveTopology(pso_obj->topology);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// SetVertexBuffer
// ---------------------------------------------------------------------------

DXBResult DX12Backend::SetVertexBuffer(DXBDevice dev_handle, DXBBuffer buf_handle,
                                        uint32_t stride, uint32_t offset)
{
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetVertexBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "SetVertexBuffer");
    if (lost != DXB_OK) {
        return lost;
    }

    auto* buf_obj = m_buffers.Get(buf_handle, ObjectType::Buffer);
    if (!buf_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetVertexBuffer: invalid buffer");
        return DXB_E_INVALID_HANDLE;
    }
    if (offset >= buf_obj->byte_size) {
        SetLastError(DXB_E_INVALID_ARG, "SetVertexBuffer: offset exceeds buffer size");
        return DXB_E_INVALID_ARG;
    }

    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = buf_obj->resource->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes    = buf_obj->byte_size - offset;
    vbv.StrideInBytes  = stride;
    dev->cmd_list->IASetVertexBuffers(0, 1, &vbv);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// SetIndexBuffer
// ---------------------------------------------------------------------------

DXBResult DX12Backend::SetIndexBuffer(DXBDevice dev_handle, DXBBuffer buf_handle,
                                       DXBFormat fmt, uint32_t offset)
{
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetIndexBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "SetIndexBuffer");
    if (lost != DXB_OK) {
        return lost;
    }

    auto* buf_obj = m_buffers.Get(buf_handle, ObjectType::Buffer);
    if (!buf_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetIndexBuffer: invalid buffer");
        return DXB_E_INVALID_HANDLE;
    }
    if (offset >= buf_obj->byte_size) {
        SetLastError(DXB_E_INVALID_ARG, "SetIndexBuffer: offset exceeds buffer size");
        return DXB_E_INVALID_ARG;
    }

    DXGI_FORMAT dxgi_fmt = DXBFormatToDXGI(fmt);
    if (dxgi_fmt == DXGI_FORMAT_UNKNOWN) {
        SetLastError(DXB_E_INVALID_ARG, "SetIndexBuffer: unsupported format value %u", (unsigned)fmt);
        return DXB_E_INVALID_ARG;
    }

    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = buf_obj->resource->GetGPUVirtualAddress() + offset;
    ibv.SizeInBytes    = buf_obj->byte_size - offset;
    ibv.Format         = dxgi_fmt;
    dev->cmd_list->IASetIndexBuffer(&ibv);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// SetConstantBuffer — Phase 1 no-op (empty root signature has no CBV slots)
// ---------------------------------------------------------------------------

DXBResult DX12Backend::SetConstantBuffer(DXBDevice dev_handle, uint32_t slot, DXBBuffer buf) {
    // Validate handles so callers writing backend-agnostic code don't get false success
    // on obviously invalid handles, but do silently succeed for valid handles.
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetConstantBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "SetConstantBuffer");
    if (lost != DXB_OK) {
        return lost;
    }
    (void)slot;
    (void)buf;
    // Phase 1: empty root signature has no CBV slots — silently no-op.
    // Phase 2: SetGraphicsRootConstantBufferView(slot, GPU virtual address)
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

DXBResult DX12Backend::Draw(DXBDevice dev_handle, uint32_t count, uint32_t start) {
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "Draw: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    DXBResult lost = GuardDeviceLost(dev, "Draw");
    if (lost != DXB_OK) {
        return lost;
    }
    dev->cmd_list->DrawInstanced(count, 1, start, 0);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DrawIndexed
// ---------------------------------------------------------------------------

DXBResult DX12Backend::DrawIndexed(DXBDevice dev_handle, uint32_t count,
                                    uint32_t start, int32_t base)
{
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "DrawIndexed: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    DXBResult lost = GuardDeviceLost(dev, "DrawIndexed");
    if (lost != DXB_OK) {
        return lost;
    }
    dev->cmd_list->DrawIndexedInstanced(count, 1, start, base, 0);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// EndFrame — barrier back to PRESENT, close + execute + signal fence
// ---------------------------------------------------------------------------

DXBResult DX12Backend::EndFrame(DXBDevice dev_handle) {
    auto* dev = m_devices.Get(dev_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "EndFrame: invalid device handle");
        return DXB_E_INVALID_HANDLE;
    }

    // GAP 2: sticky device-lost state — no further GPU work allowed
    DXBResult lost = GuardDeviceLost(dev, "EndFrame");
    if (lost != DXB_OK) {
        return lost;
    }

    if (!dev->frame_in_progress) {
        SetLastError(DXB_E_INVALID_STATE, "EndFrame: no frame in progress");
        return DXB_E_INVALID_STATE;
    }

    uint32_t fi = dev->frame_index;

    // 1. Transition back buffer: RENDER_TARGET → PRESENT
    if (dev->active_sc != DXBRIDGE_NULL_HANDLE) {
        auto* sc = m_swap_chains.Get(dev->active_sc, ObjectType::SwapChain);
        if (sc && sc->back_buffers[fi]) {
            TransitionBarrier(dev->cmd_list.Get(),
                              sc->back_buffers[fi].Get(),
                              D3D12_RESOURCE_STATE_RENDER_TARGET,
                              D3D12_RESOURCE_STATE_PRESENT);
        }
    }

    // 2. Close command list
    HRESULT hr = dev->cmd_list->Close();
    if (FAILED(hr)) {
        dev->frame_in_progress = false;
        return ReturnHRESULT(dev, hr, "EndFrame: cmd_list Close");
    }

    // 3. Execute
    ID3D12CommandList* lists[] = { dev->cmd_list.Get() };
    dev->cmd_queue->ExecuteCommandLists(1, lists);

    // 4. Signal fence with incremented value for this frame slot
    dev->fence_values[fi]++;
    hr = dev->cmd_queue->Signal(dev->fence.Get(), dev->fence_values[fi]);
    if (FAILED(hr)) {
        dev->frame_in_progress = false;
        return ReturnHRESULT(dev, hr, "EndFrame: Signal");
    }

    // GAP 1: frame is now complete — allow ResizeSwapChain again
    dev->frame_in_progress = false;
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// Present — present and update frame index
// ---------------------------------------------------------------------------

DXBResult DX12Backend::Present(DXBSwapChain sc_handle, uint32_t sync_interval) {
    auto* sc_obj = m_swap_chains.Get(sc_handle, ObjectType::SwapChain);
    if (!sc_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "Present: invalid swap chain");
        return DXB_E_INVALID_HANDLE;
    }

    // GAP 2: sticky device-lost — check via the swap chain's device
    auto* dev = m_devices.Get(sc_obj->device_handle, ObjectType::Device);
    if (dev) {
        DXBResult lost = GuardDeviceLost(dev, "Present");
        if (lost != DXB_OK) {
            return lost;
        }

    if (dev->frame_in_progress) {
        DXB_LOG_WARN("DX12 Present rejected while frame %u is still in progress", dev->frame_index);
        SetLastError(DXB_E_INVALID_STATE, "Present: called while frame is in progress");
        return DXB_E_INVALID_STATE;
    }
    }

    HRESULT hr = sc_obj->swapchain->Present(sync_interval, 0);
    if (FAILED(hr)) {
        return ReturnHRESULT(dev, hr, "IDXGISwapChain3::Present");
    }

    // Update frame index AFTER Present — IDXGISwapChain3 advances the back buffer index here
    if (dev) {
        dev->frame_index = sc_obj->swapchain->GetCurrentBackBufferIndex();
    }

    return DXB_OK;
}

} // namespace dxb
