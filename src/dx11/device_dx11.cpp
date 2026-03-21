// src/dx11/device_dx11.cpp
// DX11Backend — device lifecycle, adapter enumeration, feature queries.
#include "device_dx11.hpp"
#include "../common/error.hpp"
#include "../common/log.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstring>
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace dxb {

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

DXBResult DX11Backend::Init() {
    if (m_initialized) return DXB_OK;

    // Try debug factory first; fall back to release factory
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
                                    __uuidof(IDXGIFactory2),
                                    reinterpret_cast<void**>(m_factory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory2),
                                reinterpret_cast<void**>(m_factory.ReleaseAndGetAddressOf()));
        if (FAILED(hr)) {
            SetLastErrorFromHR(hr, "CreateDXGIFactory1");
            return MapHRESULT(hr);
        }
    }

    m_initialized = true;
    DXB_LOG_INFO("DX11Backend initialized");
    return DXB_OK;
}

void DX11Backend::Shutdown() {
    // Pools clean up via destructor (ComPtr release chain)
    m_factory.Reset();
    m_initialized = false;
    DXB_LOG_INFO("DX11Backend shutdown");
}

// ---------------------------------------------------------------------------
// Adapter enumeration  (two-call pattern)
// ---------------------------------------------------------------------------

DXBResult DX11Backend::EnumerateAdapters(DXBAdapterInfo* buf, uint32_t* count) {
    if (!count) {
        SetLastError(DXB_E_INVALID_ARG, "EnumerateAdapters: count must not be null");
        return DXB_E_INVALID_ARG;
    }
    if (!m_initialized) {
        SetLastError(DXB_E_NOT_INITIALIZED, "EnumerateAdapters: backend not initialized");
        return DXB_E_NOT_INITIALIZED;
    }

    // Collect adapters
    std::vector<ComPtr<IDXGIAdapter>> adapters;

    // Try IDXGIFactory6 for GPU-preference enumeration
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(m_factory.As(&factory6))) {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter> a;
            HRESULT hr = factory6->EnumAdapterByGpuPreference(
                i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                __uuidof(IDXGIAdapter),
                reinterpret_cast<void**>(a.ReleaseAndGetAddressOf()));
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) break;
            adapters.push_back(a);
        }
    } else {
        // Fallback: IDXGIFactory1::EnumAdapters
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter> a;
            HRESULT hr = m_factory->EnumAdapters(i, a.ReleaseAndGetAddressOf());
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) break;
            adapters.push_back(a);
        }
    }

    uint32_t available = static_cast<uint32_t>(adapters.size());

    // First call: query count
    if (!buf) {
        *count = available;
        return DXB_OK;
    }

    // Second call: fill buffer
    uint32_t to_fill = (*count < available) ? *count : available;
    for (uint32_t i = 0; i < to_fill; ++i) {
        DXGI_ADAPTER_DESC desc = {};
        adapters[i]->GetDesc(&desc);

        DXBAdapterInfo& info = buf[i];
        info.struct_version      = DXBRIDGE_STRUCT_VERSION;
        info.index               = i;
        info.dedicated_video_mem = desc.DedicatedVideoMemory;
        info.dedicated_system_mem= desc.DedicatedSystemMemory;
        info.shared_system_mem   = desc.SharedSystemMemory;

        // Convert WCHAR description to char
        WideCharToMultiByte(CP_UTF8, 0,
                            desc.Description, -1,
                            info.description, sizeof(info.description),
                            nullptr, nullptr);
        info.description[sizeof(info.description) - 1] = '\0';

        // WARP / software adapter check via IDXGIAdapter1::GetDesc1
        {
            ComPtr<IDXGIAdapter1> adapter1;
            info.is_software = 0u;
            if (SUCCEEDED(adapters[i].As(&adapter1))) {
                DXGI_ADAPTER_DESC1 desc1 = {};
                if (SUCCEEDED(adapter1->GetDesc1(&desc1))) {
                    info.is_software = (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? 1u : 0u;
                }
            }
        }
    }
    *count = to_fill;
    return DXB_OK;
}

