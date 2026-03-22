#include "dxbridge/dxbridge.h"

#include <Windows.h>

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

static_assert(sizeof(DXBResult) == 4, "DXBResult must stay 32-bit");
static_assert(sizeof(DXBDevice) == 8, "DXBDevice must stay 64-bit");
static_assert(sizeof(DXBSwapChain) == 8, "DXBSwapChain must stay 64-bit");
static_assert(sizeof(DXBBuffer) == 8, "DXBBuffer must stay 64-bit");
static_assert(sizeof(DXBShader) == 8, "DXBShader must stay 64-bit");
static_assert(sizeof(DXBPipeline) == 8, "DXBPipeline must stay 64-bit");
static_assert(sizeof(DXBInputLayout) == 8, "DXBInputLayout must stay 64-bit");
static_assert(sizeof(DXBRTV) == 8, "DXBRTV must stay 64-bit");
static_assert(sizeof(DXBDSV) == 8, "DXBDSV must stay 64-bit");
static_assert(sizeof(DXBRenderTarget) == 8, "DXBRenderTarget must stay 64-bit");

static_assert(offsetof(DXBAdapterInfo, struct_version) == 0, "DXBAdapterInfo version offset changed");
static_assert(offsetof(DXBDeviceDesc, struct_version) == 0, "DXBDeviceDesc version offset changed");
static_assert(offsetof(DXBSwapChainDesc, struct_version) == 0, "DXBSwapChainDesc version offset changed");
static_assert(offsetof(DXBBufferDesc, struct_version) == 0, "DXBBufferDesc version offset changed");
static_assert(offsetof(DXBShaderDesc, struct_version) == 0, "DXBShaderDesc version offset changed");
static_assert(offsetof(DXBInputElementDesc, struct_version) == 0, "DXBInputElementDesc version offset changed");
static_assert(offsetof(DXBDepthStencilDesc, struct_version) == 0, "DXBDepthStencilDesc version offset changed");
static_assert(offsetof(DXBPipelineDesc, struct_version) == 0, "DXBPipelineDesc version offset changed");
static_assert(offsetof(DXBCapabilityQuery, struct_version) == 0,  "DXBCapabilityQuery version offset changed");
static_assert(offsetof(DXBCapabilityQuery, device)        == 24, "DXBCapabilityQuery 'device' must be at offset 24 (4-byte explicit pad after 'format')");
static_assert(offsetof(DXBCapabilityInfo, struct_version) == 0,  "DXBCapabilityInfo version offset changed");

static_assert(sizeof(DXBAdapterInfo) == 168, "DXBAdapterInfo layout changed unexpectedly");
static_assert(sizeof(DXBDeviceDesc) == 16, "DXBDeviceDesc layout changed unexpectedly");
static_assert(sizeof(DXBSwapChainDesc) == 40, "DXBSwapChainDesc layout changed unexpectedly");
static_assert(sizeof(DXBBufferDesc) == 16, "DXBBufferDesc layout changed unexpectedly");
static_assert(sizeof(DXBShaderDesc) == 56, "DXBShaderDesc layout changed unexpectedly");
static_assert(sizeof(DXBInputElementDesc) == 24, "DXBInputElementDesc layout changed unexpectedly");
static_assert(sizeof(DXBDepthStencilDesc) == 16, "DXBDepthStencilDesc layout changed unexpectedly");
static_assert(sizeof(DXBPipelineDesc) == 56, "DXBPipelineDesc layout changed unexpectedly");
static_assert(sizeof(DXBViewport) == 24, "DXBViewport layout changed unexpectedly");
static_assert(sizeof(DXBCapabilityQuery) == 48, "DXBCapabilityQuery layout changed unexpectedly");
static_assert(sizeof(DXBCapabilityInfo) == 48, "DXBCapabilityInfo layout changed unexpectedly");

static_assert(DXB_CAPABILITY_BACKEND_AVAILABLE == 1u, "Capability id drifted");
static_assert(DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE == 2u, "Capability id drifted");
static_assert(DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE == 3u, "Capability id drifted");
static_assert(DXB_CAPABILITY_ADAPTER_COUNT == 4u, "Capability id drifted");
static_assert(DXB_CAPABILITY_ADAPTER_SOFTWARE == 5u, "Capability id drifted");
static_assert(DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL == 6u, "Capability id drifted");

