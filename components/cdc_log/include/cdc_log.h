/**
 * \file
 * \brief CDC Log: logging over TinyUSB CDC and UART.
 *
 * Logging system that outputs to both USB CDC and UART.
 * Includes error log (PSRAM-backed ring buffer for ERROR/WARN).
 */
#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __DOXYGEN__
namespace cdc::log {
#endif

// Log levels (prefixed with CDC_ to avoid NimBLE conflicts)
typedef enum {
    CDC_LOG_LEVEL_NONE = 0,
    CDC_LOG_LEVEL_ERROR,
    CDC_LOG_LEVEL_WARN,
    CDC_LOG_LEVEL_INFO,
    CDC_LOG_LEVEL_DEBUG,
    CDC_LOG_LEVEL_VERBOSE
} log_level_t;

// ============================================================================
// Error Log (WARNING and ERROR messages, PSRAM-backed stack)
// ============================================================================
#define ERROR_LOG_MAX_ENTRIES 50
#define ERROR_LOG_LINE_LEN    100

typedef struct {
    uint32_t timestamp_ms;
    log_level_t level;
    char message[ERROR_LOG_LINE_LEN];
} error_log_entry_t;

#ifdef __DOXYGEN__
} // namespace cdc::log
#endif

// Get error log entries (returns count, fills entries array)
size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries);
size_t error_log_get_count(void);
void error_log_clear(void);
void error_log_dump(void);  // Dump to console

// ============================================================================
// Logging API
// ============================================================================

// Initialize logging system (call after USB CDC is ready)
void log_init(void);

// Set the log level (messages below this level are suppressed)
void log_set_level(log_level_t level);
log_level_t log_get_level(void);

// Core logging function
void log_write(log_level_t level, const char* tag, const char* fmt, ...);

// Raw output (no level check, no prefix)
void log_raw(const char* fmt, ...);

// Hex dump
void log_hex(const char* tag, const char* label, const uint8_t* data, size_t len);

// ============================================================================
// Console I/O (for serial command interface)
// ============================================================================

void console_init(void);
bool console_available(void);
int console_getchar(void);
void console_print(const char* str);
void console_printf(const char* fmt, ...);
void console_putchar(char c);
void console_flush(void);

// ============================================================================
// Console Hooks (for additional I/O transports like BLE)
// ============================================================================

/**
 * \brief Output hook called for every console output.
 * \param data Output data.
 * \param len Data length.
 */
typedef void (*console_output_hook_t)(const char* data, size_t len);

/**
 * \brief Input-available hook used to check if additional input is available.
 * \return `true` if data is available.
 */
typedef bool (*console_input_available_hook_t)(void);

/**
 * \brief Input getchar hook used to fetch one character from an additional source.
 * \return Character or -1 if none available.
 */
typedef int (*console_input_getchar_hook_t)(void);

/**
 * \brief Registers the console output hook (only one supported at a time).
 * \param hook Callback function, or NULL to unregister.
 */
void console_register_output_hook(console_output_hook_t hook);

/**
 * \brief Registers the console input hooks (only one set supported at a time).
 * \param avail_hook Available-check callback, or NULL to unregister.
 * \param getchar_hook Getchar callback, or NULL to unregister.
 */
void console_register_input_hook(console_input_available_hook_t avail_hook,
                                  console_input_getchar_hook_t getchar_hook);

#ifdef __cplusplus
}
#endif

// Convenience macros
#define LOG_E(tag, fmt, ...) log_write(CDC_LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) log_write(CDC_LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) log_write(CDC_LOG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) log_write(CDC_LOG_LEVEL_DEBUG,   tag, fmt, ##__VA_ARGS__)
#define LOG_V(tag, fmt, ...) log_write(CDC_LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__)

// Short forms without tag (uses "APP")
#define LOGE(fmt, ...) LOG_E("APP", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_W("APP", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_I("APP", fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) LOG_D("APP", fmt, ##__VA_ARGS__)
#define LOGV(fmt, ...) LOG_V("APP", fmt, ##__VA_ARGS__)
