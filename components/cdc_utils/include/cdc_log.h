#pragma once

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;

// Log output backend
typedef enum {
    LOG_BACKEND_PRINTF = 0,  // printf() - works with USB_SERIAL_JTAG
    LOG_BACKEND_CDC,          // TinyUSB CDC (requires USBController::init())
    LOG_BACKEND_NONE          // No output
} log_backend_t;

// Initialize logging system
void log_init(void);

// Set the log level (messages below this level are suppressed)
void log_set_level(log_level_t level);

// Set the output backend
void log_set_backend(log_backend_t backend);

// Get current settings
log_level_t log_get_level(void);
log_backend_t log_get_backend(void);

// Core logging function
void log_write(log_level_t level, const char* tag, const char* fmt, ...);
void log_write_v(log_level_t level, const char* tag, const char* fmt, va_list args);

// Raw output (no level check, no prefix)
void log_raw(const char* fmt, ...);

// Hex dump
void log_hex(const char* tag, const char* label, const uint8_t* data, size_t len);

// Flush output buffer
void log_flush(void);

#ifdef __cplusplus
}
#endif

// Convenience macros
#define LOG_E(tag, fmt, ...) log_write(LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) log_write(LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) log_write(LOG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) log_write(LOG_LEVEL_DEBUG,   tag, fmt, ##__VA_ARGS__)
#define LOG_V(tag, fmt, ...) log_write(LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__)

// Short forms without tag (uses "APP")
#define LOGE(fmt, ...) LOG_E("APP", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_W("APP", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_I("APP", fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) LOG_D("APP", fmt, ##__VA_ARGS__)
#define LOGV(fmt, ...) LOG_V("APP", fmt, ##__VA_ARGS__)
