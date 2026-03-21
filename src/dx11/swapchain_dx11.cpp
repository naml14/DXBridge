// src/dx11/swapchain_dx11.cpp
// DX11Backend — swap chain, back buffer, RTV, DSV.
#include "device_dx11.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>

using Microsoft::WRL::ComPtr;

namespace dxb {

// ---------------------------------------------------------------------------
// CreateSwapChain
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreateSwapChain(DXBDevice device,
                                        const DXBSwapChainDesc* desc,
                                        DXBSwapChain* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateSwapChain: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateSwapChain: invalid device handle");
        return DXB_E_INVALID_HANDLE;
    }
    if (!desc->hwnd) {
        SetLastError(DXB_E_INVALID_ARG, "CreateSwapChain: hwnd is null");
        return DXB_E_INVALID_ARG;
    }

    DXGI_FORMAT format = DXBFormatToDXGI(desc->format);
    if (format == DXGI_FORMAT_UNKNOWN) {
        SetLastError(DXB_E_INVALID_ARG, "CreateSwapChain: unsupported format value %u", (unsigned)desc->format);
        return DXB_E_INVALID_ARG;
    }

    DXGI_SWAP_CHAIN_DESC1 sc_desc = {};
    sc_desc.Width       = desc->width;
    sc_desc.Height      = desc->height;
    sc_desc.Format      = format;
    sc_desc.SampleDesc.Count   = 1;
    sc_desc.SampleDesc.Quality = 0;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = (desc->buffer_count >= 2) ? desc->buffer_count : 2;
    sc_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc_desc.Scaling     = DXGI_SCALING_STRETCH;
    sc_desc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;
    sc_desc.Flags       = 0;

    ComPtr<IDXGISwapChain1> swapchain;
    HRESULT hr = m_factory->CreateSwapChainForHwnd(
        dev_obj->device.Get(),
        static_cast<HWND>(desc->hwnd),
        &sc_desc,
        nullptr,        // no fullscreen desc
        nullptr,        // no restrict-to-output
        swapchain.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "IDXGIFactory2::CreateSwapChainForHwnd");
        return MapHRESULT(hr);
    }

    // Disable Alt+Enter fullscreen shortcut (library users handle this themselves)
    m_factory->MakeWindowAssociation(static_cast<HWND>(desc->hwnd),
                                     DXGI_MWA_NO_ALT_ENTER);

    auto* obj          = new SwapChainObjectDX11();
    obj->swapchain     = swapchain;
    obj->device_ref    = dev_obj->device;
    obj->context_ref   = dev_obj->context;
    obj->width         = desc->width;
    obj->height        = desc->height;
    obj->format        = desc->format;

    *out = m_swapchains.Alloc(obj, ObjectType::SwapChain);
    return DXB_OK;
}

void DX11Backend::DestroySwapChain(DXBSwapChain sc) {
    auto* obj = m_swapchains.Get(sc, ObjectType::SwapChain);
    if (!obj) return;
    m_swapchains.Free(sc, ObjectType::SwapChain);
    delete obj;
}

DXBResult DX11Backend::ResizeSwapChain(DXBSwapChain sc, uint32_t w, uint32_t h) {
    auto* obj = m_swapchains.Get(sc, ObjectType::SwapChain);
    if (!obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "ResizeSwapChain: invalid handle");
        return DXB_E_INVALID_HANDLE;
    }

    // Caller must have released all RTV/back-buffer references before calling.
    HRESULT hr = obj->swapchain->ResizeBuffers(
        0,                          // keep existing buffer count
        w, h,
        DXGI_FORMAT_UNKNOWN,        // keep existing format
        0
    );
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "IDXGISwapChain1::ResizeBuffers");
        return MapHRESULT(hr);
    }
    obj->width  = w;
    obj->height = h;
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// GetBackBuffer
// ---------------------------------------------------------------------------

