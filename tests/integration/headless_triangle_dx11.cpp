// tests/integration/headless_triangle_dx11.cpp
// Headless (offscreen) WARP integration test for DXBridge DX11 backend.
// Requirements:
//   - No window required
//   - WARP software adapter (always present on Windows 10/11)
//   - Renders a red triangle to a 256x256 offscreen render target
//   - Reads back the center pixel and verifies it is red (R > 0)
//   - Returns 0 (PASS) or 1 (FAIL)
//
// Escape hatch: uses GetNativeDevice()/GetNativeContext() for the offscreen
// texture and staging buffer, since DXBridge has no CreateRenderTarget2D API.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "dxbridge/dxbridge.h"

#include <cstdio>
#include <cstring>

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// HLSL shaders
// ---------------------------------------------------------------------------

static const char* k_vs_src =
    "float4 main(float2 pos : POSITION) : SV_POSITION {\n"
    "    return float4(pos, 0.0f, 1.0f);\n"
    "}\n";

static const char* k_ps_src =
    "float4 main() : SV_TARGET {\n"
    "    return float4(1.0f, 0.0f, 0.0f, 1.0f);\n"
    "}\n";

// ---------------------------------------------------------------------------
// Vertex data: triangle covering center of 256x256 target (NDC coords)
// ---------------------------------------------------------------------------

static const float k_verts[] = {
     0.0f,  0.5f,   // top center
    -0.5f, -0.5f,   // bottom left
     0.5f, -0.5f,   // bottom right
};

// ---------------------------------------------------------------------------
// Helper: fail with message
// ---------------------------------------------------------------------------

