#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "dx12/device_dx12.hpp"

#include "common/error.hpp"
#include "common/log.hpp"

#include <windows.h>

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kWindowClassName = "DXBridgeDx12ValidationWindow";

static LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static bool EnsureWindowClassRegistered() {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = TestWndProc;
    wc.hInstance     = GetModuleHandleA(nullptr);
    wc.lpszClassName = kWindowClassName;
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    registered = true;
    return true;
}

static HWND CreateHiddenWindow() {
    if (!EnsureWindowClassRegistered()) {
        return nullptr;
    }

    return CreateWindowExA(
        0,
        kWindowClassName,
        "dxbridge-dx12-validation",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        64,
        64,
        nullptr,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr);
}

static std::string LastErrorText() {
    char buf[512] = {};
    dxb::GetLastErrorInternal(buf, static_cast<int>(sizeof(buf)));
    return std::string(buf);
}

static bool ExpectResult(const char* label, DXBResult actual, DXBResult expected) {
    if (actual == expected) {
        return true;
    }

    std::printf("FAIL: %s returned %d (expected %d): %s\n",
                label,
                static_cast<int>(actual),
                static_cast<int>(expected),
                LastErrorText().c_str());
    return false;
}

static bool ExpectBool(const char* label, bool actual, bool expected) {
    if (actual == expected) {
        return true;
    }

    std::printf("FAIL: %s returned %s (expected %s)\n",
                label,
                actual ? "true" : "false",
                expected ? "true" : "false");
    return false;
}

static bool ExpectUInt32(const char* label, uint32_t actual, uint32_t expected) {
    if (actual == expected) {
        return true;
    }

    std::printf("FAIL: %s returned %u (expected %u)\n", label, actual, expected);
    return false;
}

struct CapturedLog {
    int level;
    std::string message;
};

static std::vector<CapturedLog>* g_log_sink = nullptr;

static void CaptureLogCallback(int level, const char* message, void* user_data) {
    auto* sink = static_cast<std::vector<CapturedLog>*>(user_data);
    if (!sink) {
        sink = g_log_sink;
    }
    if (!sink) {
        return;
    }
    sink->push_back(CapturedLog{level, message ? message : ""});
}

class ScopedLogCapture {
public:
    ScopedLogCapture() {
        g_log_sink = &entries_;
        dxb::SetLogCallbackInternal(CaptureLogCallback, &entries_);
    }

    ~ScopedLogCapture() {
        dxb::SetLogCallbackInternal(nullptr, nullptr);
        g_log_sink = nullptr;
    }

