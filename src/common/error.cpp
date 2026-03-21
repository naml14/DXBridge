// src/common/error.cpp
#include "error.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>
#include <winerror.h>
#include <dxgi.h>

namespace dxb {

namespace {

const char* KnownHRESULTName(long hr) noexcept {
    switch ((HRESULT)hr) {
    case S_OK:                      return "S_OK";
    case E_FAIL:                    return "E_FAIL";
    case E_INVALIDARG:              return "E_INVALIDARG";
    case E_OUTOFMEMORY:             return "E_OUTOFMEMORY";
    case E_NOTIMPL:                 return "E_NOTIMPL";
    case E_NOINTERFACE:             return "E_NOINTERFACE";
    case E_POINTER:                 return "E_POINTER";
    case DXGI_ERROR_INVALID_CALL:   return "DXGI_ERROR_INVALID_CALL";
    case DXGI_ERROR_DEVICE_REMOVED: return "DXGI_ERROR_DEVICE_REMOVED";
    case DXGI_ERROR_DEVICE_RESET:   return "DXGI_ERROR_DEVICE_RESET";
    case DXGI_ERROR_DEVICE_HUNG:    return "DXGI_ERROR_DEVICE_HUNG";
    case DXGI_ERROR_UNSUPPORTED:    return "DXGI_ERROR_UNSUPPORTED";
    case DXGI_ERROR_WAS_STILL_DRAWING:
        return "DXGI_ERROR_WAS_STILL_DRAWING";
    default:
        return nullptr;
    }
}

void TrimTrailingWhitespace(char* text) noexcept {
    if (!text) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0) {
        const char ch = text[len - 1];
        if (ch != '\r' && ch != '\n' && ch != ' ' && ch != '\t') {
            break;
        }
        text[--len] = '\0';
    }
}

} // namespace

// Thread-local error state
thread_local char    t_error_buf[512]  = {};
thread_local long    t_last_hresult    = 0; // S_OK = 0

DXBResult MapHRESULT(long hr) noexcept {
    t_last_hresult = hr;
    if (hr >= 0)                                return DXB_OK;          // SUCCEEDED
    if (hr == (long)DXGI_ERROR_DEVICE_REMOVED ||
        hr == (long)DXGI_ERROR_DEVICE_RESET)   return DXB_E_DEVICE_LOST;
    if (hr == (long)E_OUTOFMEMORY)             return DXB_E_OUT_OF_MEMORY;
    if (hr == (long)E_INVALIDARG)              return DXB_E_INVALID_ARG;
    if (hr == (long)DXGI_ERROR_UNSUPPORTED ||
        hr == (long)E_NOTIMPL)                 return DXB_E_NOT_SUPPORTED;
    return DXB_E_INTERNAL;
}

void SetLastError(DXBResult /*code*/, const char* fmt, ...) noexcept {
    t_last_hresult = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(t_error_buf, sizeof(t_error_buf), fmt, args);
    va_end(args);
    // Always null-terminate
    t_error_buf[sizeof(t_error_buf) - 1] = '\0';
}

void SetLastErrorFromHR(long hr, const char* context) noexcept {
    t_last_hresult = hr;

    char system_message[256] = {};
    const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK;
    if (FormatMessageA(flags,
                       nullptr,
                       static_cast<DWORD>(hr),
                       0,
                       system_message,
                       static_cast<DWORD>(sizeof(system_message)),
                       nullptr) > 0) {
        TrimTrailingWhitespace(system_message);
    }

    const char* hr_name = KnownHRESULTName(hr);
    if (context) {
        if (hr_name && system_message[0]) {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "%s failed with HRESULT 0x%08X (%s): %s",
                     context,
                     (unsigned)hr,
                     hr_name,
                     system_message);
        } else if (hr_name) {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "%s failed with HRESULT 0x%08X (%s)",
                     context,
                     (unsigned)hr,
                     hr_name);
        } else if (system_message[0]) {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "%s failed with HRESULT 0x%08X: %s",
                     context,
                     (unsigned)hr,
                     system_message);
        } else {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "%s failed with HRESULT 0x%08X", context, (unsigned)hr);
        }
    } else {
        if (hr_name && system_message[0]) {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "HRESULT 0x%08X (%s): %s",
                     (unsigned)hr,
                     hr_name,
                     system_message);
        } else if (hr_name) {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "HRESULT 0x%08X (%s)",
                     (unsigned)hr,
                     hr_name);
        } else if (system_message[0]) {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "HRESULT 0x%08X: %s",
                     (unsigned)hr,
                     system_message);
        } else {
            snprintf(t_error_buf, sizeof(t_error_buf),
                     "HRESULT 0x%08X", (unsigned)hr);
        }
    }
    t_error_buf[sizeof(t_error_buf) - 1] = '\0';
}

void ClearLastError() noexcept {
    t_error_buf[0]  = '\0';
    t_last_hresult  = 0;
}

// ---------------------------------------------------------------------------
// Internal accessors — called from dxbridge.cpp (which has DXBRIDGE_EXPORTS)
// These are in the same namespace block opened above.
// ---------------------------------------------------------------------------

void GetLastErrorInternal(char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return;
    int len  = (int)strlen(t_error_buf);
    int copy = (len < buf_size - 1) ? len : buf_size - 1;
    memcpy(buf, t_error_buf, (size_t)copy);
    buf[copy] = '\0';
}

uint32_t GetLastHRESULTInternal() {
    return (uint32_t)t_last_hresult;
}

} // namespace dxb
