#include "dxbridge/dxbridge.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

std::string LastErrorText() {
    char buf[512] = {};
    DXBridge_GetLastError(buf, static_cast<int>(sizeof(buf)));
    return std::string(buf);
}

bool ExpectResult(const char* label, DXBResult actual, DXBResult expected) {
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

bool ExpectUInt32(const char* label, uint32_t actual, uint32_t expected) {
    if (actual == expected) {
        return true;
    }

    std::printf("FAIL: %s returned %u (expected %u)\n", label, actual, expected);
    return false;
}

bool ExpectNonZero(const char* label, uint32_t actual) {
    if (actual != 0u) {
        return true;
    }

    std::printf("FAIL: %s returned 0\n", label);
    return false;
}

DXBCapabilityQuery MakeQuery(DXBCapability capability, DXBBackend backend) {
    DXBCapabilityQuery query = {};
    query.struct_version = DXBRIDGE_STRUCT_VERSION;
    query.capability = capability;
    query.backend = backend;
    query.adapter_index = 0u;
    query.format = DXB_FORMAT_UNKNOWN;
    query.device = DXBRIDGE_NULL_HANDLE;
    return query;
}

DXBCapabilityInfo MakeInfo() {
    DXBCapabilityInfo info = {};
    info.struct_version = DXBRIDGE_STRUCT_VERSION;
    return info;
}

bool TestArgumentValidation() {
    DXBCapabilityQuery query = MakeQuery(DXB_CAPABILITY_BACKEND_AVAILABLE, DXB_BACKEND_DX12);
    DXBCapabilityInfo info = MakeInfo();

    bool ok = true;
    ok = ExpectResult("QueryCapability(null query)",
                      DXBridge_QueryCapability(nullptr, &info),
                      DXB_E_INVALID_ARG) && ok;
    ok = ExpectResult("QueryCapability(null out_info)",
                      DXBridge_QueryCapability(&query, nullptr),
                      DXB_E_INVALID_ARG) && ok;

    query.struct_version = 0u;
    ok = ExpectResult("QueryCapability(invalid query version)",
                      DXBridge_QueryCapability(&query, &info),
                      DXB_E_INVALID_ARG) && ok;

    query = MakeQuery(DXB_CAPABILITY_BACKEND_AVAILABLE, DXB_BACKEND_DX12);
    info.struct_version = 0u;
    ok = ExpectResult("QueryCapability(output version ignored and rewritten)",
                      DXBridge_QueryCapability(&query, &info),
                      DXB_OK) && ok;
    ok = ExpectUInt32("QueryCapability(output version rewritten)",
                      info.struct_version,
                      DXBRIDGE_STRUCT_VERSION) && ok;

    query = MakeQuery(DXB_CAPABILITY_BACKEND_AVAILABLE, DXB_BACKEND_AUTO);
    info = MakeInfo();
    ok = ExpectResult("QueryCapability(invalid backend)",
                      DXBridge_QueryCapability(&query, &info),
                      DXB_E_INVALID_ARG) && ok;

    query = MakeQuery(static_cast<DXBCapability>(999u), DXB_BACKEND_DX12);
    info = MakeInfo();
    ok = ExpectResult("QueryCapability(unknown capability)",
                      DXBridge_QueryCapability(&query, &info),
                      DXB_E_INVALID_ARG) && ok;

    return ok;
}

bool TestDx12PreInitAdapterQueries() {
    DXBridge_Shutdown();

    DXBCapabilityQuery count_query = MakeQuery(DXB_CAPABILITY_ADAPTER_COUNT, DXB_BACKEND_DX12);
    DXBCapabilityInfo count_info = MakeInfo();
    bool ok = ExpectResult("QueryCapability(DX12 adapter count)",
                           DXBridge_QueryCapability(&count_query, &count_info),
                           DXB_OK);
    ok = ExpectUInt32("DX12 adapter count supported", count_info.supported, 1u) && ok;
    ok = ExpectNonZero("DX12 adapter count value", count_info.value_u32) && ok;

    if (!ok) {
        return false;
    }

    const uint32_t warp_index = count_info.value_u32 - 1u;

    DXBCapabilityQuery software_query = MakeQuery(DXB_CAPABILITY_ADAPTER_SOFTWARE, DXB_BACKEND_DX12);
    software_query.adapter_index = warp_index;
    DXBCapabilityInfo software_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 WARP software)",
                      DXBridge_QueryCapability(&software_query, &software_info),
                      DXB_OK) && ok;
    ok = ExpectUInt32("DX12 WARP software supported", software_info.supported, 1u) && ok;
    ok = ExpectUInt32("DX12 WARP software value", software_info.value_u32, 1u) && ok;

    DXBCapabilityQuery fl_query = MakeQuery(DXB_CAPABILITY_ADAPTER_MAX_FEATURE_LEVEL, DXB_BACKEND_DX12);
    fl_query.adapter_index = warp_index;
    DXBCapabilityInfo fl_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 WARP feature level)",
                      DXBridge_QueryCapability(&fl_query, &fl_info),
                      DXB_OK) && ok;
    ok = ExpectUInt32("DX12 WARP feature level supported", fl_info.supported, 1u) && ok;
    ok = ExpectNonZero("DX12 WARP feature level value", fl_info.value_u32) && ok;

    software_query.adapter_index = count_info.value_u32;
    software_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 adapter out of range)",
                      DXBridge_QueryCapability(&software_query, &software_info),
                      DXB_OK) && ok;
    ok = ExpectUInt32("DX12 adapter out of range supported", software_info.supported, 0u) && ok;

    return ok;
}