    bool Contains(int level, std::string_view needle) const {
        for (const auto& entry : entries_) {
            if (entry.level == level && entry.message.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<CapturedLog> entries_;
};

static bool ExpectLastErrorContains(const char* label, const char* needle) {
    const std::string text = LastErrorText();
    if (text.find(needle) != std::string::npos) {
        return true;
    }

    std::printf("FAIL: %s missing '%s': %s\n", label, needle, text.c_str());
    return false;
}

static bool ExpectLogContains(const char* label,
                              const ScopedLogCapture& logs,
                              int level,
                              std::string_view needle) {
    if (logs.Contains(level, needle)) {
        return true;
    }

    std::printf("FAIL: %s missing log level %d containing '%.*s'\n",
                label,
                level,
                static_cast<int>(needle.size()),
                needle.data());
    return false;
}

static bool CreateWarpDevice(dxb::DX12Backend& backend, bool request_debug, DXBDevice* out_device) {
    *out_device = DXBRIDGE_NULL_HANDLE;

    DXBResult r = backend.Init();
    if (r != DXB_OK) {
        std::printf("SKIP: DX12Backend::Init failed: %s\n", LastErrorText().c_str());
        return false;
    }

    uint32_t adapter_count = 0;
    r = backend.EnumerateAdapters(nullptr, &adapter_count);
    if (r != DXB_OK || adapter_count == 0) {
        std::printf("SKIP: EnumerateAdapters failed: %s\n", LastErrorText().c_str());
        return false;
    }

    std::vector<DXBAdapterInfo> adapters(adapter_count);
    for (auto& adapter : adapters) {
        adapter.struct_version = DXBRIDGE_STRUCT_VERSION;
    }

    r = backend.EnumerateAdapters(adapters.data(), &adapter_count);
    if (r != DXB_OK) {
        std::printf("SKIP: EnumerateAdapters(fill) failed: %s\n", LastErrorText().c_str());
        return false;
    }

    uint32_t warp_index = UINT32_MAX;
    for (uint32_t i = 0; i < adapter_count; ++i) {
        if (adapters[i].is_software) {
            warp_index = i;
            break;
        }
    }

    if (warp_index == UINT32_MAX) {
        std::printf("SKIP: No WARP adapter found\n");
        return false;
    }

    DXBDeviceDesc desc = {};
    desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    desc.backend        = DXB_BACKEND_DX12;
    desc.adapter_index  = warp_index;
    desc.flags          = request_debug ? DXB_DEVICE_FLAG_DEBUG : 0;

    r = backend.CreateDevice(&desc, out_device);
    if (r != DXB_OK) {
        std::printf("SKIP: CreateDevice failed: %s\n", LastErrorText().c_str());
        return false;
    }

    return true;
}

static bool CreateSwapChainForTest(dxb::DX12Backend& backend,
                                   DXBDevice device,
                                   HWND* out_hwnd,
                                   DXBSwapChain* out_sc) {
    *out_hwnd = CreateHiddenWindow();
    *out_sc = DXBRIDGE_NULL_HANDLE;
    if (!*out_hwnd) {
        std::printf("FAIL: CreateHiddenWindow failed\n");
        return false;
    }

    DXBSwapChainDesc desc = {};
    desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    desc.hwnd           = *out_hwnd;
    desc.width          = 64;
    desc.height         = 64;
    desc.format         = DXB_FORMAT_RGBA8_UNORM;
    desc.buffer_count   = 2;

    if (!ExpectResult("CreateSwapChain(valid)", backend.CreateSwapChain(device, &desc, out_sc), DXB_OK)) {
        DestroyWindow(*out_hwnd);
        *out_hwnd = nullptr;
        return false;
    }

    return true;
}

static bool CreateBackBufferRtv(dxb::DX12Backend& backend,
                                DXBDevice device,
                                DXBSwapChain sc,
                                DXBRenderTarget* out_rt,
                                DXBRTV* out_rtv) {
    *out_rt = DXBRIDGE_NULL_HANDLE;
    *out_rtv = DXBRIDGE_NULL_HANDLE;

    if (!ExpectResult("GetBackBuffer(valid)", backend.GetBackBuffer(sc, out_rt), DXB_OK)) {
        return false;
    }

    if (!ExpectResult("CreateRenderTargetView(valid)",
                      backend.CreateRenderTargetView(device, *out_rt, out_rtv),
                      DXB_OK)) {
        return false;
    }

    return true;
}

static bool CompileBasicShaders(dxb::DX12Backend& backend,
                                DXBDevice device,
                                DXBShader* out_vs,
                                DXBShader* out_ps) {
    *out_vs = DXBRIDGE_NULL_HANDLE;
    *out_ps = DXBRIDGE_NULL_HANDLE;

    static const char* kVsSrc =
        "float4 main(uint vid : SV_VertexID) : SV_POSITION {\n"
        "    float2 pos[3] = { float2(0.0f, 0.5f), float2(0.5f, -0.5f), float2(-0.5f, -0.5f) };\n"
        "    return float4(pos[vid], 0.0f, 1.0f);\n"
        "}\n";

    static const char* kPsSrc =
        "float4 main() : SV_TARGET {\n"
        "    return float4(1.0f, 0.0f, 0.0f, 1.0f);\n"
        "}\n";

    DXBShaderDesc vs_desc = {};
    vs_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    vs_desc.stage          = DXB_SHADER_STAGE_VERTEX;
    vs_desc.source_code    = kVsSrc;
    vs_desc.source_name    = "test_dx12_validation_vs";
    vs_desc.entry_point    = "main";
    vs_desc.target         = "vs_5_0";

    DXBResult r = backend.CompileShader(device, &vs_desc, out_vs);
    if (r != DXB_OK) {
        std::printf("FAIL: CompileShader(VS) failed: %s\n", LastErrorText().c_str());
        return false;
    }

    DXBShaderDesc ps_desc = {};
    ps_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    ps_desc.stage          = DXB_SHADER_STAGE_PIXEL;
    ps_desc.source_code    = kPsSrc;
    ps_desc.source_name    = "test_dx12_validation_ps";
    ps_desc.entry_point    = "main";
    ps_desc.target         = "ps_5_0";

    r = backend.CompileShader(device, &ps_desc, out_ps);
    if (r != DXB_OK) {
        std::printf("FAIL: CompileShader(PS) failed: %s\n", LastErrorText().c_str());
        backend.DestroyShader(*out_vs);
        *out_vs = DXBRIDGE_NULL_HANDLE;
        return false;
    }

    return true;
}

static bool TestPsoDebugValidation() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, true, &device)) {
        backend.Shutdown();
        return true;
    }

