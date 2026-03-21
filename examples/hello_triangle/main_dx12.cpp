// examples/hello_triangle/main_dx12.cpp
// DXBridge Hello Triangle — Win32 windowed example (DX12 backend).
// Renders a colored triangle (red top, green left, blue right) in a 640x480 window.
// Press ESC to quit.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>       // Needed only for wrl/client.h COM helpers (indirect)
#include <d3dcompiler.h>

#include "dxbridge/dxbridge.h"

#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// HLSL shaders — vertex color
// ---------------------------------------------------------------------------

static const char* k_vs_src =
    "struct VSInput { float2 pos : POSITION; float3 col : COLOR; };\n"
    "struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };\n"
    "PSInput main(VSInput v) {\n"
    "    PSInput o;\n"
    "    o.pos = float4(v.pos, 0.0f, 1.0f);\n"
    "    o.col = v.col;\n"
    "    return o;\n"
    "}\n";

static const char* k_ps_src =
    "struct PSInput { float4 pos : SV_POSITION; float3 col : COLOR; };\n"
    "float4 main(PSInput p) : SV_TARGET {\n"
    "    return float4(p.col, 1.0f);\n"
    "}\n";

// ---------------------------------------------------------------------------
// Vertex data: pos(float2) + color(float3)
// ---------------------------------------------------------------------------

struct Vertex {
    float x, y;      // position
    float r, g, b;   // color
};

static const Vertex k_verts[] = {
    {  0.0f,  0.5f,  1.0f, 0.0f, 0.0f },  // top    — red
    { -0.5f, -0.5f,  0.0f, 1.0f, 0.0f },  // left   — green
    {  0.5f, -0.5f,  0.0f, 0.0f, 1.0f },  // right  — blue
};

// ---------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------

struct AppState {
    DXBDevice       device       = DXBRIDGE_NULL_HANDLE;
    DXBSwapChain    swapchain    = DXBRIDGE_NULL_HANDLE;
    DXBRenderTarget backbuffer   = DXBRIDGE_NULL_HANDLE;
    DXBRTV          rtv          = DXBRIDGE_NULL_HANDLE;
    DXBDSV          dsv          = DXBRIDGE_NULL_HANDLE;
    DXBShader       vs           = DXBRIDGE_NULL_HANDLE;
    DXBShader       ps           = DXBRIDGE_NULL_HANDLE;
    DXBInputLayout  input_layout = DXBRIDGE_NULL_HANDLE;
    DXBPipeline     pso          = DXBRIDGE_NULL_HANDLE;
    DXBBuffer       vb           = DXBRIDGE_NULL_HANDLE;

    uint32_t width   = 640;
    uint32_t height  = 480;
    bool     running = true;
};

static AppState g_app;

// ---------------------------------------------------------------------------
// Helpers for RTV/DSV + back buffer recreation on resize
// ---------------------------------------------------------------------------

static void DestroyRTVDSV() {
    if (g_app.dsv != DXBRIDGE_NULL_HANDLE) {
        DXBridge_DestroyDSV(g_app.dsv);
        g_app.dsv = DXBRIDGE_NULL_HANDLE;
    }
    if (g_app.rtv != DXBRIDGE_NULL_HANDLE) {
        DXBridge_DestroyRTV(g_app.rtv);
        g_app.rtv = DXBRIDGE_NULL_HANDLE;
    }
    // backbuffer is a thin wrapper owned by the swap chain — just null it out
    g_app.backbuffer = DXBRIDGE_NULL_HANDLE;
}

