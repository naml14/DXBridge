// src/dxbridge.cpp
// DXBridge — extern "C" dispatch layer.
// Routes all public API calls to the active IBackend.
// Note: DXBRIDGE_EXPORTS is defined via CMake target_compile_definitions
#include "../include/dxbridge/dxbridge.h"
#include "common/backend.hpp"
#include "common/error.hpp"
#include "common/log.hpp"
#include "dx11/device_dx11.hpp"
#include "dx12/device_dx12.hpp"

// ---------------------------------------------------------------------------
// Backend singleton — defined here, declared extern in backend.hpp
// ---------------------------------------------------------------------------
namespace dxb {
    IBackend* g_backend = nullptr;
}

// ---------------------------------------------------------------------------
// Guard macro — returns DXB_E_NOT_INITIALIZED if backend not set
// ---------------------------------------------------------------------------
#define GUARD_INIT(ret_type)                    \
    if (!dxb::g_backend) {                      \
        dxb::SetLastError(DXB_E_NOT_INITIALIZED, \
            "DXBridge_Init has not been called"); \
        return (ret_type)DXB_E_NOT_INITIALIZED; \
    }

// ---------------------------------------------------------------------------
// Exported C API
// ---------------------------------------------------------------------------
extern "C" {

DXBRIDGE_API uint32_t DXBridge_GetVersion(void) {
    return DXBRIDGE_VERSION;
}

DXBRIDGE_API DXBResult DXBridge_Init(DXBBackend hint) {
    if (dxb::g_backend) {
        // Idempotent: already initialized
        return DXB_OK;
    }

    // Try DX12 first if AUTO or DX12 explicitly requested
    if (hint == DXB_BACKEND_DX12 || hint == DXB_BACKEND_AUTO) {
        auto* b = new dxb::DX12Backend();
        if (b->Init() == DXB_OK) {
            dxb::g_backend = b;
            DXB_LOG_INFO("DXBridge initialized with DX12 backend");
            return DXB_OK;
        }
        delete b;
        if (hint == DXB_BACKEND_DX12) {
            dxb::SetLastError(DXB_E_NOT_SUPPORTED, "DX12 backend not available");
            return DXB_E_NOT_SUPPORTED;
        }
        // AUTO falls through to DX11
    }

    // DX11 path
    auto* b = new dxb::DX11Backend();
    DXBResult r = b->Init();
    if (r != DXB_OK) {
        delete b;
        dxb::SetLastError(r, "DX11 backend initialization failed");
        return r;
    }
    dxb::g_backend = b;
    DXB_LOG_INFO("DXBridge initialized with DX11 backend");
    return DXB_OK;
}

DXBRIDGE_API void DXBridge_Shutdown(void) {
    if (dxb::g_backend) {
        dxb::g_backend->Shutdown();
        delete dxb::g_backend;
        dxb::g_backend = nullptr;
        DXB_LOG_INFO("DXBridge shutdown complete");
    }
}

DXBRIDGE_API uint32_t DXBridge_SupportsFeature(uint32_t feature_flag) {
    if (!dxb::g_backend) return 0;
    return dxb::g_backend->SupportsFeature(feature_flag);
}

// Error info — thread-local, always available (no Init required)
DXBRIDGE_API void DXBridge_GetLastError(char* buf, int buf_size) {
    dxb::GetLastErrorInternal(buf, buf_size);
}

DXBRIDGE_API uint32_t DXBridge_GetLastHRESULT(void) {
    return dxb::GetLastHRESULTInternal();
}

DXBRIDGE_API void DXBridge_SetLogCallback(DXBLogCallback cb, void* user_data) {
    dxb::SetLogCallbackInternal(cb, user_data);
}

DXBRIDGE_API DXBResult DXBridge_EnumerateAdapters(DXBAdapterInfo* out_buf,
                                                    uint32_t* in_out_count) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->EnumerateAdapters(out_buf, in_out_count);
}