    auto* dev = backend.DebugGetDeviceObject(device);
    if (!dev || !dev->debug_enabled) {
        std::printf("SKIP: DX12 debug layer unavailable; PSO debug validation path not testable\n");
        if (device != DXBRIDGE_NULL_HANDLE) {
            backend.DestroyDevice(device);
        }
        backend.Shutdown();
        return true;
    }

    DXBShader vs = DXBRIDGE_NULL_HANDLE;
    DXBShader ps = DXBRIDGE_NULL_HANDLE;
    bool ok = CompileBasicShaders(backend, device, &vs, &ps);
    if (ok) {
        DXBPipelineDesc pso_desc = {};
        pso_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        pso_desc.vs             = vs;
        pso_desc.ps             = ps;
        pso_desc.input_layout   = DXBRIDGE_NULL_HANDLE;
        pso_desc.topology       = DXB_PRIM_TRIANGLELIST;

        DXBPipeline pipeline = DXBRIDGE_NULL_HANDLE;
        ok = ExpectResult("CreatePipelineState(debug missing RT format)",
                          backend.CreatePipelineState(device, &pso_desc, &pipeline),
                          DXB_E_INVALID_STATE);
        if (pipeline != DXBRIDGE_NULL_HANDLE) {
            backend.DestroyPipeline(pipeline);
        }
    }

