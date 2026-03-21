// tests/unit/test_handle_table.cpp
// Unit tests for HandlePool — no GPU required.
// Returns 0 on all pass, 1 on any failure.
#include <cstdio>
#include <cstring>
#include <cassert>
#include "../../src/common/handle_table.hpp"

// ---------------------------------------------------------------------------
// Simple test harness
// ---------------------------------------------------------------------------
static int g_tests_run    = 0;
static int g_tests_failed = 0;

#define TEST(name) static void name()
#define RUN(name)  do { ++g_tests_run; printf("  [RUN] " #name "\n"); name(); } while(0)
#define EXPECT(cond) \
    do { \
        if (!(cond)) { \
            printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_tests_failed; \
        } \
    } while(0)

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------
struct DummyDevice  { int id; };
struct DummyBuffer  { float val; };

using DevicePool = dxb::HandlePool<DummyDevice>;
using BufferPool = dxb::HandlePool<DummyBuffer>;

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(test_alloc_returns_nonzero_handle) {
    DevicePool pool;
    DummyDevice dev{42};
    uint64_t h = pool.Alloc(&dev, dxb::ObjectType::Device);
    EXPECT(h != 0);
    EXPECT(dxb::HandleType(h) == dxb::ObjectType::Device);
}

TEST(test_get_returns_valid_pointer) {
    DevicePool pool;
    DummyDevice dev{99};
    uint64_t h = pool.Alloc(&dev, dxb::ObjectType::Device);
    DummyDevice* p = pool.Get(h, dxb::ObjectType::Device);
    EXPECT(p != nullptr);
    EXPECT(p->id == 99);
}

TEST(test_free_then_get_returns_null) {
    DevicePool pool;
    DummyDevice dev{7};
    uint64_t h = pool.Alloc(&dev, dxb::ObjectType::Device);
    bool freed = pool.Free(h, dxb::ObjectType::Device);
    EXPECT(freed == true);
    DummyDevice* p = pool.Get(h, dxb::ObjectType::Device);
    EXPECT(p == nullptr);  // stale handle
}

TEST(test_double_free_returns_false) {
    DevicePool pool;
    DummyDevice dev{3};
    uint64_t h = pool.Alloc(&dev, dxb::ObjectType::Device);
    bool first  = pool.Free(h, dxb::ObjectType::Device);
    bool second = pool.Free(h, dxb::ObjectType::Device);
    EXPECT(first  == true);
    EXPECT(second == false);  // stale — returns false, no crash
}

TEST(test_type_mismatch_returns_null) {
    DevicePool pool;
    DummyDevice dev{5};
    uint64_t h = pool.Alloc(&dev, dxb::ObjectType::Device);
    // Cast the handle to a different type pool (same raw handle, wrong expected type)
    // We can just call Get with a different expected_type directly
    DummyDevice* p = pool.Get(h, dxb::ObjectType::SwapChain); // wrong type
    EXPECT(p == nullptr);
}

TEST(test_null_handle_returns_null) {
    DevicePool pool;
    DummyDevice* p = pool.Get(0, dxb::ObjectType::Device);
    EXPECT(p == nullptr);
}

TEST(test_handle_encode_decode_roundtrip) {
    using namespace dxb;
    ObjectType type = ObjectType::Buffer;
    uint8_t    ver  = 0x01;
    uint16_t   gen  = 0xABCD;
    uint32_t   idx  = 0x12345678;
    uint64_t   h    = MakeHandle(type, ver, gen, idx);

    EXPECT(HandleType(h)       == type);
    EXPECT(HandleVersion(h)    == ver);
    EXPECT(HandleGeneration(h) == gen);
    EXPECT(HandleIndex(h)      == idx);
}

TEST(test_reuse_slot_after_free) {
    DevicePool pool;
    DummyDevice dev1{1};
    DummyDevice dev2{2};
    uint64_t h1 = pool.Alloc(&dev1, dxb::ObjectType::Device);
    pool.Free(h1, dxb::ObjectType::Device);
    uint64_t h2 = pool.Alloc(&dev2, dxb::ObjectType::Device);
    // h2 reuses the same index but with a new generation
    EXPECT(dxb::HandleIndex(h1) == dxb::HandleIndex(h2));
    EXPECT(dxb::HandleGeneration(h1) != dxb::HandleGeneration(h2));
    // h1 is now stale
    EXPECT(pool.Get(h1, dxb::ObjectType::Device) == nullptr);
    // h2 is valid
    DummyDevice* p = pool.Get(h2, dxb::ObjectType::Device);
    EXPECT(p != nullptr && p->id == 2);
}

TEST(test_multiple_alloc_independent) {
    DevicePool pool;
    DummyDevice a{10}, b{20}, c{30};
    uint64_t ha = pool.Alloc(&a, dxb::ObjectType::Device);
    uint64_t hb = pool.Alloc(&b, dxb::ObjectType::Device);
    uint64_t hc = pool.Alloc(&c, dxb::ObjectType::Device);
    EXPECT(ha != hb && hb != hc && ha != hc);
    EXPECT(pool.Get(ha, dxb::ObjectType::Device)->id == 10);
    EXPECT(pool.Get(hb, dxb::ObjectType::Device)->id == 20);
    EXPECT(pool.Get(hc, dxb::ObjectType::Device)->id == 30);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    printf("=== test_handle_table ===\n");
    RUN(test_alloc_returns_nonzero_handle);
    RUN(test_get_returns_valid_pointer);
    RUN(test_free_then_get_returns_null);
    RUN(test_double_free_returns_false);
    RUN(test_type_mismatch_returns_null);
    RUN(test_null_handle_returns_null);
    RUN(test_handle_encode_decode_roundtrip);
    RUN(test_reuse_slot_after_free);
    RUN(test_multiple_alloc_independent);

    printf("\n%d/%d tests passed\n", g_tests_run - g_tests_failed, g_tests_run);
    return (g_tests_failed > 0) ? 1 : 0;
}