DXBRIDGE_API DXBResult DXBridge_CreateDevice(const DXBDeviceDesc* desc,
                                               DXBDevice* out_device) {
    GUARD_INIT(DXBResult);
    if (!desc || !out_device) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "desc and out_device must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreateDevice(desc, out_device);
}

DXBRIDGE_API void DXBridge_DestroyDevice(DXBDevice device) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyDevice(device);
}

DXBRIDGE_API uint32_t DXBridge_GetFeatureLevel(DXBDevice device) {
    if (!dxb::g_backend) return 0;
    return dxb::g_backend->GetFeatureLevel(device);
}

DXBRIDGE_API DXBResult DXBridge_CreateSwapChain(DXBDevice device,
                                                  const DXBSwapChainDesc* desc,
                                                  DXBSwapChain* out_sc) {
    GUARD_INIT(DXBResult);
    if (!desc || !out_sc) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "desc and out_sc must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreateSwapChain(device, desc, out_sc);
}

DXBRIDGE_API void DXBridge_DestroySwapChain(DXBSwapChain sc) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroySwapChain(sc);
}

DXBRIDGE_API DXBResult DXBridge_ResizeSwapChain(DXBSwapChain sc,
                                                  uint32_t width, uint32_t height) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->ResizeSwapChain(sc, width, height);
}

DXBRIDGE_API DXBResult DXBridge_GetBackBuffer(DXBSwapChain sc,
                                               DXBRenderTarget* out_rt) {
    GUARD_INIT(DXBResult);
    if (!out_rt) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "out_rt must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->GetBackBuffer(sc, out_rt);
}

DXBRIDGE_API DXBResult DXBridge_CreateRenderTargetView(DXBDevice device,
                                                         DXBRenderTarget rt,
                                                         DXBRTV* out_rtv) {
    GUARD_INIT(DXBResult);
    if (!out_rtv) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "out_rtv must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreateRenderTargetView(device, rt, out_rtv);
}

DXBRIDGE_API void DXBridge_DestroyRTV(DXBRTV rtv) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyRTV(rtv);
}

DXBRIDGE_API DXBResult DXBridge_CreateDepthStencilView(DXBDevice device,
                                                         const DXBDepthStencilDesc* desc,
                                                         DXBDSV* out_dsv) {
    GUARD_INIT(DXBResult);
    if (!desc || !out_dsv) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "desc and out_dsv must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreateDepthStencilView(device, desc, out_dsv);
}

DXBRIDGE_API void DXBridge_DestroyDSV(DXBDSV dsv) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyDSV(dsv);
}

DXBRIDGE_API DXBResult DXBridge_CompileShader(DXBDevice device,
                                               const DXBShaderDesc* desc,
                                               DXBShader* out_shader) {
    GUARD_INIT(DXBResult);
    if (!desc || !out_shader) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "desc and out_shader must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CompileShader(device, desc, out_shader);
}

DXBRIDGE_API void DXBridge_DestroyShader(DXBShader shader) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyShader(shader);
}

DXBRIDGE_API DXBResult DXBridge_CreateInputLayout(DXBDevice device,
                                                    const DXBInputElementDesc* elements,
                                                    uint32_t element_count,
                                                    DXBShader vs,
                                                    DXBInputLayout* out_layout) {
    GUARD_INIT(DXBResult);
    if (!out_layout) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "out_layout must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreateInputLayout(device, elements, element_count, vs, out_layout);
}

DXBRIDGE_API void DXBridge_DestroyInputLayout(DXBInputLayout layout) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyInputLayout(layout);
}

DXBRIDGE_API DXBResult DXBridge_CreatePipelineState(DXBDevice device,
                                                      const DXBPipelineDesc* desc,
                                                      DXBPipeline* out_pipeline) {
    GUARD_INIT(DXBResult);
    if (!desc || !out_pipeline) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "desc and out_pipeline must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreatePipelineState(device, desc, out_pipeline);
}

DXBRIDGE_API void DXBridge_DestroyPipeline(DXBPipeline pipeline) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyPipeline(pipeline);
}