// ---------------------------------------------------------------------------
// CreateDevice
// ---------------------------------------------------------------------------

DXBResult DX11Backend::CreateDevice(const DXBDeviceDesc* desc, DXBDevice* out) {
    if (!desc || !out) {
        SetLastError(DXB_E_INVALID_ARG, "CreateDevice: null argument");
        return DXB_E_INVALID_ARG;
    }
    if (!m_initialized) {
        SetLastError(DXB_E_NOT_INITIALIZED, "CreateDevice: not initialized");
        return DXB_E_NOT_INITIALIZED;
    }

    *out = DXBRIDGE_NULL_HANDLE;

    // Resolve adapter
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory6> factory6;
    if (desc->adapter_index == 0 && SUCCEEDED(m_factory.As(&factory6))) {
        factory6->EnumAdapterByGpuPreference(
            0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            __uuidof(IDXGIAdapter),
            reinterpret_cast<void**>(adapter.ReleaseAndGetAddressOf()));
    }
    if (!adapter) {
        m_factory->EnumAdapters(desc->adapter_index, adapter.ReleaseAndGetAddressOf());
    }

    // Device creation flags
    UINT flags = 0;
    bool debug_enabled = false;
    if (desc->flags & DXB_DEVICE_FLAG_DEBUG) {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
        debug_enabled = true;
    }

    // Feature level cascade
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    ComPtr<ID3D11Device>        d3d_device;
    ComPtr<ID3D11DeviceContext> d3d_context;
    D3D_FEATURE_LEVEL           chosen_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        adapter.Get(),
        adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        feature_levels, ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        d3d_device.ReleaseAndGetAddressOf(),
        &chosen_level,
        d3d_context.ReleaseAndGetAddressOf()
    );

    if (FAILED(hr)) {
        // Retry without debug flag (debug layer may not be installed)
        if (flags & D3D11_CREATE_DEVICE_DEBUG) {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDevice(
                adapter.Get(),
                adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                flags,
                feature_levels, ARRAYSIZE(feature_levels),
                D3D11_SDK_VERSION,
                d3d_device.ReleaseAndGetAddressOf(),
                &chosen_level,
                d3d_context.ReleaseAndGetAddressOf()
            );
        }
        if (FAILED(hr)) {
            SetLastErrorFromHR(hr, "D3D11CreateDevice");
            return MapHRESULT(hr);
        }
    }

    auto* obj            = new DeviceObjectDX11();
    obj->device          = d3d_device;
    obj->context         = d3d_context;
    obj->adapter         = adapter;
    obj->feature_level   = chosen_level;
    obj->debug_enabled   = debug_enabled;

    *out = m_devices.Alloc(obj, ObjectType::Device);

    DXB_LOG_INFO("DX11 device created (feature level 0x%04X)", (unsigned)chosen_level);
    return DXB_OK;
}

void DX11Backend::DestroyDevice(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return;
    m_devices.Free(device, ObjectType::Device);
    delete obj;
}

uint32_t DX11Backend::GetFeatureLevel(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return 0;
    return static_cast<uint32_t>(obj->feature_level);
}

// ---------------------------------------------------------------------------
// SupportsFeature
// ---------------------------------------------------------------------------

uint32_t DX11Backend::SupportsFeature(uint32_t flag) {
    // DX11 backend never supports DX12 features
    if (flag & DXB_FEATURE_DX12) return 0;
    return 0; // extend as needed
}

// ---------------------------------------------------------------------------
// Native escape hatches
// ---------------------------------------------------------------------------

void* DX11Backend::GetNativeDevice(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return nullptr;
    // Caller must AddRef/Release if they store it
    return static_cast<void*>(obj->device.Get());
}

void* DX11Backend::GetNativeContext(DXBDevice device) {
    auto* obj = m_devices.Get(device, ObjectType::Device);
    if (!obj) return nullptr;
    return static_cast<void*>(obj->context.Get());
}

void DX11Backend::SetLogCallback(DXBLogCallback /*cb*/, void* /*ud*/) {
    // Log callbacks are handled globally via SetLogCallbackInternal
}

} // namespace dxb
