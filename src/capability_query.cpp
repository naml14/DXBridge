// src/capability_query.cpp
// Additive capability-discovery entry point for compatibility-preserving v1 growth.
#include "../include/dxbridge/dxbridge.h"

#include "common/error.hpp"

#include <d3d11.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

struct AdapterEntry {
    ComPtr<IDXGIAdapter1> adapter;
    bool                  is_software = false;
};

bool IsSoftwareAdapter(IDXGIAdapter1* adapter) {
    if (!adapter) {
        return false;
    }

    DXGI_ADAPTER_DESC1 desc = {};
    return SUCCEEDED(adapter->GetDesc1(&desc)) && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
}

bool CreateFactory(ComPtr<IDXGIFactory1>* out_factory1) {
    if (!out_factory1) {
        return false;
    }

    out_factory1->Reset();
    return SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(out_factory1->ReleaseAndGetAddressOf())));
}

bool EnumerateDX11Adapters(std::vector<AdapterEntry>* out_entries) {
    if (!out_entries) {
        return false;
    }

    out_entries->clear();

    ComPtr<IDXGIFactory1> factory1;
    if (!CreateFactory(&factory1)) {
        return false;
    }

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(factory1.As(&factory6))) {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT hr = factory6->EnumAdapterByGpuPreference(
                i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr)) {
                break;
            }

            out_entries->push_back(AdapterEntry{adapter, IsSoftwareAdapter(adapter.Get())});
        }
    } else {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT hr = factory1->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf());
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr)) {
                break;
            }

            out_entries->push_back(AdapterEntry{adapter, IsSoftwareAdapter(adapter.Get())});
        }
    }

    return true;
}

bool EnumerateDX12Adapters(std::vector<AdapterEntry>* out_entries) {
    if (!out_entries) {
        return false;
    }

    out_entries->clear();

    ComPtr<IDXGIFactory1> factory1;
    if (!CreateFactory(&factory1)) {
        return false;
    }

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(factory1.As(&factory6))) {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT hr = factory6->EnumAdapterByGpuPreference(
                i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr)) {
                break;
            }
            if (IsSoftwareAdapter(adapter.Get())) {
                continue;
            }

            out_entries->push_back(AdapterEntry{adapter, false});
        }
    } else {
        for (uint32_t i = 0; ; ++i) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT hr = factory1->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf());
            if (hr == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (FAILED(hr)) {
                break;
            }
            if (IsSoftwareAdapter(adapter.Get())) {
                continue;
            }

            out_entries->push_back(AdapterEntry{adapter, false});
        }
    }

    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(factory1.As(&factory4))) {
        ComPtr<IDXGIAdapter1> warp;
        if (SUCCEEDED(factory4->EnumWarpAdapter(IID_PPV_ARGS(warp.ReleaseAndGetAddressOf())))) {
            out_entries->push_back(AdapterEntry{warp, true});
        }
    }

    return true;
}

bool EnumerateAdaptersForBackend(DXBBackend backend, std::vector<AdapterEntry>* out_entries) {
    switch (backend) {
        case DXB_BACKEND_DX11:
            return EnumerateDX11Adapters(out_entries);
        case DXB_BACKEND_DX12:
            return EnumerateDX12Adapters(out_entries);
        default:
            return false;
    }
}

bool ProbeDX11FeatureLevel(IDXGIAdapter1* adapter, uint32_t* out_feature_level, bool enable_debug) {
    UINT flags = enable_debug ? D3D11_CREATE_DEVICE_DEBUG : 0u;
    const D3D_FEATURE_LEVEL requested_levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL chosen_level = D3D_FEATURE_LEVEL_11_0;
    const HRESULT hr = D3D11CreateDevice(
        adapter,
        adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        requested_levels,
        ARRAYSIZE(requested_levels),
        D3D11_SDK_VERSION,
        device.ReleaseAndGetAddressOf(),
        &chosen_level,
        context.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        return false;
    }

    if (out_feature_level) {
        *out_feature_level = static_cast<uint32_t>(chosen_level);
    }
    return true;
}

bool ProbeDX12FeatureLevel(IDXGIAdapter1* adapter, uint32_t* out_feature_level) {
    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device.ReleaseAndGetAddressOf())))) {
        return false;
    }

    if (!out_feature_level) {
        return true;
    }

    D3D_FEATURE_LEVEL requested_levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels = {};
    feature_levels.NumFeatureLevels = ARRAYSIZE(requested_levels);
    feature_levels.pFeatureLevelsRequested = requested_levels;
    feature_levels.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_FEATURE_LEVELS,
            &feature_levels,
            static_cast<UINT>(sizeof(feature_levels))))) {
        *out_feature_level = static_cast<uint32_t>(feature_levels.MaxSupportedFeatureLevel);
    } else {
        *out_feature_level = static_cast<uint32_t>(D3D_FEATURE_LEVEL_12_0);
    }
    return true;
}

bool ProbeFeatureLevel(DXBBackend backend, IDXGIAdapter1* adapter, uint32_t* out_feature_level, bool enable_debug) {
    switch (backend) {
        case DXB_BACKEND_DX11:
            return ProbeDX11FeatureLevel(adapter, out_feature_level, enable_debug);
        case DXB_BACKEND_DX12:
            if (enable_debug) {
                return false;
            }
            return ProbeDX12FeatureLevel(adapter, out_feature_level);
        default:
            return false;
    }
}

void ResetOutput(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    out_info->struct_version = DXBRIDGE_STRUCT_VERSION;
    out_info->capability = query ? query->capability : 0u;
    out_info->backend = query ? query->backend : DXB_BACKEND_AUTO;
    out_info->adapter_index = query ? query->adapter_index : 0u;
    out_info->supported = 0u;
    out_info->value_u32 = 0u;
    out_info->value_u64 = 0u;
    std::memset(out_info->reserved, 0, sizeof(out_info->reserved));
}