    if (ps != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyShader(ps);
    }
    if (vs != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyShader(vs);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestFrameLifecycleAndResizeGuards() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, false, &device)) {
        backend.Shutdown();
        return true;
    }

    bool ok = true;
    HWND hwnd = nullptr;
    DXBSwapChain sc = DXBRIDGE_NULL_HANDLE;
    DXBRenderTarget rt = DXBRIDGE_NULL_HANDLE;
    DXBRTV rtv = DXBRIDGE_NULL_HANDLE;
    DXBRenderTarget resized_rt = DXBRIDGE_NULL_HANDLE;
    DXBRTV resized_rtv = DXBRIDGE_NULL_HANDLE;

    if (CreateSwapChainForTest(backend, device, &hwnd, &sc)) {
        auto* dev = backend.DebugGetDeviceObject(device);
        auto* sc_obj = backend.DebugGetSwapChainObject(sc);
        if (!dev || !sc_obj) {
            std::printf("FAIL: debug hooks unavailable for lifecycle validation\n");
            ok = false;
        } else {
            ok = ExpectBool("Frame initially idle", dev->frame_in_progress, false) && ok;
            ok = ExpectUInt32("Initial frame index matches swap chain",
                              dev->frame_index,
                              sc_obj->swapchain->GetCurrentBackBufferIndex()) && ok;

            ok = ExpectResult("BeginFrame(valid)", backend.BeginFrame(device), DXB_OK) && ok;
            ok = ExpectBool("BeginFrame marks frame active", dev->frame_in_progress, true) && ok;
            ok = ExpectResult("BeginFrame(while active)",
                              backend.BeginFrame(device),
                              DXB_E_INVALID_STATE) && ok;
            ok = ExpectBool("Nested BeginFrame keeps frame active", dev->frame_in_progress, true) && ok;

            if (ok) {
                ok = CreateBackBufferRtv(backend, device, sc, &rt, &rtv) && ok;
            }

            if (ok) {
                ok = ExpectResult("SetRenderTargets(valid)",
                                  backend.SetRenderTargets(device, rtv, DXBRIDGE_NULL_HANDLE),
                                  DXB_OK) && ok;
            }

            if (ok) {
                const float clear[4] = {0.1f, 0.2f, 0.3f, 1.0f};
                ok = ExpectResult("ClearRenderTarget(valid)", backend.ClearRenderTarget(rtv, clear), DXB_OK) && ok;
            }

            if (ok) {
                ok = ExpectResult("ResizeSwapChain(during frame)",
                                  backend.ResizeSwapChain(sc, 80, 80),
                                  DXB_E_INVALID_STATE) && ok;
                ok = ExpectResult("Present(during frame)",
                                  backend.Present(sc, 0),
                                  DXB_E_INVALID_STATE) && ok;
                ok = ExpectBool("Invalid Present keeps frame active", dev->frame_in_progress, true) && ok;
            }

            if (ok) {
                ok = ExpectResult("EndFrame(valid)", backend.EndFrame(device), DXB_OK) && ok;
                ok = ExpectBool("EndFrame clears frame active", dev->frame_in_progress, false) && ok;
                ok = ExpectResult("EndFrame(without BeginFrame)",
                                  backend.EndFrame(device),
                                  DXB_E_INVALID_STATE) && ok;
                ok = ExpectBool("Invalid EndFrame keeps frame idle", dev->frame_in_progress, false) && ok;
            }

            if (ok) {
                ok = ExpectResult("Present(valid)", backend.Present(sc, 0), DXB_OK) && ok;
                ok = ExpectUInt32("Present syncs device frame index",
                                  dev->frame_index,
                                  sc_obj->swapchain->GetCurrentBackBufferIndex()) && ok;
            }

            if (ok) {
                ok = ExpectResult("ResizeSwapChain(after frame)",
                                  backend.ResizeSwapChain(sc, 80, 80),
                                  DXB_OK) && ok;
                ok = ExpectUInt32("ResizeSwapChain updates width", sc_obj->width, 80) && ok;
                ok = ExpectUInt32("ResizeSwapChain updates height", sc_obj->height, 80) && ok;
                ok = ExpectUInt32("ResizeSwapChain syncs frame index",
                                  dev->frame_index,
                                  sc_obj->swapchain->GetCurrentBackBufferIndex()) && ok;
            }

            if (ok) {
                ok = CreateBackBufferRtv(backend, device, sc, &resized_rt, &resized_rtv) && ok;
            }

            if (ok) {
                ok = ExpectResult("BeginFrame(after resize)", backend.BeginFrame(device), DXB_OK) && ok;
                ok = ExpectBool("BeginFrame(after resize) marks frame active", dev->frame_in_progress, true) && ok;
                ok = ExpectResult("EndFrame(after resize)", backend.EndFrame(device), DXB_OK) && ok;
                ok = ExpectBool("EndFrame(after resize) clears frame active", dev->frame_in_progress, false) && ok;
                ok = ExpectResult("Present(after resize)", backend.Present(sc, 0), DXB_OK) && ok;
            }
        }
    } else {
        ok = false;
    }

    if (resized_rtv != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyRTV(resized_rtv);
    }
    if (rtv != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyRTV(rtv);
    }
    if (sc != DXBRIDGE_NULL_HANDLE) {
        backend.DestroySwapChain(sc);
    }
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestIndexFormatValidation() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, false, &device)) {
        backend.Shutdown();
        return true;
    }

    bool ok = true;

    DXBBufferDesc buffer_desc = {};
    buffer_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    buffer_desc.flags          = DXB_BUFFER_FLAG_INDEX;
    buffer_desc.byte_size      = 16;

    DXBBuffer buffer = DXBRIDGE_NULL_HANDLE;
    if (!ExpectResult("CreateBuffer(index)", backend.CreateBuffer(device, &buffer_desc, &buffer), DXB_OK)) {
        ok = false;
    }

    if (ok) {
        ok = ExpectResult("SetIndexBuffer(unknown format)",
                          backend.SetIndexBuffer(device, buffer, DXB_FORMAT_UNKNOWN, 0),
                          DXB_E_INVALID_ARG);
    }

    if (buffer != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyBuffer(buffer);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestBufferOffsetValidation() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, false, &device)) {
        backend.Shutdown();
        return true;
    }

    bool ok = true;

    DXBBufferDesc buffer_desc = {};
    buffer_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    buffer_desc.flags          = DXB_BUFFER_FLAG_VERTEX | DXB_BUFFER_FLAG_INDEX;
    buffer_desc.byte_size      = 16;

    DXBBuffer buffer = DXBRIDGE_NULL_HANDLE;
    ok = ExpectResult("CreateBuffer(offset validation)",
                      backend.CreateBuffer(device, &buffer_desc, &buffer),
                      DXB_OK) && ok;

    if (ok) {
        ok = ExpectResult("SetVertexBuffer(offset == size)",
                          backend.SetVertexBuffer(device, buffer, 4, 16),
                          DXB_OK) && ok;

        ok = ExpectResult("SetVertexBuffer(offset > size)",
                          backend.SetVertexBuffer(device, buffer, 4, 20),
                          DXB_E_INVALID_ARG) && ok;
        ok = ExpectLastErrorContains("SetVertexBuffer(offset > size) error text",
                                     "offset exceeds buffer size") && ok;

        ok = ExpectResult("SetIndexBuffer(offset == size)",
                          backend.SetIndexBuffer(device, buffer, DXB_FORMAT_R32_UINT, 16),
                          DXB_OK) && ok;

        ok = ExpectResult("SetIndexBuffer(offset > size)",
                          backend.SetIndexBuffer(device, buffer, DXB_FORMAT_R32_UINT, 20),
                          DXB_E_INVALID_ARG) && ok;
        ok = ExpectLastErrorContains("SetIndexBuffer(offset > size) error text",
                                     "offset exceeds buffer size") && ok;
    }

    if (buffer != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyBuffer(buffer);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestStickyDeviceLostShortCircuits() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, false, &device)) {
        backend.Shutdown();
        return true;
    }

    bool ok = true;
    HWND hwnd = nullptr;
    DXBSwapChain sc = DXBRIDGE_NULL_HANDLE;

    if (!CreateSwapChainForTest(backend, device, &hwnd, &sc)) {
        backend.DestroyDevice(device);
        backend.Shutdown();
        return false;
    }

    auto* dev = backend.DebugGetDeviceObject(device);
    if (!dev) {
        std::printf("FAIL: DebugGetDeviceObject returned null\n");
        backend.DestroySwapChain(sc);
        DestroyWindow(hwnd);
        backend.DestroyDevice(device);
        backend.Shutdown();
        return false;
    }

    DXBBufferDesc upload_buffer_desc = {};
    upload_buffer_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    upload_buffer_desc.flags          = DXB_BUFFER_FLAG_VERTEX;
    upload_buffer_desc.byte_size      = 32;

    DXBBuffer upload_buffer = DXBRIDGE_NULL_HANDLE;
    ok = ExpectResult("CreateBuffer(upload before device lost)",
                      backend.CreateBuffer(device, &upload_buffer_desc, &upload_buffer),
                      DXB_OK) && ok;

    dev->device_lost = true;

    ok = ExpectResult("BeginFrame(device lost)", backend.BeginFrame(device), DXB_E_DEVICE_LOST) && ok;
    ok = ExpectResult("EndFrame(device lost)", backend.EndFrame(device), DXB_E_DEVICE_LOST) && ok;
    ok = ExpectResult("Present(device lost)", backend.Present(sc, 0), DXB_E_DEVICE_LOST) && ok;
    ok = ExpectResult("ResizeSwapChain(device lost)", backend.ResizeSwapChain(sc, 72, 72), DXB_E_DEVICE_LOST) && ok;

    DXBRenderTarget rt = DXBRIDGE_NULL_HANDLE;
    ok = ExpectResult("GetBackBuffer(device lost)", backend.GetBackBuffer(sc, &rt), DXB_E_DEVICE_LOST) && ok;

    const uint8_t upload_data[4] = {1, 2, 3, 4};
    ok = ExpectResult("UploadData(device lost)",
                      backend.UploadData(upload_buffer, upload_data, static_cast<uint32_t>(sizeof(upload_data))),
                      DXB_E_DEVICE_LOST) && ok;

    DXBBufferDesc buffer_desc = {};
    buffer_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    buffer_desc.flags          = DXB_BUFFER_FLAG_VERTEX;
    buffer_desc.byte_size      = 32;

    DXBBuffer buffer = DXBRIDGE_NULL_HANDLE;
    ok = ExpectResult("CreateBuffer(device lost)",
                      backend.CreateBuffer(device, &buffer_desc, &buffer),
                      DXB_E_DEVICE_LOST) && ok;

    DXBInputElementDesc element = {};
    element.struct_version = DXBRIDGE_STRUCT_VERSION;
    element.semantic       = DXB_SEMANTIC_POSITION;
    element.format         = DXB_FORMAT_RGBA32_FLOAT;

    DXBInputLayout layout = DXBRIDGE_NULL_HANDLE;
    ok = ExpectResult("CreateInputLayout(device lost)",
                      backend.CreateInputLayout(device, &element, 1, DXBRIDGE_NULL_HANDLE, &layout),
                      DXB_E_DEVICE_LOST) && ok;

    static const char* kLostVsSrc =
        "float4 main(uint vid : SV_VertexID) : SV_POSITION {\n"
        "    return float4(0.0f, 0.0f, 0.0f, 1.0f);\n"
        "}\n";

    DXBShaderDesc shader_desc = {};
    shader_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    shader_desc.stage          = DXB_SHADER_STAGE_VERTEX;
    shader_desc.source_code    = kLostVsSrc;
    shader_desc.source_name    = "test_dx12_validation_device_lost_vs";
    shader_desc.entry_point    = "main";
    shader_desc.target         = "vs_5_0";

    DXBShader shader = DXBRIDGE_NULL_HANDLE;
    ok = ExpectResult("CompileShader(device lost)",
                      backend.CompileShader(device, &shader_desc, &shader),
                      DXB_E_DEVICE_LOST) && ok;

    if (layout != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyInputLayout(layout);
    }
    if (shader != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyShader(shader);
    }
    if (upload_buffer != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyBuffer(upload_buffer);
    }
    if (buffer != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyBuffer(buffer);
    }
    backend.DestroySwapChain(sc);
    DestroyWindow(hwnd);
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestSwapChainUnknownFormat() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, false, &device)) {
        backend.Shutdown();
        return true;
    }

    HWND hwnd = CreateHiddenWindow();
    if (!hwnd) {
        std::printf("FAIL: CreateHiddenWindow failed\n");
        backend.DestroyDevice(device);
        backend.Shutdown();
        return false;
    }

    DXBSwapChainDesc desc = {};
    desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    desc.hwnd           = hwnd;
    desc.width          = 64;
    desc.height         = 64;
    desc.format         = DXB_FORMAT_UNKNOWN;
    desc.buffer_count   = 2;

    DXBSwapChain sc = DXBRIDGE_NULL_HANDLE;
    bool ok = ExpectResult("CreateSwapChain(unknown format)",
                           backend.CreateSwapChain(device, &desc, &sc),
                           DXB_E_INVALID_ARG);
    ok = ExpectLastErrorContains("CreateSwapChain(unknown format) error text",
                                 "unsupported format value") && ok;

    if (sc != DXBRIDGE_NULL_HANDLE) {
        backend.DestroySwapChain(sc);
    }
    DestroyWindow(hwnd);
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestDescriptorHeapExhaustion() {
    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, false, &device)) {
        backend.Shutdown();
        return true;
    }

    bool ok = true;
    std::vector<HWND> windows;
    std::vector<DXBSwapChain> swapchains;
    std::vector<DXBDSV> dsvs;

    DXBSwapChainDesc sc_desc = {};
    sc_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    sc_desc.width          = 64;
    sc_desc.height         = 64;
    sc_desc.format         = DXB_FORMAT_RGBA8_UNORM;
    sc_desc.buffer_count   = 3;

    for (int i = 0; i < 5 && ok; ++i) {
        HWND hwnd = CreateHiddenWindow();
        if (!hwnd) {
            std::printf("FAIL: CreateHiddenWindow failed for swap chain %d\n", i);
            ok = false;
            break;
        }

        windows.push_back(hwnd);
        sc_desc.hwnd = hwnd;

        DXBSwapChain sc = DXBRIDGE_NULL_HANDLE;
        if (!ExpectResult("CreateSwapChain(fill RTV heap)", backend.CreateSwapChain(device, &sc_desc, &sc), DXB_OK)) {
            ok = false;
            break;
        }
        swapchains.push_back(sc);
    }

    HWND overflow_hwnd = nullptr;
    if (ok) {
        overflow_hwnd = CreateHiddenWindow();
        if (!overflow_hwnd) {
            std::printf("FAIL: CreateHiddenWindow failed for RTV overflow\n");
            ok = false;
        } else {
            windows.push_back(overflow_hwnd);
            sc_desc.hwnd = overflow_hwnd;
            DXBSwapChain sc = DXBRIDGE_NULL_HANDLE;
            ok = ExpectResult("CreateSwapChain(RTV heap exhausted)",
                              backend.CreateSwapChain(device, &sc_desc, &sc),
                              DXB_E_OUT_OF_MEMORY);
            if (sc != DXBRIDGE_NULL_HANDLE) {
                backend.DestroySwapChain(sc);
            }
        }
    }

    DXBDepthStencilDesc dsv_desc = {};
    dsv_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    dsv_desc.format         = DXB_FORMAT_D32_FLOAT;
    dsv_desc.width          = 64;
    dsv_desc.height         = 64;

    for (int i = 0; i < 8 && ok; ++i) {
        DXBDSV dsv = DXBRIDGE_NULL_HANDLE;
        if (!ExpectResult("CreateDepthStencilView(fill DSV heap)",
                          backend.CreateDepthStencilView(device, &dsv_desc, &dsv),
                          DXB_OK)) {
            ok = false;
            break;
        }
        dsvs.push_back(dsv);
    }

    if (ok) {
        DXBDSV dsv = DXBRIDGE_NULL_HANDLE;
        ok = ExpectResult("CreateDepthStencilView(DSV heap exhausted)",
                          backend.CreateDepthStencilView(device, &dsv_desc, &dsv),
                          DXB_E_OUT_OF_MEMORY);
        if (dsv != DXBRIDGE_NULL_HANDLE) {
            backend.DestroyDSV(dsv);
        }
    }

    for (DXBDSV dsv : dsvs) {
        backend.DestroyDSV(dsv);
    }
    for (DXBSwapChain sc : swapchains) {
        backend.DestroySwapChain(sc);
    }
    for (HWND hwnd : windows) {
        DestroyWindow(hwnd);
    }

    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestDiagnosticLogs() {
    ScopedLogCapture logs;

    dxb::DX12Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, true, &device)) {
        backend.Shutdown();
        return true;
    }

    bool ok = true;

    auto* dev = backend.DebugGetDeviceObject(device);
    if (!dev || !dev->debug_enabled) {
        std::printf("SKIP: DX12 debug layer unavailable; diagnostic log coverage partial\n");
    } else {
        DXBShader vs = DXBRIDGE_NULL_HANDLE;
        DXBShader ps = DXBRIDGE_NULL_HANDLE;
        if (CompileBasicShaders(backend, device, &vs, &ps)) {
            DXBPipelineDesc pso_desc = {};
            pso_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
            pso_desc.vs             = vs;
            pso_desc.ps             = ps;
            pso_desc.topology       = DXB_PRIM_TRIANGLELIST;

            DXBPipeline pipeline = DXBRIDGE_NULL_HANDLE;
            ok = ExpectResult("CreatePipelineState(debug diagnostics)",
                              backend.CreatePipelineState(device, &pso_desc, &pipeline),
                              DXB_E_INVALID_STATE) && ok;
            ok = ExpectLogContains("CreatePipelineState(debug diagnostics) log",
                                   logs,
                                   dxb::LOG_WARN,
                                   "debug validation") && ok;
            if (pipeline != DXBRIDGE_NULL_HANDLE) {
                backend.DestroyPipeline(pipeline);
            }
        } else {
            ok = false;
        }

        if (ps != DXBRIDGE_NULL_HANDLE) {
            backend.DestroyShader(ps);
        }
        if (vs != DXBRIDGE_NULL_HANDLE) {
            backend.DestroyShader(vs);
        }
    }

    HWND hwnd = nullptr;
    DXBSwapChain sc = DXBRIDGE_NULL_HANDLE;
    if (CreateSwapChainForTest(backend, device, &hwnd, &sc)) {
        ok = ExpectResult("BeginFrame(valid for diagnostics)", backend.BeginFrame(device), DXB_OK) && ok;
        ok = ExpectResult("BeginFrame(nested diagnostics)", backend.BeginFrame(device), DXB_E_INVALID_STATE) && ok;
        ok = ExpectLogContains("BeginFrame(nested diagnostics) log",
                               logs,
                               dxb::LOG_WARN,
                               "already active") && ok;

        ok = ExpectResult("ResizeSwapChain(during frame diagnostics)",
                          backend.ResizeSwapChain(sc, 96, 96),
                          DXB_E_INVALID_STATE) && ok;
        ok = ExpectLogContains("ResizeSwapChain(during frame diagnostics) log",
                               logs,
                               dxb::LOG_WARN,
                               "ResizeSwapChain rejected") && ok;

        ok = ExpectResult("Present(during frame diagnostics)", backend.Present(sc, 0), DXB_E_INVALID_STATE) && ok;
        ok = ExpectLogContains("Present(during frame diagnostics) log",
                               logs,
                               dxb::LOG_WARN,
                               "Present rejected") && ok;

        ok = ExpectResult("EndFrame(cleanup diagnostics)", backend.EndFrame(device), DXB_OK) && ok;
    } else {
        ok = false;
    }

    if (sc != DXBRIDGE_NULL_HANDLE) {
        backend.DestroySwapChain(sc);
    }
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

} // namespace

int main() {
    std::printf("=== DX12 validation regression tests ===\n");

    bool ok = true;
    ok = TestPsoDebugValidation() && ok;
    ok = TestFrameLifecycleAndResizeGuards() && ok;
    ok = TestIndexFormatValidation() && ok;
    ok = TestBufferOffsetValidation() && ok;
    ok = TestStickyDeviceLostShortCircuits() && ok;
    ok = TestSwapChainUnknownFormat() && ok;
    ok = TestDescriptorHeapExhaustion() && ok;
    ok = TestDiagnosticLogs() && ok;

    if (!ok) {
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