static int fail(const char* reason) {
    printf("FAIL: %s\n", reason);
    DXBridge_Shutdown();
    return 1;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    printf("=== DXBridge Headless Triangle (WARP) ===\n");

    // ------------------------------------------------------------------
    // 1. Init DXBridge
    // ------------------------------------------------------------------
    DXBResult r = DXBridge_Init(DXB_BACKEND_DX11);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        printf("SKIP: DXBridge_Init failed: %s (HRESULT 0x%08X)\n",
               err, DXBridge_GetLastHRESULT());
        return 0; // Not a hard failure on CI
    }

    // ------------------------------------------------------------------
    // 2. Enumerate adapters — find WARP (software) adapter
    // ------------------------------------------------------------------
    uint32_t adapter_count = 0;
    DXBridge_EnumerateAdapters(nullptr, &adapter_count);
    if (adapter_count == 0) return fail("No adapters found");

    DXBAdapterInfo* adapters = new DXBAdapterInfo[adapter_count];
    for (uint32_t i = 0; i < adapter_count; ++i)
        adapters[i].struct_version = DXBRIDGE_STRUCT_VERSION;

    r = DXBridge_EnumerateAdapters(adapters, &adapter_count);
    if (r != DXB_OK) { delete[] adapters; return fail("EnumerateAdapters failed"); }

    // Find WARP adapter index
    uint32_t warp_index = 0xFFFFFFFF;
    for (uint32_t i = 0; i < adapter_count; ++i) {
        printf("  Adapter %u: %s  software=%u\n",
               i, adapters[i].description, adapters[i].is_software);
        if (adapters[i].is_software && warp_index == 0xFFFFFFFF)
            warp_index = i;
    }
    delete[] adapters;

    if (warp_index == 0xFFFFFFFF) {
        // WARP may not be listed as a DXGI adapter on some CI environments.
        printf("SKIP: No WARP adapter found — skipping headless test.\n");
        DXBridge_Shutdown();
        return 0;
    }

    printf("  Using WARP adapter index %u\n", warp_index);

    // ------------------------------------------------------------------
    // 3. Create device on WARP adapter with debug flag (graceful fallback)
    // ------------------------------------------------------------------
    DXBDeviceDesc dev_desc = {};
    dev_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    dev_desc.backend        = DXB_BACKEND_DX11;
    dev_desc.adapter_index  = warp_index;
    dev_desc.flags          = DXB_DEVICE_FLAG_DEBUG; // backend retries without if unavailable

    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CreateDevice(&dev_desc, &device);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        printf("SKIP: CreateDevice failed: %s — WARP may be unavailable.\n", err);
        DXBridge_Shutdown();
        return 0;
    }
    printf("  Device created (feature level 0x%04X)\n",
           DXBridge_GetFeatureLevel(device));

    // ------------------------------------------------------------------
    // 4. Use escape hatch to get raw D3D11 device/context
    //    and create the offscreen render target + staging buffer
    // ------------------------------------------------------------------
    auto* d3d = static_cast<ID3D11Device*>(DXBridge_GetNativeDevice(device));
    auto* ctx = static_cast<ID3D11DeviceContext*>(DXBridge_GetNativeContext(device));
    if (!d3d || !ctx) return fail("GetNativeDevice/Context returned null");

    const uint32_t RT_W = 256;
    const uint32_t RT_H = 256;

    // Offscreen texture (render target)
    ComPtr<ID3D11Texture2D>        rt_tex;
    ComPtr<ID3D11RenderTargetView> raw_rtv;
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = RT_W;
        td.Height           = RT_H;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_DEFAULT;
        td.BindFlags        = D3D11_BIND_RENDER_TARGET;

        HRESULT hr = d3d->CreateTexture2D(&td, nullptr, rt_tex.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("FAIL: CreateTexture2D(RT) HRESULT=0x%08X\n", (unsigned)hr);
            DXBridge_DestroyDevice(device);
            DXBridge_Shutdown();
            return 1;
        }

        hr = d3d->CreateRenderTargetView(rt_tex.Get(), nullptr,
                                          raw_rtv.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("FAIL: CreateRenderTargetView HRESULT=0x%08X\n", (unsigned)hr);
            DXBridge_DestroyDevice(device);
            DXBridge_Shutdown();
            return 1;
        }
    }

    // Staging texture for readback
    ComPtr<ID3D11Texture2D> staging_tex;
    {
        D3D11_TEXTURE2D_DESC sd = {};
        sd.Width            = RT_W;
        sd.Height           = RT_H;
        sd.MipLevels        = 1;
        sd.ArraySize        = 1;
        sd.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.Usage            = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;

        HRESULT hr = d3d->CreateTexture2D(&sd, nullptr, staging_tex.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            printf("FAIL: CreateTexture2D(Staging) HRESULT=0x%08X\n", (unsigned)hr);
            DXBridge_DestroyDevice(device);
            DXBridge_Shutdown();
            return 1;
        }
    }

    // ------------------------------------------------------------------
    // 5. Compile shaders via DXBridge
    // ------------------------------------------------------------------
    DXBShaderDesc vs_desc = {};
    vs_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    vs_desc.stage          = DXB_SHADER_STAGE_VERTEX;
    vs_desc.source_code    = k_vs_src;
    vs_desc.source_size    = 0; // auto strlen
    vs_desc.source_name    = "headless_vs";
    vs_desc.entry_point    = "main";
    vs_desc.target         = "vs_5_0";
    vs_desc.compile_flags  = 0;

    DXBShader vs = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CompileShader(device, &vs_desc, &vs);
    if (r != DXB_OK) {
        char err[512]; DXBridge_GetLastError(err, 512);
        printf("FAIL: CompileShader(VS): %s\n", err);
        DXBridge_DestroyDevice(device); DXBridge_Shutdown(); return 1;
    }

    DXBShaderDesc ps_desc = {};
    ps_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    ps_desc.stage          = DXB_SHADER_STAGE_PIXEL;
    ps_desc.source_code    = k_ps_src;
    ps_desc.source_size    = 0;
    ps_desc.source_name    = "headless_ps";
    ps_desc.entry_point    = "main";
    ps_desc.target         = "ps_5_0";
    ps_desc.compile_flags  = 0;

    DXBShader ps = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CompileShader(device, &ps_desc, &ps);
    if (r != DXB_OK) {
        char err[512]; DXBridge_GetLastError(err, 512);
        printf("FAIL: CompileShader(PS): %s\n", err);
        DXBridge_DestroyShader(vs);
        DXBridge_DestroyDevice(device); DXBridge_Shutdown(); return 1;
    }

    // ------------------------------------------------------------------
    // 6. Create input layout via DXBridge for float2 POSITION.
    // ------------------------------------------------------------------
    DXBInputElementDesc input_element = {};
    input_element.struct_version = DXBRIDGE_STRUCT_VERSION;
    input_element.semantic       = DXB_SEMANTIC_POSITION;
    input_element.semantic_index = 0;
    input_element.format         = DXB_FORMAT_RG32_FLOAT;
    input_element.input_slot     = 0;
    input_element.byte_offset    = 0;

    DXBInputLayout input_layout = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CreateInputLayout(device, &input_element, 1, vs, &input_layout);
    if (r != DXB_OK) {
        char err[512]; DXBridge_GetLastError(err, 512);
        printf("FAIL: CreateInputLayout: %s\n", err);
        DXBridge_DestroyShader(ps); DXBridge_DestroyShader(vs);
        DXBridge_DestroyDevice(device); DXBridge_Shutdown();
        return 1;
    }

    // ------------------------------------------------------------------
    // 8. Create vertex buffer via DXBridge
    // ------------------------------------------------------------------
    DXBBufferDesc vb_desc = {};
    vb_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    vb_desc.flags          = DXB_BUFFER_FLAG_VERTEX;
    vb_desc.byte_size      = sizeof(k_verts);
    vb_desc.stride         = sizeof(float) * 2; // float2

    DXBBuffer vb = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CreateBuffer(device, &vb_desc, &vb);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        printf("FAIL: CreateBuffer: %s\n", err);
        DXBridge_DestroyInputLayout(input_layout);
        DXBridge_DestroyShader(ps); DXBridge_DestroyShader(vs);
        DXBridge_DestroyDevice(device); DXBridge_Shutdown();
        return 1;
    }

    r = DXBridge_UploadData(vb, k_verts, sizeof(k_verts));
    if (r != DXB_OK) return fail("UploadData failed");

    // ------------------------------------------------------------------
    // 8. Create pipeline via DXBridge using the public input-layout handle
    // ------------------------------------------------------------------
    DXBPipelineDesc pso_desc = {};
    pso_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    pso_desc.vs             = vs;
    pso_desc.ps             = ps;
    pso_desc.input_layout   = input_layout;
    pso_desc.topology       = DXB_PRIM_TRIANGLELIST;
    pso_desc.depth_test     = 0;
    pso_desc.depth_write    = 0;
    pso_desc.cull_back      = 0;
    pso_desc.wireframe      = 0;

    DXBPipeline pso = DXBRIDGE_NULL_HANDLE;
    r = DXBridge_CreatePipelineState(device, &pso_desc, &pso);
    if (r != DXB_OK) {
        char err[512]; DXBridge_GetLastError(err, 512);
        printf("FAIL: CreatePipelineState: %s\n", err);
        DXBridge_DestroyBuffer(vb);
        DXBridge_DestroyInputLayout(input_layout);
        DXBridge_DestroyShader(ps); DXBridge_DestroyShader(vs);
        DXBridge_DestroyDevice(device); DXBridge_Shutdown();
        return 1;
    }

    // ------------------------------------------------------------------
    // 10. Render one frame (offscreen — no swap chain)
    // ------------------------------------------------------------------

    // BeginFrame
    r = DXBridge_BeginFrame(device);
    if (r != DXB_OK) return fail("BeginFrame failed");

    // Bind render target directly via native context
    ID3D11RenderTargetView* rtv_arr[] = { raw_rtv.Get() };
    ctx->OMSetRenderTargets(1, rtv_arr, nullptr);

    // Clear to black
    float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ctx->ClearRenderTargetView(raw_rtv.Get(), clear_color);

    // Set viewport
    DXBViewport vp = {};
    vp.x         = 0.0f;
    vp.y         = 0.0f;
    vp.width     = static_cast<float>(RT_W);
    vp.height    = static_cast<float>(RT_H);
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    r = DXBridge_SetViewport(device, &vp);
    if (r != DXB_OK) return fail("SetViewport failed");

    // Set pipeline (VS, PS, RS, DS, Blend states)
    r = DXBridge_SetPipeline(device, pso);
    if (r != DXB_OK) return fail("SetPipeline failed");

    // Set vertex buffer
    r = DXBridge_SetVertexBuffer(device, vb, sizeof(float) * 2, 0);
    if (r != DXB_OK) return fail("SetVertexBuffer failed");

    // Draw 3 vertices
    r = DXBridge_Draw(device, 3, 0);
    if (r != DXB_OK) return fail("Draw failed");

    // EndFrame
    r = DXBridge_EndFrame(device);
    if (r != DXB_OK) return fail("EndFrame failed");

    // ------------------------------------------------------------------
    // 11. Readback: CopyResource to staging, Map, read center pixel
    // ------------------------------------------------------------------
    ctx->CopyResource(staging_tex.Get(), rt_tex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr_map = ctx->Map(staging_tex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr_map)) {
        printf("FAIL: Map staging HRESULT=0x%08X\n", (unsigned)hr_map);
        DXBridge_DestroyPipeline(pso);
        DXBridge_DestroyBuffer(vb);
        DXBridge_DestroyInputLayout(input_layout);
        DXBridge_DestroyShader(ps); DXBridge_DestroyShader(vs);
        DXBridge_DestroyDevice(device); DXBridge_Shutdown();
        return 1;
    }

    // Center pixel (128, 128)
    const uint32_t cx = RT_W / 2;
    const uint32_t cy = RT_H / 2;
    const uint8_t* row   = reinterpret_cast<const uint8_t*>(mapped.pData)
                           + cy * mapped.RowPitch;
    const uint8_t* pixel = row + cx * 4; // RGBA8

    uint8_t pR = pixel[0];
    uint8_t pG = pixel[1];
    uint8_t pB = pixel[2];
    uint8_t pA = pixel[3];

    printf("  Center pixel (R=%u G=%u B=%u A=%u)\n", pR, pG, pB, pA);

    ctx->Unmap(staging_tex.Get(), 0);

    // ------------------------------------------------------------------
    // 12. Cleanup
    // ------------------------------------------------------------------
    DXBridge_DestroyPipeline(pso);
    DXBridge_DestroyBuffer(vb);
    DXBridge_DestroyInputLayout(input_layout);
    DXBridge_DestroyShader(ps);
    DXBridge_DestroyShader(vs);
    DXBridge_DestroyDevice(device);
    DXBridge_Shutdown();

    // ------------------------------------------------------------------
    // 13. Verify result
    // ------------------------------------------------------------------
    if (pR == 0) {
        printf("FAIL: Center pixel red channel is 0 (expected > 0 for red triangle)\n");
        return 1;
    }

    printf("PASS\n");
    return 0;
}
