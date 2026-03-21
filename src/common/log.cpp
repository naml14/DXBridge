// src/common/log.cpp
#include "log.hpp"
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace dxb {

// Global log callback state (protected by a mutex)
static std::mutex         g_log_mutex;
static DXBLogCallback     g_log_callback  = nullptr;
static void*              g_log_userdata  = nullptr;

void LogMessage(int level, const char* fmt, ...) noexcept {
    DXBLogCallback cb;
    void* ud;
    {
        std::lock_guard<std::mutex> lk(g_log_mutex);
        cb = g_log_callback;
        ud = g_log_userdata;
    }
    if (!cb) return;  // no callback registered — silent

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';

    cb(level, buf, ud);
}

// Internal accessor — the exported wrapper lives in dxbridge.cpp
void SetLogCallbackInternal(DXBLogCallback cb, void* user_data) noexcept {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    g_log_callback = cb;
    g_log_userdata = user_data;
}

} // namespace dxb
