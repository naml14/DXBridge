// src/common/log.hpp
#pragma once
#include "../../include/dxbridge/dxbridge.h"

namespace dxb {

// Internal log levels
enum LogLevel {
    LOG_INFO  = 0,
    LOG_WARN  = 1,
    LOG_ERROR = 2,
};

// Internal log dispatch (calls the registered callback if set)
void LogMessage(int level, const char* fmt, ...) noexcept;

// Internal accessor — the exported wrapper lives in dxbridge.cpp
void SetLogCallbackInternal(DXBLogCallback cb, void* user_data) noexcept;

} // namespace dxb

// Internal logging macros
#define DXB_LOG_INFO(fmt, ...)  dxb::LogMessage(dxb::LOG_INFO,  fmt, ##__VA_ARGS__)
#define DXB_LOG_WARN(fmt, ...)  dxb::LogMessage(dxb::LOG_WARN,  fmt, ##__VA_ARGS__)
#define DXB_LOG_ERROR(fmt, ...) dxb::LogMessage(dxb::LOG_ERROR, fmt, ##__VA_ARGS__)
