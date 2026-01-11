#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

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

// Log ring buffer (stores recent log lines for LOG_TAIL)
#ifndef CDC_LOG_RING_BUFFER
#define CDC_LOG_RING_BUFFER 0
#endif

// ============================================================================
// Error Log (stores only WARNING and ERROR messages)
// Uses dynamically allocated memory (no pre-allocation required)
// ============================================================================
#ifndef CDC_ERROR_LOG
#define CDC_ERROR_LOG 1
#endif

#define ERROR_LOG_MAX_ENTRIES 50
#define ERROR_LOG_LINE_LEN    100

// Error log entry structure
typedef struct {
    uint32_t timestamp_ms;      // Uptime in ms when logged
    log_level_t level;          // ERROR or WARN
    char message[ERROR_LOG_LINE_LEN];
} error_log_entry_t;

// Get error log entries
// Returns number of entries, fills entries array (caller provides array)
size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries);

// Get error log count
size_t error_log_get_count(void);

// Clear error log
void error_log_clear(void);

// Dump error log to console
void error_log_dump(void);

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

// Dump last N log lines (max stored lines)
void log_dump_recent(size_t lines);

// ============================================================================
// BLE UART Integration (callback-based to avoid circular dependency)
// ============================================================================

// Callback type for BLE UART output
typedef size_t (*ble_uart_send_func_t)(const uint8_t *data, size_t len);
typedef bool (*ble_uart_ready_func_t)(void);
typedef int (*ble_uart_getchar_func_t)(void);

// Register BLE UART callbacks (called by ble_uart.cpp during init)
void console_register_ble_uart(ble_uart_send_func_t send_func,
                               ble_uart_ready_func_t ready_func,
                               ble_uart_getchar_func_t getchar_func);

// ============================================================================
// Console I/O (for serial command interface)
// These functions route through TinyUSB CDC when backend is LOG_BACKEND_CDC
// ============================================================================

// Initialize console (call after USB is ready)
void console_init(void);

// Check if console input is available
bool console_available(void);

// Read single character from console (non-blocking, returns -1 if none)
int console_getchar(void);

// Write string to console
void console_print(const char* str);

// Write formatted string to console
void console_printf(const char* fmt, ...);

// Write single character to console
void console_putchar(char c);

// Flush console output
void console_flush(void);

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
