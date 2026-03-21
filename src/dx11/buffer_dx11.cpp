// src/dx11/buffer_dx11.cpp
// DX11Backend — GPU buffer creation and data upload.
#include "device_dx11.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d11.h>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace dxb {

// ---------------------------------------------------------------------------
// CreateBuffer
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreateBuffer(DXBDevice device,
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

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth    = desc->byte_size;
    bd.MiscFlags    = 0;
    bd.StructureByteStride = 0;

    // Determine bind flags
    if (desc->flags & DXB_BUFFER_FLAG_VERTEX)   bd.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    if (desc->flags & DXB_BUFFER_FLAG_INDEX)    bd.BindFlags |= D3D11_BIND_INDEX_BUFFER;
    if (desc->flags & DXB_BUFFER_FLAG_CONSTANT) {
        bd.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
        // Constant buffers must be 16-byte aligned
        bd.ByteWidth = (bd.ByteWidth + 15u) & ~15u;
    }

    // Dynamic vs. default
    if (desc->flags & DXB_BUFFER_FLAG_DYNAMIC) {
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else {
        bd.Usage          = D3D11_USAGE_DEFAULT;
        bd.CPUAccessFlags = 0;
    }

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = dev_obj->device->CreateBuffer(&bd, nullptr,
                                               buffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateBuffer");
        return MapHRESULT(hr);
    }

    auto* obj           = new BufferObjectDX11();
    obj->buffer         = buffer;
    obj->device_ref     = dev_obj->device;
    obj->context_ref    = dev_obj->context;
    obj->flags          = desc->flags;
    obj->byte_size      = bd.ByteWidth; // may be padded for CBs

    *out = m_buffers.Alloc(obj, ObjectType::Buffer);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// UploadData
// ---------------------------------------------------------------------------

DXBResult DX11Backend::UploadData(DXBBuffer buf,
                                   const void* data,
                                   uint32_t size)
{
    if (!data) {
        SetLastError(DXB_E_INVALID_ARG, "UploadData: data is null");
        return DXB_E_INVALID_ARG;
    }

    auto* obj = m_buffers.Get(buf, ObjectType::Buffer);
    if (!obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "UploadData: invalid buffer");
        return DXB_E_INVALID_HANDLE;
    }

    if (size > obj->byte_size) {
        SetLastError(DXB_E_INVALID_ARG,
                     "UploadData: data size (%u) exceeds buffer size (%u)",
                     size, obj->byte_size);
        return DXB_E_INVALID_ARG;
    }

    if (obj->flags & DXB_BUFFER_FLAG_DYNAMIC) {
        // Map/Unmap path for dynamic buffers
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = obj->context_ref->Map(obj->buffer.Get(), 0,
                                            D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            SetLastErrorFromHR(hr, "ID3D11DeviceContext::Map");
            return MapHRESULT(hr);
        }
        memcpy(mapped.pData, data, size);
        obj->context_ref->Unmap(obj->buffer.Get(), 0);
    } else {
        // Static buffer: use a staging buffer + CopyResource
        D3D11_BUFFER_DESC staging_desc = {};
        staging_desc.ByteWidth      = obj->byte_size;
        staging_desc.Usage          = D3D11_USAGE_STAGING;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        staging_desc.BindFlags      = 0;

        D3D11_SUBRESOURCE_DATA init_data = {};
        init_data.pSysMem = data;

        ComPtr<ID3D11Buffer> staging;
        HRESULT hr = obj->device_ref->CreateBuffer(&staging_desc, &init_data,
                                                    staging.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            SetLastErrorFromHR(hr, "ID3D11Device::CreateBuffer (staging)");
            return MapHRESULT(hr);
        }
        obj->context_ref->CopyResource(obj->buffer.Get(), staging.Get());
    }
    return DXB_OK;
}

void DX11Backend::DestroyBuffer(DXBBuffer buf) {
    auto* obj = m_buffers.Get(buf, ObjectType::Buffer);
    if (!obj) return;
    m_buffers.Free(buf, ObjectType::Buffer);
    delete obj;
}

} // namespace dxb