static bool CreateRTVDSV() {
    DXBResult r;

    r = DXBridge_GetBackBuffer(g_app.swapchain, &g_app.backbuffer);
    if (r != DXB_OK) { OutputDebugStringA("GetBackBuffer failed\n"); return false; }

    r = DXBridge_CreateRenderTargetView(g_app.device, g_app.backbuffer, &g_app.rtv);
    if (r != DXB_OK) { OutputDebugStringA("CreateRenderTargetView failed\n"); return false; }

    DXBDepthStencilDesc dsv_desc = {};
    dsv_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    dsv_desc.format         = DXB_FORMAT_D32_FLOAT;
    dsv_desc.width          = g_app.width;
    dsv_desc.height         = g_app.height;

    r = DXBridge_CreateDepthStencilView(g_app.device, &dsv_desc, &g_app.dsv);
    if (r != DXB_OK) { OutputDebugStringA("CreateDepthStencilView failed\n"); return false; }

    return true;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        uint32_t new_w = LOWORD(lp);
        uint32_t new_h = HIWORD(lp);
        if (new_w == 0 || new_h == 0) break; // minimized
        if (new_w == g_app.width && new_h == g_app.height) break;

        g_app.width  = new_w;
        g_app.height = new_h;

        DestroyRTVDSV();
        DXBridge_ResizeSwapChain(g_app.swapchain, new_w, new_h);
        CreateRTVDSV();
        break;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        g_app.running = false;
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// WinMain entry point
// ---------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // ------------------------------------------------------------------
    // 1. Create Win32 window
    // ------------------------------------------------------------------
    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = "DXBridgeHelloTriangleDX12";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(nullptr, "RegisterClassEx failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT rect = { 0, 0, static_cast<LONG>(g_app.width),
                        static_cast<LONG>(g_app.height) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0, "DXBridgeHelloTriangleDX12",
        "DXBridge \xe2\x80\x94 Hello Triangle (DX12)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        MessageBoxA(nullptr, "CreateWindowEx failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ------------------------------------------------------------------
    // 2. Init DXBridge for DX12
    // ------------------------------------------------------------------
    DXBResult r = DXBridge_Init(DXB_BACKEND_DX12);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        MessageBoxA(hwnd, err, "DXBridge_Init failed", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ------------------------------------------------------------------
    // 3. Create device (first adapter, high-perf GPU)
    // ------------------------------------------------------------------
    DXBDeviceDesc dev_desc = {};
    dev_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    dev_desc.backend        = DXB_BACKEND_DX12;
    dev_desc.adapter_index  = 0;
    dev_desc.flags          = DXB_DEVICE_FLAG_DEBUG;

    r = DXBridge_CreateDevice(&dev_desc, &g_app.device);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        MessageBoxA(hwnd, err, "DXBridge_CreateDevice failed", MB_OK | MB_ICONERROR);
        DXBridge_Shutdown(); return 1;
    }

    // ------------------------------------------------------------------
    // 4. Create swap chain
    // ------------------------------------------------------------------
    DXBSwapChainDesc sc_desc = {};
    sc_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    sc_desc.hwnd           = hwnd;
    sc_desc.width          = g_app.width;
    sc_desc.height         = g_app.height;
    sc_desc.format         = DXB_FORMAT_RGBA8_UNORM;
    sc_desc.buffer_count   = 2;
    sc_desc.flags          = 0;

    r = DXBridge_CreateSwapChain(g_app.device, &sc_desc, &g_app.swapchain);
    if (r != DXB_OK) {
        char err[256]; DXBridge_GetLastError(err, 256);
        MessageBoxA(hwnd, err, "DXBridge_CreateSwapChain failed", MB_OK | MB_ICONERROR);
        DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
    }

    // ------------------------------------------------------------------
    // 5. Get back buffer + create RTV/DSV
    // ------------------------------------------------------------------
    if (!CreateRTVDSV()) {
        MessageBoxA(hwnd, "CreateRTVDSV failed", "Error", MB_OK | MB_ICONERROR);
        DXBridge_DestroySwapChain(g_app.swapchain);
        DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
    }

    // ------------------------------------------------------------------
    // 6. Compile shaders
    // ------------------------------------------------------------------
    {
        DXBShaderDesc vs_desc = {};
        vs_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        vs_desc.stage          = DXB_SHADER_STAGE_VERTEX;
        vs_desc.source_code    = k_vs_src;
        vs_desc.source_size    = 0;
        vs_desc.source_name    = "hello_triangle_dx12_vs";
        vs_desc.entry_point    = "main";
        vs_desc.target         = "vs_5_0";
        vs_desc.compile_flags  = 1; // debug

        r = DXBridge_CompileShader(g_app.device, &vs_desc, &g_app.vs);
        if (r != DXB_OK) {
            char err[512]; DXBridge_GetLastError(err, 512);
            MessageBoxA(hwnd, err, "VS compile failed", MB_OK | MB_ICONERROR);
            DestroyRTVDSV();
            DXBridge_DestroySwapChain(g_app.swapchain);
            DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
        }
    }

    {
        DXBShaderDesc ps_desc = {};
        ps_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        ps_desc.stage          = DXB_SHADER_STAGE_PIXEL;
        ps_desc.source_code    = k_ps_src;
        ps_desc.source_size    = 0;
        ps_desc.source_name    = "hello_triangle_dx12_ps";
        ps_desc.entry_point    = "main";
        ps_desc.target         = "ps_5_0";
        ps_desc.compile_flags  = 1;

        r = DXBridge_CompileShader(g_app.device, &ps_desc, &g_app.ps);
        if (r != DXB_OK) {
            char err[512]; DXBridge_GetLastError(err, 512);
            MessageBoxA(hwnd, err, "PS compile failed", MB_OK | MB_ICONERROR);
            DXBridge_DestroyShader(g_app.vs);
            DestroyRTVDSV();
            DXBridge_DestroySwapChain(g_app.swapchain);
            DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
        }
    }

    // ------------------------------------------------------------------
    // 7. Create input layout via DXBridge
    //    DX12 path uses DXBInputElementDesc (no native escape hatch needed).
    //    DXB_FORMAT_RG32_FLOAT  = float2 POSITION
    //    DXB_FORMAT_RGB32_FLOAT = float3 COLOR
    // ------------------------------------------------------------------
    {
        DXBInputElementDesc elems[2] = {};

        elems[0].struct_version = DXBRIDGE_STRUCT_VERSION;
        elems[0].semantic       = DXB_SEMANTIC_POSITION;
        elems[0].semantic_index = 0;
        elems[0].format         = DXB_FORMAT_RG32_FLOAT;
        elems[0].input_slot     = 0;
        elems[0].byte_offset    = 0;  // POSITION at offset 0

        elems[1].struct_version = DXBRIDGE_STRUCT_VERSION;
        elems[1].semantic       = DXB_SEMANTIC_COLOR;
        elems[1].semantic_index = 0;
        elems[1].format         = DXB_FORMAT_RGB32_FLOAT;
        elems[1].input_slot     = 0;
        elems[1].byte_offset    = 8; // COLOR at offset 8 (after 2 floats)

        r = DXBridge_CreateInputLayout(g_app.device, elems, 2, g_app.vs, &g_app.input_layout);
        if (r != DXB_OK) {
            char err[512]; DXBridge_GetLastError(err, 512);
            MessageBoxA(hwnd, err, "CreateInputLayout failed", MB_OK | MB_ICONERROR);
            DXBridge_DestroyShader(g_app.ps); DXBridge_DestroyShader(g_app.vs);
            DestroyRTVDSV();
            DXBridge_DestroySwapChain(g_app.swapchain);
            DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
        }
    }

    // ------------------------------------------------------------------
    // 8. Create pipeline state
    // ------------------------------------------------------------------
    {
        DXBPipelineDesc pso_desc = {};
        pso_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        pso_desc.vs             = g_app.vs;
        pso_desc.ps             = g_app.ps;
        pso_desc.input_layout   = g_app.input_layout;
        pso_desc.topology       = DXB_PRIM_TRIANGLELIST;
        pso_desc.depth_test     = 1;
        pso_desc.depth_write    = 1;
        pso_desc.cull_back      = 0;
        pso_desc.wireframe      = 0;

        r = DXBridge_CreatePipelineState(g_app.device, &pso_desc, &g_app.pso);
        if (r != DXB_OK) {
            char err[512]; DXBridge_GetLastError(err, 512);
            MessageBoxA(hwnd, err, "CreatePipelineState failed", MB_OK | MB_ICONERROR);
            DXBridge_DestroyInputLayout(g_app.input_layout);
            DXBridge_DestroyShader(g_app.ps); DXBridge_DestroyShader(g_app.vs);
            DestroyRTVDSV();
            DXBridge_DestroySwapChain(g_app.swapchain);
            DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
        }
    }

    // ------------------------------------------------------------------
    // 9. Create vertex buffer
    // ------------------------------------------------------------------
    {
        DXBBufferDesc vb_desc = {};
        vb_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        vb_desc.flags          = DXB_BUFFER_FLAG_VERTEX;
        vb_desc.byte_size      = sizeof(k_verts);
        vb_desc.stride         = sizeof(Vertex);

        r = DXBridge_CreateBuffer(g_app.device, &vb_desc, &g_app.vb);
        if (r != DXB_OK) {
            char err[256]; DXBridge_GetLastError(err, 256);
            MessageBoxA(hwnd, err, "CreateBuffer failed", MB_OK | MB_ICONERROR);
            DXBridge_DestroyPipeline(g_app.pso);
            DXBridge_DestroyInputLayout(g_app.input_layout);
            DXBridge_DestroyShader(g_app.ps); DXBridge_DestroyShader(g_app.vs);
            DestroyRTVDSV();
            DXBridge_DestroySwapChain(g_app.swapchain);
            DXBridge_DestroyDevice(g_app.device); DXBridge_Shutdown(); return 1;
        }
        DXBridge_UploadData(g_app.vb, k_verts, sizeof(k_verts));
    }

    // ------------------------------------------------------------------
    // 10. Message + render loop
    // ------------------------------------------------------------------
    MSG msg = {};
    while (g_app.running) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) { g_app.running = false; break; }
        }
        if (!g_app.running) break;

        DXBridge_BeginFrame(g_app.device);

        DXBridge_SetRenderTargets(g_app.device, g_app.rtv, g_app.dsv);

        float clear_col[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        DXBridge_ClearRenderTarget(g_app.rtv, clear_col);
        DXBridge_ClearDepthStencil(g_app.dsv, 1.0f, 0);

        DXBViewport vp = {};
        vp.x         = 0.0f;
        vp.y         = 0.0f;
        vp.width     = static_cast<float>(g_app.width);
        vp.height    = static_cast<float>(g_app.height);
        vp.min_depth = 0.0f;
        vp.max_depth = 1.0f;
        DXBridge_SetViewport(g_app.device, &vp);

        DXBridge_SetPipeline(g_app.device, g_app.pso);
        DXBridge_SetVertexBuffer(g_app.device, g_app.vb, sizeof(Vertex), 0);
        DXBridge_Draw(g_app.device, 3, 0);

        DXBridge_EndFrame(g_app.device);
        DXBridge_Present(g_app.swapchain, 1);

        // After Present, frame_index updates to the new back buffer.
        // Recreate RTV to reflect the new current back buffer index.
        // (DestroyRTV + GetBackBuffer + CreateRenderTargetView)
        DXBridge_DestroyRTV(g_app.rtv);
        g_app.rtv = DXBRIDGE_NULL_HANDLE;
        g_app.backbuffer = DXBRIDGE_NULL_HANDLE;
        DXBridge_GetBackBuffer(g_app.swapchain, &g_app.backbuffer);
        DXBridge_CreateRenderTargetView(g_app.device, g_app.backbuffer, &g_app.rtv);
    }

    // ------------------------------------------------------------------
    // 11. Cleanup
    // ------------------------------------------------------------------
    DXBridge_DestroyBuffer(g_app.vb);
    DXBridge_DestroyPipeline(g_app.pso);
    DXBridge_DestroyInputLayout(g_app.input_layout);
    DXBridge_DestroyShader(g_app.ps);
    DXBridge_DestroyShader(g_app.vs);
    DestroyRTVDSV();
    DXBridge_DestroySwapChain(g_app.swapchain);
    DXBridge_DestroyDevice(g_app.device);
    DXBridge_Shutdown();

    return 0;
}
