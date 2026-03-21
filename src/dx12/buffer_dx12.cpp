// src/dx12/buffer_dx12.cpp
// DX12Backend — buffer creation (upload heap + persistent Map), upload, destroy.
// Phase C.1: CreateBuffer, UploadData, DestroyBuffer.
#include "device_dx12.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d12.h>

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

} // namespace

namespace dxb {

// ---------------------------------------------------------------------------
// CreateBuffer — upload heap with persistent Map
// ---------------------------------------------------------------------------

DXBResult DX12Backend::CreateBuffer(DXBDevice device_handle,
                                     const DXBBufferDesc* desc,
                                     DXBBuffer* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateBuffer: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    if (desc->byte_size == 0) {
        SetLastError(DXB_E_INVALID_ARG, "CreateBuffer: byte_size is 0");
        return DXB_E_INVALID_ARG;
    }

    auto* dev = m_devices.Get(device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateBuffer: invalid device handle");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "CreateBuffer");
    if (lost != DXB_OK) {
        return lost;
    }

    // Phase 1: all buffers on upload heap (CPU-writable, GPU-readable)
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Alignment          = 0;
    rd.Width              = desc->byte_size;
    rd.Height             = 1;
    rd.DepthOrArraySize   = 1;
    rd.MipLevels          = 1;
    rd.Format             = DXGI_FORMAT_UNKNOWN; // must be UNKNOWN for buffers
    rd.SampleDesc         = { 1, 0 };
    rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags              = D3D12_RESOURCE_FLAG_NONE;

    auto* buf_obj = new BufferObjectDX12();
    // Upload heap initial state MUST be GENERIC_READ — no other state is valid
    HRESULT hr = dev->device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buf_obj->resource));
    if (FAILED(hr)) {
        delete buf_obj;
        return ReturnHRESULT(dev, hr, "CreateCommittedResource (buffer)");
    }

    // Persistent Map — never Unmap for upload heap (valid for the resource lifetime)
    D3D12_RANGE read_range = { 0, 0 }; // we never read from CPU
    hr = buf_obj->resource->Map(0, &read_range, &buf_obj->mapped_ptr);
    if (FAILED(hr)) {
        delete buf_obj;
        return ReturnHRESULT(dev, hr, "ID3D12Resource::Map (buffer)");
    }

    buf_obj->byte_size     = desc->byte_size;
    buf_obj->flags         = desc->flags;
    buf_obj->stride        = desc->stride;
    buf_obj->device_handle = device_handle;

    *out = m_buffers.Alloc(buf_obj, ObjectType::Buffer);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// UploadData — simple memcpy into persistently-mapped upload buffer
// ---------------------------------------------------------------------------

DXBResult DX12Backend::UploadData(DXBBuffer buf_handle, const void* data, uint32_t size) {
    if (!data) {
        SetLastError(DXB_E_INVALID_ARG, "UploadData: null data pointer");
        return DXB_E_INVALID_ARG;
    }

    auto* buf_obj = m_buffers.Get(buf_handle, ObjectType::Buffer);
    if (!buf_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "UploadData: invalid buffer handle");
        return DXB_E_INVALID_HANDLE;
    }

    auto* dev = m_devices.Get(buf_obj->device_handle, ObjectType::Device);
    if (!dev) {
        SetLastError(DXB_E_INVALID_HANDLE, "UploadData: device gone");
        return DXB_E_INVALID_HANDLE;
    }

    DXBResult lost = GuardDeviceLost(dev, "UploadData");
    if (lost != DXB_OK) {
        return lost;
    }

    if (!buf_obj->mapped_ptr) {
        SetLastError(DXB_E_INVALID_STATE, "UploadData: buffer is not mapped");
        return DXB_E_INVALID_STATE;
    }
    if (size > buf_obj->byte_size) {
        SetLastError(DXB_E_INVALID_ARG, "UploadData: size exceeds buffer capacity");
        return DXB_E_INVALID_ARG;
    }

    // Upload heap is persistently mapped — plain memcpy
    memcpy(buf_obj->mapped_ptr, data, size);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// DestroyBuffer — unmap and release resource
// ---------------------------------------------------------------------------

void DX12Backend::DestroyBuffer(DXBBuffer buf_handle) {
    auto* buf_obj = m_buffers.Get(buf_handle, ObjectType::Buffer);
    if (!buf_obj) return;

    // Unmap before releasing (good practice; upload heap can stay mapped but we clean up)
    if (buf_obj->mapped_ptr && buf_obj->resource) {
        buf_obj->resource->Unmap(0, nullptr);
        buf_obj->mapped_ptr = nullptr;
    }

    m_buffers.Free(buf_handle, ObjectType::Buffer);
    delete buf_obj; // ComPtr<ID3D12Resource> released here
}

} // namespace dxb