DXBResult QueryBackendAvailable(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    std::vector<AdapterEntry> adapters;
    if (!EnumerateAdaptersForBackend(query->backend, &adapters)) {
        return DXB_OK;
    }

    uint32_t best_feature_level = 0u;
    for (const AdapterEntry& entry : adapters) {
        uint32_t feature_level = 0u;
        if (ProbeFeatureLevel(query->backend, entry.adapter.Get(), &feature_level, false)) {
            out_info->supported = 1u;
            best_feature_level = std::max(best_feature_level, feature_level);
        }
    }

    out_info->value_u32 = best_feature_level;
    return DXB_OK;
}

DXBResult QueryDebugLayerAvailable(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    if (query->backend == DXB_BACKEND_DX12) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug.ReleaseAndGetAddressOf())))) {
            DXBCapabilityInfo backend_info = {};
            ResetOutput(query, &backend_info);
            QueryBackendAvailable(query, &backend_info);
            out_info->supported = backend_info.supported;
            out_info->value_u32 = backend_info.value_u32;
        }
        return DXB_OK;
    }

    std::vector<AdapterEntry> adapters;
    if (!EnumerateAdaptersForBackend(query->backend, &adapters)) {
        return DXB_OK;
    }

    uint32_t best_feature_level = 0u;
    for (const AdapterEntry& entry : adapters) {
        uint32_t feature_level = 0u;
        if (ProbeDX11FeatureLevel(entry.adapter.Get(), &feature_level, true)) {
            out_info->supported = 1u;
            best_feature_level = std::max(best_feature_level, feature_level);
        }
    }

    out_info->value_u32 = best_feature_level;
    return DXB_OK;
}

DXBResult QueryGpuValidationAvailable(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    if (query->backend != DXB_BACKEND_DX12) {
        return DXB_OK;
    }

    ComPtr<ID3D12Debug1> debug1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug1.ReleaseAndGetAddressOf())))) {
        DXBCapabilityInfo backend_info = {};
        ResetOutput(query, &backend_info);
        QueryBackendAvailable(query, &backend_info);
        out_info->supported = backend_info.supported;
        out_info->value_u32 = backend_info.value_u32;
    }

    return DXB_OK;
}

DXBResult QueryAdapterCount(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    std::vector<AdapterEntry> adapters;
    if (!EnumerateAdaptersForBackend(query->backend, &adapters)) {
        out_info->supported = 1u;
        out_info->value_u32 = 0u;
        return DXB_OK;
    }

    out_info->supported = 1u;
    out_info->value_u32 = static_cast<uint32_t>(adapters.size());
    return DXB_OK;
}

DXBResult QueryAdapterSoftware(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    std::vector<AdapterEntry> adapters;
    if (!EnumerateAdaptersForBackend(query->backend, &adapters)) {
        return DXB_OK;
    }
    if (query->adapter_index >= adapters.size()) {
        return DXB_OK;
    }

    out_info->supported = 1u;
    out_info->value_u32 = adapters[query->adapter_index].is_software ? 1u : 0u;
    return DXB_OK;
}

DXBResult QueryAdapterMaxFeatureLevel(const DXBCapabilityQuery* query, DXBCapabilityInfo* out_info) {
    std::vector<AdapterEntry> adapters;
    if (!EnumerateAdaptersForBackend(query->backend, &adapters)) {
        return DXB_OK;
    }
    if (query->adapter_index >= adapters.size()) {
        return DXB_OK;
    }

    uint32_t feature_level = 0u;
    if (ProbeFeatureLevel(query->backend, adapters[query->adapter_index].adapter.Get(), &feature_level, false)) {
        out_info->supported = 1u;
        out_info->value_u32 = feature_level;
    }
    return DXB_OK;
}

} // namespace

extern "C" DXBRIDGE_API DXBResult DXBridge_QueryCapability(const DXBCapabilityQuery* query,
                                                            DXBCapabilityInfo* out_info) {
    if (!query || !out_info) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "QueryCapability: query and out_info must not be null");
        return DXB_E_INVALID_ARG;
    }

    if (query->struct_version != DXBRIDGE_STRUCT_VERSION) {
        dxb::SetLastError(DXB_E_INVALID_ARG,
                          "QueryCapability: query->struct_version must be %u",
                          (unsigned)DXBRIDGE_STRUCT_VERSION);
        return DXB_E_INVALID_ARG;
    }

    ResetOutput(query, out_info);

    if (query->backend != DXB_BACKEND_DX11 && query->backend != DXB_BACKEND_DX12) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "QueryCapability: backend must be DX11 or DX12");
        return DXB_E_INVALID_ARG;
    }

    switch (query->capability) {
        case DXB_CAPABILITY_BACKEND_AVAILABLE:
            return QueryBackendAvailable(query, out_info);
        case DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE:
            return QueryDebugLayerAvailable(query, out_info);
        case DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE:
            return QueryGpuValidationAvailable(query, out_info);
        case DXB_CAPABILITY_ADAPTER_COUNT:
            return QueryAdapterCount(query, out_info);
        case DXB_CAPABILITY_ADAPTER_SOFTWARE:
            return QueryAdapterSoftware(query, out_info);
        case DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL:
            return QueryAdapterMaxFeatureLevel(query, out_info);
        default:
            dxb::SetLastError(DXB_E_INVALID_ARG,
                              "QueryCapability: unsupported capability id %u",
                              (unsigned)query->capability);
            return DXB_E_INVALID_ARG;
    }
}
