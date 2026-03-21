// src/common/error.hpp
#pragma once
#include <cstdint>
#include "../../include/dxbridge/dxbridge.h"

namespace dxb {

// Map a Win32 HRESULT to a DXBResult and store it as last HRESULT on TLS.
DXBResult MapHRESULT(long hr) noexcept;

// Format and store an error string in the calling thread's TLS buffer.
void SetLastError(DXBResult code, const char* fmt, ...) noexcept;

// Convenience: map HRESULT + set error string with context label.
void SetLastErrorFromHR(long hr, const char* context) noexcept;

// Clear error state for current thread.
void ClearLastError() noexcept;

// Internal accessors used by dxbridge.cpp to implement the exported C funcs.
void     GetLastErrorInternal(char* buf, int buf_size);
uint32_t GetLastHRESULTInternal();

} // namespace dxb
