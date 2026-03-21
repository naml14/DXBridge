// src/common/handle_table.hpp
// Thread-safe handle pool with generation counters for use-after-free detection.
#pragma once
#include <cstdint>
#include <vector>
#include <mutex>

namespace dxb {

enum class ObjectType : uint8_t {
    None        = 0x00,
    Device      = 0x01,
    SwapChain   = 0x02,
    Buffer      = 0x03,
    Shader      = 0x04,
    Pipeline    = 0x05,
    RTV         = 0x06,
    DSV         = 0x07,
    InputLayout = 0x08,
    RenderTarget= 0x09,
};

// ---------------------------------------------------------------------------
// Handle encoding:
//   Bits [63:56]  8 bits   type tag
//   Bits [55:48]  8 bits   API version  (0x01)
//   Bits [47:32]  16 bits  generation
//   Bits [31:0]   32 bits  pool index
// ---------------------------------------------------------------------------
inline uint64_t MakeHandle(ObjectType type, uint8_t api_ver,
                            uint16_t generation, uint32_t index) noexcept {
    return (static_cast<uint64_t>(type)       << 56)
         | (static_cast<uint64_t>(api_ver)    << 48)
         | (static_cast<uint64_t>(generation) << 32)
         |  static_cast<uint64_t>(index);
}

inline ObjectType HandleType(uint64_t h)       noexcept { return static_cast<ObjectType>(h >> 56); }
inline uint8_t    HandleVersion(uint64_t h)    noexcept { return static_cast<uint8_t>((h >> 48) & 0xFF); }
inline uint16_t   HandleGeneration(uint64_t h) noexcept { return static_cast<uint16_t>((h >> 32) & 0xFFFF); }
inline uint32_t   HandleIndex(uint64_t h)      noexcept { return static_cast<uint32_t>(h & 0xFFFFFFFF); }

// Slot in the pool
template<typename T>
struct Slot {
    T*       ptr        = nullptr;  // null = free
    uint16_t generation = 0;
};

// ---------------------------------------------------------------------------
// HandlePool<T> — one pool per object type, thread-safe via std::mutex.
// ---------------------------------------------------------------------------
template<typename T>
class HandlePool {
public:
    HandlePool() = default;
    ~HandlePool() = default;

    // Alloc: store ptr, bump generation, return new handle.
    uint64_t Alloc(T* obj, ObjectType type) noexcept;

    // Get: decode handle, validate type + generation; return ptr or nullptr.
    T* Get(uint64_t handle, ObjectType expected_type) const noexcept;

    // Free: invalidate slot, push index to free list.
    // Returns false if handle is already invalid/stale.
    bool Free(uint64_t handle, ObjectType expected_type) noexcept;

private:
    mutable std::mutex    m_mutex;
    std::vector<Slot<T>>  m_slots;
    std::vector<uint32_t> m_freelist;
};

// ---------------------------------------------------------------------------
// Inline method bodies (template — defined in header)
// ---------------------------------------------------------------------------
template<typename T>
uint64_t HandlePool<T>::Alloc(T* obj, ObjectType type) noexcept {
    std::lock_guard<std::mutex> lk(m_mutex);
    uint32_t index;
    if (!m_freelist.empty()) {
        index = m_freelist.back();
        m_freelist.pop_back();
        auto& slot = m_slots[index];
        slot.generation = static_cast<uint16_t>(slot.generation + 1u);
        if (slot.generation == 0) slot.generation = 1; // skip 0
        slot.ptr = obj;
    } else {
        index = static_cast<uint32_t>(m_slots.size());
        Slot<T> s;
        s.generation = 1;
        s.ptr = obj;
        m_slots.push_back(s);
    }
    return MakeHandle(type, 0x01, m_slots[index].generation, index);
}

template<typename T>
T* HandlePool<T>::Get(uint64_t handle, ObjectType expected_type) const noexcept {
    if (handle == 0) return nullptr;
    if (HandleType(handle) != expected_type) return nullptr;
    uint32_t index = HandleIndex(handle);
    uint16_t gen   = HandleGeneration(handle);
    std::lock_guard<std::mutex> lk(m_mutex);
    if (index >= static_cast<uint32_t>(m_slots.size())) return nullptr;
    const auto& slot = m_slots[index];
    if (slot.generation != gen || slot.ptr == nullptr) return nullptr;
    return slot.ptr;
}

template<typename T>
bool HandlePool<T>::Free(uint64_t handle, ObjectType expected_type) noexcept {
    if (handle == 0) return false;
    if (HandleType(handle) != expected_type) return false;
    uint32_t index = HandleIndex(handle);
    uint16_t gen   = HandleGeneration(handle);
    std::lock_guard<std::mutex> lk(m_mutex);
    if (index >= static_cast<uint32_t>(m_slots.size())) return false;
    auto& slot = m_slots[index];
    if (slot.generation != gen || slot.ptr == nullptr) return false;
    slot.ptr = nullptr;
    m_freelist.push_back(index);
    return true;
}

} // namespace dxb
