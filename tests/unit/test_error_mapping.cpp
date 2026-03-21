// tests/unit/test_error_mapping.cpp
// Unit tests for MapHRESULT and error string functions.
// Returns 0 on all pass, 1 on any failure.
#include <windows.h>
#include <winerror.h>
#include <dxgi.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include "../../src/common/error.hpp"

// ---------------------------------------------------------------------------
// Simple test harness
// ---------------------------------------------------------------------------
static int g_tests_run    = 0;
static int g_tests_failed = 0;

#define RUN(name)  do { ++g_tests_run; printf("  [RUN] " #name "\n"); name(); } while(0)
#define EXPECT(cond) \
    do { \
        if (!(cond)) { \
            printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_tests_failed; \
        } \
    } while(0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_S_OK_maps_to_DXB_OK() {
    DXBResult r = dxb::MapHRESULT(S_OK);
    EXPECT(r == DXB_OK);
}

static void test_E_OUTOFMEMORY_maps_to_out_of_memory() {
    DXBResult r = dxb::MapHRESULT(E_OUTOFMEMORY);
    EXPECT(r == DXB_E_OUT_OF_MEMORY);
}

static void test_device_removed_maps_to_device_lost() {
    DXBResult r = dxb::MapHRESULT(DXGI_ERROR_DEVICE_REMOVED);
    EXPECT(r == DXB_E_DEVICE_LOST);
}

static void test_device_reset_maps_to_device_lost() {
    DXBResult r = dxb::MapHRESULT(DXGI_ERROR_DEVICE_RESET);
    EXPECT(r == DXB_E_DEVICE_LOST);
}

static void test_E_INVALIDARG_maps_to_invalid_arg() {
    DXBResult r = dxb::MapHRESULT(E_INVALIDARG);
    EXPECT(r == DXB_E_INVALID_ARG);
}

static void test_E_NOTIMPL_maps_to_not_supported() {
    DXBResult r = dxb::MapHRESULT(E_NOTIMPL);
    EXPECT(r == DXB_E_NOT_SUPPORTED);
}

static void test_DXGI_unsupported_maps_to_not_supported() {
    DXBResult r = dxb::MapHRESULT(DXGI_ERROR_UNSUPPORTED);
    EXPECT(r == DXB_E_NOT_SUPPORTED);
}

static void test_unknown_hr_maps_to_internal() {
    // Use a made-up negative HRESULT
    DXBResult r = dxb::MapHRESULT(0x88880001L);
    EXPECT(r == DXB_E_INTERNAL);
}

static void test_SetLastError_GetLastError() {
    dxb::SetLastError(DXB_E_INTERNAL, "test error %d", 42);
    char buf[256] = {};
    dxb::GetLastErrorInternal(buf, sizeof(buf));
    EXPECT(strstr(buf, "test error 42") != nullptr);
}

static void test_GetLastError_truncates_safely() {
    dxb::SetLastError(DXB_E_INTERNAL, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    char buf[8] = {};
    dxb::GetLastErrorInternal(buf, 8);
    EXPECT(buf[7] == '\0');  // must be null-terminated
    // Content should be first 7 chars
    EXPECT(buf[0] == 'A');
    EXPECT(strlen(buf) <= 7);
}

static void test_MapHRESULT_stores_last_hresult() {
    dxb::MapHRESULT(E_OUTOFMEMORY);
    uint32_t hr = dxb::GetLastHRESULTInternal();
    EXPECT(hr == (uint32_t)E_OUTOFMEMORY);
}

static void test_SetLastErrorFromHR_includes_hresult_name() {
    dxb::SetLastErrorFromHR(E_INVALIDARG, "CreateThing");
    char buf[256] = {};
    dxb::GetLastErrorInternal(buf, sizeof(buf));
    EXPECT(strstr(buf, "CreateThing failed with HRESULT 0x80070057") != nullptr);
    EXPECT(strstr(buf, "E_INVALIDARG") != nullptr);
}

static void test_SetLastError_clears_last_hresult() {
    dxb::SetLastErrorFromHR(E_OUTOFMEMORY, "CreateThing");
    dxb::SetLastError(DXB_E_INTERNAL, "plain text error");
    EXPECT(dxb::GetLastHRESULTInternal() == 0);
}

static void test_thread_isolation() {
    // Two threads set different errors; each reads its own
    std::atomic<bool> t1_ok{false}, t2_ok{false};

    std::thread t1([&]() {
        dxb::SetLastError(DXB_E_INTERNAL, "thread1_error");
        // Small delay to let t2 set its error
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        char buf[256] = {};
        dxb::GetLastErrorInternal(buf, sizeof(buf));
        t1_ok = (strstr(buf, "thread1_error") != nullptr);
    });

    std::thread t2([&]() {
        dxb::SetLastError(DXB_E_INTERNAL, "thread2_error");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        char buf[256] = {};
        dxb::GetLastErrorInternal(buf, sizeof(buf));
        t2_ok = (strstr(buf, "thread2_error") != nullptr);
    });

    t1.join();
    t2.join();

    EXPECT(t1_ok.load());
    EXPECT(t2_ok.load());
}

static void test_all_error_codes_are_negative_except_ok() {
    // Use volatile to prevent C4127 "constant conditional expression" warning
    volatile DXBResult ok_val   = DXB_OK;
    volatile DXBResult ih_val   = DXB_E_INVALID_HANDLE;
    volatile DXBResult dl_val   = DXB_E_DEVICE_LOST;
    volatile DXBResult oom_val  = DXB_E_OUT_OF_MEMORY;
    volatile DXBResult ia_val   = DXB_E_INVALID_ARG;
    volatile DXBResult ns_val   = DXB_E_NOT_SUPPORTED;
    volatile DXBResult sc_val   = DXB_E_SHADER_COMPILE;
    volatile DXBResult ni_val   = DXB_E_NOT_INITIALIZED;
    volatile DXBResult is_val   = DXB_E_INVALID_STATE;
    volatile DXBResult int_val  = DXB_E_INTERNAL;
    EXPECT(ok_val  == 0);
    EXPECT(ih_val  < 0);
    EXPECT(dl_val  < 0);
    EXPECT(oom_val < 0);
    EXPECT(ia_val  < 0);
    EXPECT(ns_val  < 0);
    EXPECT(sc_val  < 0);
    EXPECT(ni_val  < 0);
    EXPECT(is_val  < 0);
    EXPECT(int_val < 0);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    printf("=== test_error_mapping ===\n");
    RUN(test_S_OK_maps_to_DXB_OK);
    RUN(test_E_OUTOFMEMORY_maps_to_out_of_memory);
    RUN(test_device_removed_maps_to_device_lost);
    RUN(test_device_reset_maps_to_device_lost);
    RUN(test_E_INVALIDARG_maps_to_invalid_arg);
    RUN(test_E_NOTIMPL_maps_to_not_supported);
    RUN(test_DXGI_unsupported_maps_to_not_supported);
    RUN(test_unknown_hr_maps_to_internal);
    RUN(test_SetLastError_GetLastError);
    RUN(test_GetLastError_truncates_safely);
    RUN(test_MapHRESULT_stores_last_hresult);
    RUN(test_SetLastErrorFromHR_includes_hresult_name);
    RUN(test_SetLastError_clears_last_hresult);
    RUN(test_thread_isolation);
    RUN(test_all_error_codes_are_negative_except_ok);

    printf("\n%d/%d tests passed\n", g_tests_run - g_tests_failed, g_tests_run);
    return (g_tests_failed > 0) ? 1 : 0;
}
