#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "dx11/device_dx11.hpp"

#include "common/error.hpp"

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr const char* kWindowClassName = "DXBridgeDx11ValidationWindow";

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
        "dxbridge-dx11-validation",
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

static bool ExpectLastErrorContains(const char* label, const char* needle) {
    const std::string text = LastErrorText();
    if (text.find(needle) != std::string::npos) {
        return true;
    }

    std::printf("FAIL: %s missing '%s': %s\n", label, needle, text.c_str());
    return false;
}

static bool CompileBasicShaders(dxb::DX11Backend& backend,
                                DXBDevice device,
                                DXBShader* out_vs,
                                DXBShader* out_ps) {
    *out_vs = DXBRIDGE_NULL_HANDLE;
    *out_ps = DXBRIDGE_NULL_HANDLE;

    static const char* kVsSrc =
        "struct VSInput { float2 pos : POSITION; float3 col : COLOR; };\n"
        "float4 main(VSInput input) : SV_POSITION {\n"
        "    return float4(input.pos, 0.0f, 1.0f);\n"
        "}\n";

    static const char* kPsSrc =
        "float4 main() : SV_TARGET {\n"
        "    return float4(1.0f, 0.0f, 0.0f, 1.0f);\n"
        "}\n";

    DXBShaderDesc vs_desc = {};
    vs_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    vs_desc.stage          = DXB_SHADER_STAGE_VERTEX;
    vs_desc.source_code    = kVsSrc;
    vs_desc.source_name    = "test_dx11_validation_vs";
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
    ps_desc.source_name    = "test_dx11_validation_ps";
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

static DXBInputElementDesc MakePositionElement(DXBFormat format = DXB_FORMAT_RG32_FLOAT) {
    DXBInputElementDesc element = {};
    element.struct_version = DXBRIDGE_STRUCT_VERSION;
    element.semantic       = DXB_SEMANTIC_POSITION;
    element.semantic_index = 0;
    element.format         = format;
    element.input_slot     = 0;
    element.byte_offset    = 0;
    return element;
}

static DXBInputElementDesc MakeColorElement() {
    DXBInputElementDesc element = {};
    element.struct_version = DXBRIDGE_STRUCT_VERSION;
    element.semantic       = DXB_SEMANTIC_COLOR;
    element.semantic_index = 0;
    element.format         = DXB_FORMAT_RGB32_FLOAT;
    element.input_slot     = 0;
    element.byte_offset    = 8;
    return element;
}

static bool CreateWarpDevice(dxb::DX11Backend& backend, DXBDevice* out_device) {
    *out_device = DXBRIDGE_NULL_HANDLE;

    DXBResult r = backend.Init();
    if (r != DXB_OK) {
        std::printf("SKIP: DX11Backend::Init failed: %s\n", LastErrorText().c_str());
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
    desc.backend        = DXB_BACKEND_DX11;
    desc.adapter_index  = warp_index;

    r = backend.CreateDevice(&desc, out_device);
    if (r != DXB_OK) {
        std::printf("SKIP: CreateDevice failed: %s\n", LastErrorText().c_str());
        return false;
    }

    return true;
}

static bool ReadBackBufferContents(dxb::DX11Backend& backend,
                                   DXBDevice device,
                                   DXBBuffer buffer,
                                   uint32_t byte_size,
                                   std::vector<uint8_t>* out_data) {
    out_data->clear();

    auto* d3d = static_cast<ID3D11Device*>(backend.GetNativeDevice(device));
    auto* ctx = static_cast<ID3D11DeviceContext*>(backend.GetNativeContext(device));
    auto* buffer_obj = backend.DebugGetBufferObject(buffer);
    if (!d3d || !ctx || !buffer_obj || !buffer_obj->buffer) {
        std::printf("FAIL: ReadBackBufferContents missing native DX11 objects\n");
        return false;
    }

    D3D11_BUFFER_DESC staging_desc = {};
    staging_desc.ByteWidth      = byte_size;
    staging_desc.Usage          = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Microsoft::WRL::ComPtr<ID3D11Buffer> staging_buffer;
    HRESULT hr = d3d->CreateBuffer(&staging_desc, nullptr, staging_buffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        std::printf("FAIL: ReadBackBufferContents CreateBuffer(staging) HRESULT=0x%08X\n",
                    static_cast<unsigned>(hr));
        return false;
    }

    ctx->CopyResource(staging_buffer.Get(), buffer_obj->buffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    hr = ctx->Map(staging_buffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr) || !mapped.pData) {
        std::printf("FAIL: ReadBackBufferContents Map(staging) HRESULT=0x%08X\n",
                    static_cast<unsigned>(hr));
        return false;
    }

    const auto* bytes = static_cast<const uint8_t*>(mapped.pData);
    out_data->assign(bytes, bytes + byte_size);
    ctx->Unmap(staging_buffer.Get(), 0);
    return true;
}

static bool TestSwapChainUnknownFormat() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
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
    bool ok = ExpectResult("DX11 CreateSwapChain(unknown format)",
                           backend.CreateSwapChain(device, &desc, &sc),
                           DXB_E_INVALID_ARG);
    ok = ExpectLastErrorContains("DX11 CreateSwapChain(unknown format) error text",
                                 "unsupported format value") && ok;

    if (sc != DXBRIDGE_NULL_HANDLE) {
        backend.DestroySwapChain(sc);
    }
    DestroyWindow(hwnd);
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestDepthStencilUnknownFormat() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
        backend.Shutdown();
        return true;
    }

    DXBDepthStencilDesc desc = {};
    desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    desc.format         = DXB_FORMAT_UNKNOWN;
    desc.width          = 64;
    desc.height         = 64;

    DXBDSV dsv = DXBRIDGE_NULL_HANDLE;
    bool ok = ExpectResult("DX11 CreateDepthStencilView(unknown format)",
                           backend.CreateDepthStencilView(device, &desc, &dsv),
                           DXB_E_INVALID_ARG);

    if (dsv != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyDSV(dsv);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestIndexBufferUnknownFormat() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
        backend.Shutdown();
        return true;
    }

    DXBBufferDesc desc = {};
    desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    desc.flags          = DXB_BUFFER_FLAG_INDEX;
    desc.byte_size      = 16;

    DXBBuffer buffer = DXBRIDGE_NULL_HANDLE;
    bool ok = ExpectResult("DX11 CreateBuffer(index)", backend.CreateBuffer(device, &desc, &buffer), DXB_OK);
    if (ok) {
        ok = ExpectResult("DX11 SetIndexBuffer(unknown format)",
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

static bool TestStaticBufferPartialUpload() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
        backend.Shutdown();
        return true;
    }

    DXBBufferDesc desc = {};
    desc.struct_version = DXBRIDGE_STRUCT_VERSION;
    desc.flags          = DXB_BUFFER_FLAG_VERTEX;
    desc.byte_size      = 256;

    DXBBuffer buffer = DXBRIDGE_NULL_HANDLE;
    bool ok = ExpectResult("DX11 CreateBuffer(static vertex)", backend.CreateBuffer(device, &desc, &buffer), DXB_OK);
    if (ok) {
        const uint8_t upload_data[16] = {
            0x01u, 0x02u, 0x03u, 0x04u,
            0x05u, 0x06u, 0x07u, 0x08u,
            0x09u, 0x0Au, 0x0Bu, 0x0Cu,
            0x0Du, 0x0Eu, 0x0Fu, 0x10u,
        };
        ok = ExpectResult("DX11 UploadData(static partial)",
                          backend.UploadData(buffer, upload_data, static_cast<uint32_t>(sizeof(upload_data))),
                          DXB_OK);

        if (ok) {
            std::vector<uint8_t> readback_data;
            ok = ReadBackBufferContents(backend, device, buffer, desc.byte_size, &readback_data) && ok;
            if (ok) {
                const uint32_t upload_size = static_cast<uint32_t>(sizeof(upload_data));
                for (uint32_t i = 0; i < upload_size; ++i) {
                    if (readback_data[i] != upload_data[i]) {
                        std::printf("FAIL: DX11 UploadData(static partial) mismatch at byte %u: got %u expected %u\n",
                                    i,
                                    static_cast<unsigned>(readback_data[i]),
                                    static_cast<unsigned>(upload_data[i]));
                        ok = false;
                        break;
                    }
                }

                for (uint32_t i = upload_size; ok && i < desc.byte_size; ++i) {
                    if (readback_data[i] != 0u) {
                        std::printf("FAIL: DX11 UploadData(static partial) expected zero padding at byte %u but found %u\n",
                                    i,
                                    static_cast<unsigned>(readback_data[i]));
                        ok = false;
                    }
                }
            }
        }
    }

    if (buffer != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyBuffer(buffer);
    }
    backend.DestroyDevice(device);
    backend.Shutdown();
    return ok;
}

static bool TestInputLayoutInvalidDevice() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
        backend.Shutdown();
        return true;
    }

    DXBShader vs = DXBRIDGE_NULL_HANDLE;
    DXBShader ps = DXBRIDGE_NULL_HANDLE;
    bool ok = CompileBasicShaders(backend, device, &vs, &ps);

    if (ok) {
        DXBInputElementDesc element = MakePositionElement();
        DXBInputLayout layout = static_cast<DXBInputLayout>(1);
        ok = ExpectResult("DX11 CreateInputLayout(invalid device)",
                          backend.CreateInputLayout(DXBRIDGE_NULL_HANDLE, &element, 1, vs, &layout),
                          DXB_E_INVALID_HANDLE) && ok;
        ok = ExpectLastErrorContains("DX11 CreateInputLayout(invalid device) error text",
                                     "invalid device") && ok;
        ok = (layout == DXBRIDGE_NULL_HANDLE) && ok;
        if (layout != DXBRIDGE_NULL_HANDLE) {
            std::printf("FAIL: DX11 CreateInputLayout(invalid device) produced non-null layout\n");
            backend.DestroyInputLayout(layout);
            layout = DXBRIDGE_NULL_HANDLE;
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

static bool TestInputLayoutInvalidShaderOrDescriptor() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
        backend.Shutdown();
        return true;
    }

    DXBShader vs = DXBRIDGE_NULL_HANDLE;
    DXBShader ps = DXBRIDGE_NULL_HANDLE;
    bool ok = CompileBasicShaders(backend, device, &vs, &ps);

    if (ok) {
        DXBInputElementDesc valid_element = MakePositionElement();
        DXBInputLayout layout = DXBRIDGE_NULL_HANDLE;
        ok = ExpectResult("DX11 CreateInputLayout(invalid vertex shader)",
                          backend.CreateInputLayout(device, &valid_element, 1, DXBRIDGE_NULL_HANDLE, &layout),
                          DXB_E_INVALID_HANDLE) && ok;
        ok = ExpectLastErrorContains("DX11 CreateInputLayout(invalid vertex shader) error text",
                                     "invalid vertex shader") && ok;
        ok = (layout == DXBRIDGE_NULL_HANDLE) && ok;

        DXBInputElementDesc bad_format = MakePositionElement(DXB_FORMAT_UNKNOWN);
        ok = ExpectResult("DX11 CreateInputLayout(unknown element format)",
                          backend.CreateInputLayout(device, &bad_format, 1, vs, &layout),
                          DXB_E_INVALID_ARG) && ok;
        ok = ExpectLastErrorContains("DX11 CreateInputLayout(unknown element format) error text",
                                     "unrecognised format") && ok;
        ok = (layout == DXBRIDGE_NULL_HANDLE) && ok;

        ok = ExpectResult("DX11 CreateInputLayout(null elements)",
                          backend.CreateInputLayout(device, nullptr, 1, vs, &layout),
                          DXB_E_INVALID_ARG) && ok;
        ok = ExpectLastErrorContains("DX11 CreateInputLayout(null elements) error text",
                                     "no elements") && ok;
        ok = (layout == DXBRIDGE_NULL_HANDLE) && ok;
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

static bool TestPipelineInvalidInputLayoutHandle() {
    dxb::DX11Backend backend;
    DXBDevice device = DXBRIDGE_NULL_HANDLE;
    if (!CreateWarpDevice(backend, &device)) {
        backend.Shutdown();
        return true;
    }

    DXBShader vs = DXBRIDGE_NULL_HANDLE;
    DXBShader ps = DXBRIDGE_NULL_HANDLE;
    DXBInputLayout valid_layout = DXBRIDGE_NULL_HANDLE;
    bool ok = CompileBasicShaders(backend, device, &vs, &ps);

    if (ok) {
        DXBInputElementDesc elements[2] = { MakePositionElement(), MakeColorElement() };
        ok = ExpectResult("DX11 CreateInputLayout(valid)",
                          backend.CreateInputLayout(device, elements, 2, vs, &valid_layout),
                          DXB_OK) && ok;
    }

    if (ok) {
        DXBPipelineDesc pso_desc = {};
        pso_desc.struct_version = DXBRIDGE_STRUCT_VERSION;
        pso_desc.vs             = vs;
        pso_desc.ps             = ps;
        pso_desc.input_layout   = static_cast<DXBInputLayout>(0x1234);
        pso_desc.topology       = DXB_PRIM_TRIANGLELIST;

        DXBPipeline pipeline = static_cast<DXBPipeline>(1);
        ok = ExpectResult("DX11 CreatePipelineState(invalid input layout)",
                          backend.CreatePipelineState(device, &pso_desc, &pipeline),
                          DXB_E_INVALID_HANDLE) && ok;
        ok = ExpectLastErrorContains("DX11 CreatePipelineState(invalid input layout) error text",
                                     "invalid input layout") && ok;
        ok = (pipeline == DXBRIDGE_NULL_HANDLE) && ok;
        if (pipeline != DXBRIDGE_NULL_HANDLE) {
            std::printf("FAIL: DX11 CreatePipelineState(invalid input layout) produced non-null pipeline\n");
            backend.DestroyPipeline(pipeline);
            pipeline = DXBRIDGE_NULL_HANDLE;
        }

        pso_desc.input_layout = valid_layout;
        ok = ExpectResult("DX11 CreatePipelineState(valid input layout)",
                          backend.CreatePipelineState(device, &pso_desc, &pipeline),
                          DXB_OK) && ok;
        if (pipeline != DXBRIDGE_NULL_HANDLE) {
            backend.DestroyPipeline(pipeline);
        }
    }

    if (valid_layout != DXBRIDGE_NULL_HANDLE) {
        backend.DestroyInputLayout(valid_layout);
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

} // namespace

int main() {
    std::printf("=== DX11 validation regression tests ===\n");

    bool ok = true;
    ok = TestSwapChainUnknownFormat() && ok;
    ok = TestDepthStencilUnknownFormat() && ok;
    ok = TestIndexBufferUnknownFormat() && ok;
    ok = TestStaticBufferPartialUpload() && ok;
    ok = TestInputLayoutInvalidDevice() && ok;
    ok = TestInputLayoutInvalidShaderOrDescriptor() && ok;
    ok = TestPipelineInvalidInputLayoutHandle() && ok;

    if (!ok) {
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