DXBRIDGE_API DXBResult DXBridge_CreateBuffer(DXBDevice device,
                                               const DXBBufferDesc* desc,
                                               DXBBuffer* out_buf) {
    GUARD_INIT(DXBResult);
    if (!desc || !out_buf) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "desc and out_buf must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->CreateBuffer(device, desc, out_buf);
}

DXBRIDGE_API DXBResult DXBridge_UploadData(DXBBuffer buf,
                                             const void* data, uint32_t byte_size) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->UploadData(buf, data, byte_size);
}

DXBRIDGE_API void DXBridge_DestroyBuffer(DXBBuffer buf) {
    if (!dxb::g_backend) return;
    dxb::g_backend->DestroyBuffer(buf);
}

DXBRIDGE_API DXBResult DXBridge_BeginFrame(DXBDevice device) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->BeginFrame(device);
}

DXBRIDGE_API DXBResult DXBridge_SetRenderTargets(DXBDevice device,
                                                   DXBRTV rtv, DXBDSV dsv) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->SetRenderTargets(device, rtv, dsv);
}

DXBRIDGE_API DXBResult DXBridge_ClearRenderTarget(DXBRTV rtv,
                                                    const float rgba[4]) {
    GUARD_INIT(DXBResult);
    if (!rgba) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "rgba must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->ClearRenderTarget(rtv, rgba);
}

DXBRIDGE_API DXBResult DXBridge_ClearDepthStencil(DXBDSV dsv,
                                                    float depth, uint8_t stencil) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->ClearDepthStencil(dsv, depth, stencil);
}

DXBRIDGE_API DXBResult DXBridge_SetViewport(DXBDevice device,
                                              const DXBViewport* vp) {
    GUARD_INIT(DXBResult);
    if (!vp) {
        dxb::SetLastError(DXB_E_INVALID_ARG, "vp must not be null");
        return DXB_E_INVALID_ARG;
    }
    return dxb::g_backend->SetViewport(device, vp);
}

DXBRIDGE_API DXBResult DXBridge_SetPipeline(DXBDevice device,
                                              DXBPipeline pipeline) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->SetPipeline(device, pipeline);
}

DXBRIDGE_API DXBResult DXBridge_SetVertexBuffer(DXBDevice device,
                                                  DXBBuffer buf,
                                                  uint32_t stride,
                                                  uint32_t offset) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->SetVertexBuffer(device, buf, stride, offset);
}

DXBRIDGE_API DXBResult DXBridge_SetIndexBuffer(DXBDevice device,
                                                 DXBBuffer buf,
                                                 DXBFormat fmt,
                                                 uint32_t offset) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->SetIndexBuffer(device, buf, fmt, offset);
}

DXBRIDGE_API DXBResult DXBridge_SetConstantBuffer(DXBDevice device,
                                                    uint32_t slot,
                                                    DXBBuffer buf) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->SetConstantBuffer(device, slot, buf);
}

DXBRIDGE_API DXBResult DXBridge_Draw(DXBDevice device,
                                       uint32_t vertex_count,
                                       uint32_t start_vertex) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->Draw(device, vertex_count, start_vertex);
}

DXBRIDGE_API DXBResult DXBridge_DrawIndexed(DXBDevice device,
                                              uint32_t index_count,
                                              uint32_t start_index,
                                              int32_t  base_vertex) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->DrawIndexed(device, index_count, start_index, base_vertex);
}

DXBRIDGE_API DXBResult DXBridge_EndFrame(DXBDevice device) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->EndFrame(device);
}

DXBRIDGE_API DXBResult DXBridge_Present(DXBSwapChain sc,
                                          uint32_t sync_interval) {
    GUARD_INIT(DXBResult);
    return dxb::g_backend->Present(sc, sync_interval);
}

DXBRIDGE_API void* DXBridge_GetNativeDevice(DXBDevice device) {
    if (!dxb::g_backend) return nullptr;
    return dxb::g_backend->GetNativeDevice(device);
}

DXBRIDGE_API void* DXBridge_GetNativeContext(DXBDevice device) {
    if (!dxb::g_backend) return nullptr;
    return dxb::g_backend->GetNativeContext(device);
}

} // extern "C"