namespace {

const char* kDllName = "dxbridge.dll";

using GetVersionFn = uint32_t(__cdecl*)();

bool ExpectString(const char* label, const std::string& actual, const char* expected) {
    if (actual == expected) {
        return true;
    }

    std::printf("FAIL: %s was '%s' (expected '%s')\n", label, actual.c_str(), expected);
    return false;
}

bool ExpectTrue(const char* label, bool actual) {
    if (actual) {
        return true;
    }

    std::printf("FAIL: %s\n", label);
    return false;
}

std::string Trim(const std::string& input) {
    const size_t begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }

    const size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

bool LoadExports(std::map<int, std::string>* out_exports) {
    if (!out_exports) {
        return false;
    }

    out_exports->clear();

    std::ifstream file(DXB_SOURCE_DIR "\\src\\dxbridge.def");
    if (!file) {
        std::printf("FAIL: could not open %s\\src\\dxbridge.def\n", DXB_SOURCE_DIR);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed == "LIBRARY dxbridge" || trimmed == "EXPORTS") {
            continue;
        }

        std::istringstream iss(trimmed);
        std::string name;
        std::string ordinal_token;
        if (!(iss >> name >> ordinal_token)) {
            continue;
        }
        if (ordinal_token.empty() || ordinal_token[0] != '@') {
            continue;
        }

        const int ordinal = std::stoi(ordinal_token.substr(1));
        (*out_exports)[ordinal] = name;
    }

    return true;
}

std::string GetExecutableDirectory() {
    char path[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(sizeof(path)));
    if (size == 0 || size >= sizeof(path)) {
        return std::string();
    }

    std::string result(path, size);
    const size_t slash = result.find_last_of("\\/");
    if (slash == std::string::npos) {
        return std::string();
    }

    return result.substr(0, slash);
}

bool TestDefOrdinals() {
    static const struct {
        int ordinal;
        const char* name;
    } kExpected[] = {
        {1, "DXBridge_Init"},
        {2, "DXBridge_Shutdown"},
        {3, "DXBridge_GetVersion"},
        {4, "DXBridge_SupportsFeature"},
        {5, "DXBridge_GetLastError"},
        {6, "DXBridge_GetLastHRESULT"},
        {7, "DXBridge_SetLogCallback"},
        {8, "DXBridge_EnumerateAdapters"},
        {9, "DXBridge_CreateDevice"},
        {10, "DXBridge_DestroyDevice"},
        {11, "DXBridge_GetFeatureLevel"},
        {12, "DXBridge_CreateSwapChain"},
        {13, "DXBridge_DestroySwapChain"},
        {14, "DXBridge_ResizeSwapChain"},
        {15, "DXBridge_GetBackBuffer"},
        {16, "DXBridge_CreateRenderTargetView"},
        {17, "DXBridge_DestroyRTV"},
        {18, "DXBridge_CreateDepthStencilView"},
        {19, "DXBridge_DestroyDSV"},
        {20, "DXBridge_CompileShader"},
        {21, "DXBridge_DestroyShader"},
        {22, "DXBridge_CreateInputLayout"},
        {23, "DXBridge_DestroyInputLayout"},
        {24, "DXBridge_CreatePipelineState"},
        {25, "DXBridge_DestroyPipeline"},
        {26, "DXBridge_CreateBuffer"},
        {27, "DXBridge_UploadData"},
        {28, "DXBridge_DestroyBuffer"},
        {29, "DXBridge_BeginFrame"},
        {30, "DXBridge_SetRenderTargets"},
        {31, "DXBridge_ClearRenderTarget"},
        {32, "DXBridge_ClearDepthStencil"},
        {33, "DXBridge_SetViewport"},
        {34, "DXBridge_SetPipeline"},
        {35, "DXBridge_SetVertexBuffer"},
        {36, "DXBridge_SetIndexBuffer"},
        {37, "DXBridge_SetConstantBuffer"},
        {38, "DXBridge_Draw"},
        {39, "DXBridge_DrawIndexed"},
        {40, "DXBridge_EndFrame"},
        {41, "DXBridge_Present"},
        {42, "DXBridge_GetNativeDevice"},
        {43, "DXBridge_GetNativeContext"},
        {44, "DXBridge_QueryCapability"},
    };

    std::map<int, std::string> exports;
    if (!LoadExports(&exports)) {
        return false;
    }

    bool ok = true;
    for (const auto& expected : kExpected) {
        const auto it = exports.find(expected.ordinal);
        if (it == exports.end()) {
            std::printf("FAIL: missing ordinal %d\n", expected.ordinal);
            ok = false;
            continue;
        }

        ok = ExpectString(expected.name, it->second, expected.name) && ok;
    }

    if (exports.size() != (sizeof(kExpected) / sizeof(kExpected[0]))) {
        std::printf("FAIL: export count was %zu (expected %zu)\n",
                    exports.size(),
                    sizeof(kExpected) / sizeof(kExpected[0]));
        ok = false;
    }

    return ok;
}