DXBResult DX11Backend::GetBackBuffer(DXBSwapChain sc, DXBRenderTarget* out) {
    if (!out) {
        SetLastError(DXB_E_INVALID_ARG, "GetBackBuffer: null out");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* sc_obj = m_swapchains.Get(sc, ObjectType::SwapChain);
    if (!sc_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "GetBackBuffer: invalid swapchain");
        return DXB_E_INVALID_HANDLE;
    }

    ComPtr<ID3D11Texture2D> backbuf;
    HRESULT hr = sc_obj->swapchain->GetBuffer(
        0, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backbuf.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "IDXGISwapChain1::GetBuffer");
        return MapHRESULT(hr);
    }

    auto* rt_obj          = new RenderTargetObjectDX11();
    rt_obj->texture       = backbuf;
    rt_obj->from_backbuffer = true;

    *out = m_render_targets.Alloc(rt_obj, ObjectType::RenderTarget);
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// CreateRenderTargetView
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreateRenderTargetView(DXBDevice device,
                                               DXBRenderTarget rt,
                                               DXBRTV* out)
{
    if (!out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateRenderTargetView: null out");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateRenderTargetView: invalid device");
        return DXB_E_INVALID_HANDLE;
    }
    auto* rt_obj = m_render_targets.Get(rt, ObjectType::RenderTarget);
    if (!rt_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateRenderTargetView: invalid render target");
        return DXB_E_INVALID_HANDLE;
    }

    ComPtr<ID3D11RenderTargetView> rtv;
    HRESULT hr = dev_obj->device->CreateRenderTargetView(
        rt_obj->texture.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateRenderTargetView");
        return MapHRESULT(hr);
    }

    auto* rtv_obj         = new RTVObjectDX11();
    rtv_obj->rtv          = rtv;
    rtv_obj->context_ref  = dev_obj->context;

    *out = m_rtvs.Alloc(rtv_obj, ObjectType::RTV);
    return DXB_OK;
}

void DX11Backend::DestroyRTV(DXBRTV rtv) {
    auto* obj = m_rtvs.Get(rtv, ObjectType::RTV);
    if (!obj) return;
    m_rtvs.Free(rtv, ObjectType::RTV);
    delete obj;
}

// ---------------------------------------------------------------------------
// CreateDepthStencilView
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreateDepthStencilView(DXBDevice device,
                                               const DXBDepthStencilDesc* desc,
                                               DXBDSV* out)
{
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateDepthStencilView: null argument");
        return DXB_E_INVALID_ARG;
    }
    *out = DXBRIDGE_NULL_HANDLE;

    auto* dev_obj = m_devices.Get(device, ObjectType::Device);
    if (!dev_obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "CreateDepthStencilView: invalid device");
        return DXB_E_INVALID_HANDLE;
    }

    DXGI_FORMAT tex_format = DXBFormatToTypeless(desc->format);
    DXGI_FORMAT dsv_format = DXBFormatToDXGI(desc->format);
    if (tex_format == DXGI_FORMAT_UNKNOWN || dsv_format == DXGI_FORMAT_UNKNOWN) {
        SetLastError(DXB_E_INVALID_ARG, "CreateDepthStencilView: unsupported format");
        return DXB_E_INVALID_ARG;
    }

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width              = desc->width;
    tex_desc.Height             = desc->height;
    tex_desc.MipLevels          = 1;
    tex_desc.ArraySize          = 1;
    tex_desc.Format             = tex_format;
    tex_desc.SampleDesc.Count   = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage              = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    tex_desc.CPUAccessFlags     = 0;
    tex_desc.MiscFlags          = 0;

    ComPtr<ID3D11Texture2D> depth_tex;
    HRESULT hr = dev_obj->device->CreateTexture2D(
        &tex_desc, nullptr, depth_tex.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateTexture2D (depth)");
        return MapHRESULT(hr);
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format             = dsv_format;
    dsv_desc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11DepthStencilView> dsv;
    hr = dev_obj->device->CreateDepthStencilView(
        depth_tex.Get(), &dsv_desc, dsv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "ID3D11Device::CreateDepthStencilView");
        return MapHRESULT(hr);
    }

    auto* obj           = new DSVObjectDX11();
    obj->texture        = depth_tex;
    obj->dsv            = dsv;
    obj->context_ref    = dev_obj->context;
    obj->width          = desc->width;
    obj->height         = desc->height;

    *out = m_dsvs.Alloc(obj, ObjectType::DSV);
    return DXB_OK;
}

void DX11Backend::DestroyDSV(DXBDSV dsv) {
    auto* obj = m_dsvs.Get(dsv, ObjectType::DSV);
    if (!obj) return;
    m_dsvs.Free(dsv, ObjectType::DSV);
    delete obj;
}

// ---------------------------------------------------------------------------
// Present
// ---------------------------------------------------------------------------

DXBResult DX11Backend::Present(DXBSwapChain sc, uint32_t sync_interval) {
    auto* obj = m_swapchains.Get(sc, ObjectType::SwapChain);
    if (!obj) {
        SetLastError(DXB_E_INVALID_HANDLE, "Present: invalid swapchain");
        return DXB_E_INVALID_HANDLE;
    }

    HRESULT hr = obj->swapchain->Present(sync_interval, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        SetLastErrorFromHR(hr, "IDXGISwapChain1::Present (device removed)");
        return DXB_E_DEVICE_LOST;
    }
    if (FAILED(hr)) {
        SetLastErrorFromHR(hr, "IDXGISwapChain1::Present");
        return MapHRESULT(hr);
    }
    return DXB_OK;
}

} // namespace dxb
