// src/dx11/render_dx11.cpp
// DX11Backend — render command recording and submission.
#include "device_dx11.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d11.h>

using Microsoft::WRL::ComPtr;

namespace dxb {

// ---------------------------------------------------------------------------
// BeginFrame / EndFrame — no-ops for DX11 (DX12 needs cmd list reset/submit)
// ---------------------------------------------------------------------------

DXBResult DX11Backend::BeginFrame(DXBDevice device) {
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "BeginFrame: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    return DXB_OK; // no-op for DX11
}

DXBResult DX11Backend::EndFrame(DXBDevice device) {
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "EndFrame: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    return DXB_OK; // no-op for DX11
}

// ---------------------------------------------------------------------------
// SetRenderTargets
// ---------------------------------------------------------------------------

DXBResult DX11Backend::SetRenderTargets(DXBDevice device, DXBRTV rtv, DXBDSV dsv) {
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetRenderTargets: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    ID3D11RenderTargetView* rtv_ptr = nullptr;
    ID3D11DepthStencilView* dsv_ptr = nullptr;

    if (rtv != DXBRIDGE_NULL_HANDLE) {
        auto* rtv_obj = m_rtvs.Get(rtv, ObjectType::RTV);
        if (!rtv_obj) {
            SetLastError(DXB_E_INVALID_HANDLE, "SetRenderTargets: invalid RTV");
            return DXB_E_INVALID_HANDLE;
        }
        rtv_ptr = rtv_obj->rtv.Get();
    }

    if (dsv != DXBRIDGE_NULL_HANDLE) {
        auto* dsv_obj = m_dsvs.Get(dsv, ObjectType::DSV);
        if (!dsv_obj) {
            SetLastError(DXB_E_INVALID_HANDLE, "SetRenderTargets: invalid DSV");
            return DXB_E_INVALID_HANDLE;
        }
        dsv_ptr = dsv_obj->dsv.Get();
    }

    dev_obj->context->OMSetRenderTargets(1, &rtv_ptr, dsv_ptr);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// Clear operations
// ---------------------------------------------------------------------------

DXBResult DX11Backend::ClearRenderTarget(DXBRTV rtv, const float rgba[4]) {
    auto* rtv_obj = m_rtvs.Get(rtv, ObjectType::RTV);
    if (!rtv_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "ClearRenderTarget: invalid RTV");
        return DXB_E_INVALID_HANDLE;
    }
    rtv_obj->context_ref->ClearRenderTargetView(rtv_obj->rtv.Get(), rgba);
    return DXB_OK;
}

DXBResult DX11Backend::ClearDepthStencil(DXBDSV dsv, float depth, uint8_t stencil) {
    auto* dsv_obj = m_dsvs.Get(dsv, ObjectType::DSV);
    if (!dsv_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "ClearDepthStencil: invalid DSV");
        return DXB_E_INVALID_HANDLE;
    }
    dsv_obj->context_ref->ClearDepthStencilView(
        dsv_obj->dsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        depth,
        stencil);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// State binding
// ---------------------------------------------------------------------------

DXBResult DX11Backend::SetViewport(DXBDevice device, const DXBViewport* vp) {
    if (!vp) {
        SetLastError(DXB_E_INVALID_ARG, "SetViewport: vp is null");
        return DXB_E_INVALID_ARG;
    }
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetViewport: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    D3D11_VIEWPORT d3d_vp = {};
    d3d_vp.TopLeftX = vp->x;
    d3d_vp.TopLeftY = vp->y;
    d3d_vp.Width    = vp->width;
    d3d_vp.Height   = vp->height;
    d3d_vp.MinDepth = vp->min_depth;
    d3d_vp.MaxDepth = vp->max_depth;

    dev_obj->context->RSSetViewports(1, &d3d_vp);
    return DXB_OK;
}

DXBResult DX11Backend::SetPipeline(DXBDevice device, DXBPipeline pipeline) {
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetPipeline: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    auto* pipe = m_pipelines.Get(pipeline, ObjectType::Pipeline);
    if (!pipe) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetPipeline: invalid pipeline");
        return DXB_E_INVALID_HANDLE;
    }

    ID3D11DeviceContext* ctx = dev_obj->context.Get();

    // IA — input assembler
    ctx->IASetInputLayout(pipe->input_layout.Get());
    ctx->IASetPrimitiveTopology(pipe->topology);

    // VS / PS
    ctx->VSSetShader(pipe->vs.Get(), nullptr, 0);
    ctx->PSSetShader(pipe->ps.Get(), nullptr, 0);

    // RS — rasterizer
    ctx->RSSetState(pipe->rasterizer.Get());

    // OM — output merger (blend + depth)
    const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx->OMSetBlendState(pipe->blend.Get(), blend_factor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(pipe->depth_stencil.Get(), 0);

    return DXB_OK;
}

// ---------------------------------------------------------------------------
// Vertex / index / constant buffer binding
// ---------------------------------------------------------------------------

DXBResult DX11Backend::SetVertexBuffer(DXBDevice device, DXBBuffer buf,
                                        uint32_t stride, uint32_t offset)
{
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetVertexBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    auto* buf_obj = m_buffers.Get(buf, ObjectType::Buffer);
    if (!buf_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetVertexBuffer: invalid buffer");
        return DXB_E_INVALID_HANDLE;
    }

    ID3D11Buffer* vb  = buf_obj->buffer.Get();
    dev_obj->context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    return DXB_OK;
}

DXBResult DX11Backend::SetIndexBuffer(DXBDevice device, DXBBuffer buf,
                                       DXBFormat fmt, uint32_t offset)
{
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetIndexBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    auto* buf_obj = m_buffers.Get(buf, ObjectType::Buffer);
    if (!buf_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetIndexBuffer: invalid buffer");
        return DXB_E_INVALID_HANDLE;
    }

    DXGI_FORMAT dxgi_fmt = DXBFormatToDXGI(fmt);
    if (dxgi_fmt == DXGI_FORMAT_UNKNOWN) {
        SetLastError(DXB_E_INVALID_ARG, "SetIndexBuffer: unsupported format");
        return DXB_E_INVALID_ARG;
    }

    dev_obj->context->IASetIndexBuffer(buf_obj->buffer.Get(), dxgi_fmt, offset);
    return DXB_OK;
}

DXBResult DX11Backend::SetConstantBuffer(DXBDevice device, uint32_t slot, DXBBuffer buf) {
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetConstantBuffer: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    auto* buf_obj = m_buffers.Get(buf, ObjectType::Buffer);
    if (!buf_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "SetConstantBuffer: invalid buffer");
        return DXB_E_INVALID_HANDLE;
    }

    ID3D11Buffer* cb = buf_obj->buffer.Get();
    // Bind to both VS and PS for convenience
    dev_obj->context->VSSetConstantBuffers(slot, 1, &cb);
    dev_obj->context->PSSetConstantBuffers(slot, 1, &cb);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// Draw calls
// ---------------------------------------------------------------------------

DXBResult DX11Backend::Draw(DXBDevice device, uint32_t count, uint32_t start) {
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "Draw: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    dev_obj->context->Draw(count, start);
    return DXB_OK;
}

DXBResult DX11Backend::DrawIndexed(DXBDevice device, uint32_t count,
                                    uint32_t start, int32_t base)
{
    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "DrawIndexed: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    dev_obj->context->DrawIndexed(count, start, base);
    return DXB_OK;
}

} // namespace dxb