bool TestBinaryExports() {
    static const struct {
        int ordinal;
        const char* name;
    } kExpected[] = {
        {1, "DXBridge_Init"},
        {2, "DXBridge_Shutdown"},
        {3, "DXBridge_GetVersion"},
        {4, "DXBridge_SupportsFeature"},
        {5, "DXBridge_GetLastError"},
        {6, "DXBridge_GetLastHRESULT"},
        {7, "DXBridge_SetLogCallback"},
        {8, "DXBridge_EnumerateAdapters"},
        {9, "DXBridge_CreateDevice"},
        {10, "DXBridge_DestroyDevice"},
        {11, "DXBridge_GetFeatureLevel"},
        {12, "DXBridge_CreateSwapChain"},
        {13, "DXBridge_DestroySwapChain"},
        {14, "DXBridge_ResizeSwapChain"},
        {15, "DXBridge_GetBackBuffer"},
        {16, "DXBridge_CreateRenderTargetView"},
        {17, "DXBridge_DestroyRTV"},
        {18, "DXBridge_CreateDepthStencilView"},
        {19, "DXBridge_DestroyDSV"},
        {20, "DXBridge_CompileShader"},
        {21, "DXBridge_DestroyShader"},
        {22, "DXBridge_CreateInputLayout"},
        {23, "DXBridge_DestroyInputLayout"},
        {24, "DXBridge_CreatePipelineState"},
        {25, "DXBridge_DestroyPipeline"},
        {26, "DXBridge_CreateBuffer"},
        {27, "DXBridge_UploadData"},
        {28, "DXBridge_DestroyBuffer"},
        {29, "DXBridge_BeginFrame"},
        {30, "DXBridge_SetRenderTargets"},
        {31, "DXBridge_ClearRenderTarget"},
        {32, "DXBridge_ClearDepthStencil"},
        {33, "DXBridge_SetViewport"},
        {34, "DXBridge_SetPipeline"},
        {35, "DXBridge_SetVertexBuffer"},
        {36, "DXBridge_SetIndexBuffer"},
        {37, "DXBridge_SetConstantBuffer"},
        {38, "DXBridge_Draw"},
        {39, "DXBridge_DrawIndexed"},
        {40, "DXBridge_EndFrame"},
        {41, "DXBridge_Present"},
        {42, "DXBridge_GetNativeDevice"},
        {43, "DXBridge_GetNativeContext"},
        {44, "DXBridge_QueryCapability"},
    };

    const std::string directory = GetExecutableDirectory();
    if (directory.empty()) {
        std::printf("FAIL: could not resolve executable directory\n");
        return false;
    }

    const std::string dll_path = directory + "\\" + kDllName;
    HMODULE module = LoadLibraryA(dll_path.c_str());
    if (!module) {
        std::printf("FAIL: could not load %s\n", dll_path.c_str());
        return false;
    }

    bool ok = true;
    for (const auto& expected : kExpected) {
        FARPROC by_name = GetProcAddress(module, expected.name);
        if (!by_name) {
            std::printf("FAIL: missing binary export by name: %s\n", expected.name);
            ok = false;
            continue;
        }

        FARPROC by_ordinal = GetProcAddress(module, MAKEINTRESOURCEA(expected.ordinal));
        if (!by_ordinal) {
            std::printf("FAIL: missing binary export ordinal %d (%s)\n", expected.ordinal, expected.name);
            ok = false;
            continue;
        }

        ok = ExpectTrue(expected.name, by_name == by_ordinal) && ok;
    }

    FARPROC proc = GetProcAddress(module, "DXBridge_GetVersion");
    if (!proc) {
        std::printf("FAIL: DXBridge_GetVersion missing in binary\n");
        ok = false;
    } else {
        const auto get_version = reinterpret_cast<GetVersionFn>(proc);
        const uint32_t packed_version = get_version();
        if (packed_version != DXBRIDGE_VERSION) {
            std::printf("FAIL: binary version 0x%08X did not match header version 0x%08X\n",
                        packed_version,
                        DXBRIDGE_VERSION);
            ok = false;
        }
    }

    FreeLibrary(module);
    return ok;
}

} // namespace

int main() {
    std::printf("=== Public ABI contract tests ===\n");

    if (!TestDefOrdinals() || !TestBinaryExports()) {
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