bool TestValidationAvailabilityQueries() {
    DXBridge_Shutdown();

    DXBCapabilityQuery dx11_gpu_query = MakeQuery(DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE, DXB_BACKEND_DX11);
    DXBCapabilityInfo dx11_gpu_info = MakeInfo();
    bool ok = ExpectResult("QueryCapability(DX11 GPU validation)",
                           DXBridge_QueryCapability(&dx11_gpu_query, &dx11_gpu_info),
                           DXB_OK);
    ok = ExpectUInt32("DX11 GPU validation supported", dx11_gpu_info.supported, 0u) && ok;

    DXBCapabilityQuery dx12_debug_query = MakeQuery(DXB_CAPABILITY_DEBUG_LAYER_AVAILABLE, DXB_BACKEND_DX12);
    DXBCapabilityInfo dx12_debug_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 debug layer)",
                      DXBridge_QueryCapability(&dx12_debug_query, &dx12_debug_info),
                      DXB_OK) && ok;

    DXBCapabilityQuery dx12_gpu_query = MakeQuery(DXB_CAPABILITY_GPU_VALIDATION_AVAILABLE, DXB_BACKEND_DX12);
    DXBCapabilityInfo dx12_gpu_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 GPU validation)",
                      DXBridge_QueryCapability(&dx12_gpu_query, &dx12_gpu_info),
                      DXB_OK) && ok;

    if (dx12_gpu_info.supported != 0u) {
        ok = ExpectUInt32("DX12 GPU validation implies debug layer",
                          dx12_debug_info.supported,
                          1u) && ok;
    }

    return ok;
}

bool TestLegacySupportsFeatureRemainsActiveBackendScoped() {
    DXBridge_Shutdown();

    DXBCapabilityQuery dx11_query = MakeQuery(DXB_CAPABILITY_BACKEND_AVAILABLE, DXB_BACKEND_DX11);
    DXBCapabilityInfo dx11_info = MakeInfo();
    bool ok = ExpectResult("QueryCapability(DX11 backend available)",
                           DXBridge_QueryCapability(&dx11_query, &dx11_info),
                           DXB_OK);
    if (!ok) {
        return false;
    }
    if (dx11_info.supported == 0u) {
        std::printf("SKIP: DX11 backend unavailable on this machine\n");
        return true;
    }

    DXBCapabilityQuery dx12_query = MakeQuery(DXB_CAPABILITY_BACKEND_AVAILABLE, DXB_BACKEND_DX12);
    DXBCapabilityInfo before_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 backend available before init)",
                      DXBridge_QueryCapability(&dx12_query, &before_info),
                      DXB_OK) && ok;
    ok = ExpectUInt32("SupportsFeature(DX12) before init",
                      DXBridge_SupportsFeature(DXB_FEATURE_DX12),
                      0u) && ok;

    ok = ExpectResult("Init(DX11)", DXBridge_Init(DXB_BACKEND_DX11), DXB_OK) && ok;
    ok = ExpectUInt32("SupportsFeature(DX12) on DX11 backend",
                      DXBridge_SupportsFeature(DXB_FEATURE_DX12),
                      0u) && ok;

    DXBCapabilityInfo after_info = MakeInfo();
    ok = ExpectResult("QueryCapability(DX12 backend available after DX11 init)",
                      DXBridge_QueryCapability(&dx12_query, &after_info),
                      DXB_OK) && ok;
    ok = ExpectUInt32("DX12 backend availability stable across DX11 init",
                      after_info.supported,
                      before_info.supported) && ok;
    ok = ExpectUInt32("DX12 backend feature level stable across DX11 init",
                      after_info.value_u32,
                      before_info.value_u32) && ok;

    DXBridge_Shutdown();
    return ok;
}

} // namespace

int main() {
    std::printf("=== Capability discovery regression tests ===\n");

    bool ok = true;
    ok = TestArgumentValidation() && ok;
    ok = TestDx12PreInitAdapterQueries() && ok;
    ok = TestValidationAvailabilityQueries() && ok;
    ok = TestLegacySupportsFeatureRemainsActiveBackendScoped() && ok;

    DXBridge_Shutdown();

    if (!ok) {
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
