/**
 * CDC Log Implementation
 *
 * Outputs to both TinyUSB CDC and UART.
 * Error log stores WARNING/ERROR in PSRAM for later inspection.
 */
#include "cdc_log.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "sdkconfig.h"
#include "esp_timer.h"
#include "esp_attr.h"

#if CONFIG_TINYUSB_CDC_ENABLED
#include "tusb.h"
#endif

static log_level_t s_log_level = CDC_LOG_LEVEL_DEBUG;
static bool s_initialized = false;

// Console hooks for additional I/O transports (e.g., BLE)
static console_output_hook_t s_output_hook = nullptr;
static console_input_available_hook_t s_input_avail_hook = nullptr;
static console_input_getchar_hook_t s_input_getchar_hook = nullptr;

static const char* level_str[] = {
    "",      // NONE
    "E",     // ERROR
    "W",     // WARN
    "I",     // INFO
    "D",     // DEBUG
    "V"      // VERBOSE
};

// ============================================================================
// Error Log (PSRAM-backed ring buffer)
// ============================================================================

// Static array in PSRAM - no heap allocation needed
EXT_RAM_BSS_ATTR static error_log_entry_t s_error_log[ERROR_LOG_MAX_ENTRIES];
static size_t s_error_log_head = 0;  // Next write position
static size_t s_error_log_count = 0; // Number of entries

static void error_log_add(log_level_t level, const char* message) {
    if (level != CDC_LOG_LEVEL_ERROR && level != CDC_LOG_LEVEL_WARN) return;
    if (!message) return;

    // Write to current position (overwrites oldest if full)
    error_log_entry_t* entry = &s_error_log[s_error_log_head];
    entry->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    entry->level = level;
    strncpy(entry->message, message, ERROR_LOG_LINE_LEN - 1);
    entry->message[ERROR_LOG_LINE_LEN - 1] = '\0';

    // Advance head (ring buffer)
    s_error_log_head = (s_error_log_head + 1) % ERROR_LOG_MAX_ENTRIES;
    if (s_error_log_count < ERROR_LOG_MAX_ENTRIES) {
        s_error_log_count++;
    }
}

size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries) {
    if (!entries || max_entries == 0) return 0;

    size_t count = 0;
    // Calculate start position (oldest entry)
    size_t start = (s_error_log_head + ERROR_LOG_MAX_ENTRIES - s_error_log_count) % ERROR_LOG_MAX_ENTRIES;

    for (size_t i = 0; i < s_error_log_count && count < max_entries; i++) {
        size_t idx = (start + i) % ERROR_LOG_MAX_ENTRIES;
        entries[count++] = s_error_log[idx];
    }
    return count;
}

size_t error_log_get_count(void) {
    return s_error_log_count;
}

void error_log_clear(void) {
    s_error_log_head = 0;
    s_error_log_count = 0;
}

void error_log_dump(void) {
    if (s_error_log_count == 0) {
        console_printf("Error log: (empty)\r\n");
        return;
    }

    console_printf("Error log (%zu entries):\r\n", s_error_log_count);

    size_t start = (s_error_log_head + ERROR_LOG_MAX_ENTRIES - s_error_log_count) % ERROR_LOG_MAX_ENTRIES;
    for (size_t i = 0; i < s_error_log_count; i++) {
        size_t idx = (start + i) % ERROR_LOG_MAX_ENTRIES;
        error_log_entry_t* e = &s_error_log[idx];

        uint32_t secs = e->timestamp_ms / 1000;
        uint32_t mins = secs / 60;
        uint32_t hours = mins / 60;
        console_printf("[%02lu:%02lu:%02lu][%s] %s\r\n",
                       hours % 24, mins % 60, secs % 60,
                       e->level == CDC_LOG_LEVEL_ERROR ? "E" : "W",
                       e->message);
    }
}

// ============================================================================
// Logging
// ============================================================================

void log_init(void) {
    s_log_level = CDC_LOG_LEVEL_DEBUG;
    console_init();
}

void log_set_level(log_level_t level) {
    s_log_level = level;
}

log_level_t log_get_level(void) {
    return s_log_level;
}

void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Always capture ERROR/WARN to error log
    bool capture = (level == CDC_LOG_LEVEL_ERROR || level == CDC_LOG_LEVEL_WARN);

    // Format message
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Format with prefix
    char line[300];
    snprintf(line, sizeof(line), "[%s][%s] %s",
             level_str[level], tag ? tag : "???", buf);

    // Capture to error log (before suppression check)
    if (capture) {
        error_log_add(level, line);
    }

    // Output if not suppressed
    if (level <= s_log_level) {
        console_printf("%s\n", line);
    }
}

void log_raw(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_print(buf);
}

void log_hex(const char* tag, const char* label, const uint8_t* data, size_t len) {
    console_printf("[D][%s] %s (%zu bytes): ", tag ? tag : "HEX", label ? label : "data", len);
    for (size_t i = 0; i < len; i++) {
        console_printf("%02X", data[i]);
        if ((i + 1) % 32 == 0 && (i + 1) < len) {
            console_print("\n      ");
        } else if ((i + 1) % 4 == 0 && (i + 1) < len) {
            console_putchar(' ');
        }
    }
    console_print("\n");
}

// ============================================================================
// Console I/O
// ============================================================================

void console_init(void) {
    if (s_initialized) return;

    // Set stdin to non-blocking for UART fallback
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    s_initialized = true;
}

bool console_available(void) {
    if (!s_initialized) return false;

#if CONFIG_TINYUSB_CDC_ENABLED
    if (tud_cdc_connected() && tud_cdc_available() > 0) {
        return true;
    }
#endif

    // Check input hook (e.g., BLE)
    if (s_input_avail_hook && s_input_avail_hook()) {
        return true;
    }

    return false;
}

int console_getchar(void) {
    if (!s_initialized) return -1;

#if CONFIG_TINYUSB_CDC_ENABLED
    // Priority 1: USB CDC
    if (tud_cdc_connected() && tud_cdc_available()) {
        return tud_cdc_read_char();
    }
#endif

    // Priority 2: Input hook (e.g., BLE)
    if (s_input_getchar_hook) {
        int c = s_input_getchar_hook();
        if (c >= 0) {
            return c;
        }
    }

    // Fallback: UART via stdin
    int c = getchar();
    if (c != EOF) {
        return c;
    }
    return -1;
}

void console_print(const char* str) {
    if (!str) return;
    size_t len = strlen(str);

    // Always output to UART (visible via USB-Serial-JTAG or external adapter)
    printf("%s", str);
    fflush(stdout);

#if CONFIG_TINYUSB_CDC_ENABLED
    // Also send to USB CDC if connected
    if (s_initialized && tud_cdc_connected()) {
        size_t written = 0;
        while (written < len) {
            size_t avail = tud_cdc_write_available();
            if (avail == 0) {
                tud_cdc_write_flush();
                continue;
            }
            size_t to_write = len - written;
            if (to_write > avail) to_write = avail;
            written += tud_cdc_write(str + written, to_write);
        }
        tud_cdc_write_flush();
    }
#endif

    // Also send to output hook (e.g., BLE)
    if (s_output_hook) {
        s_output_hook(str, len);
    }
}

void console_printf(const char* fmt, ...) {
    if (!fmt) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_print(buf);
}

void console_putchar(char c) {
    putchar(c);
    fflush(stdout);  // Immediate echo for serial terminal

#if CONFIG_TINYUSB_CDC_ENABLED
    if (s_initialized && tud_cdc_connected()) {
        tud_cdc_write_char(c);
        tud_cdc_write_flush();  // Immediate echo for USB CDC
    }
#endif

    // Also send to output hook (e.g., BLE)
    if (s_output_hook) {
        s_output_hook(&c, 1);
    }
}

void console_flush(void) {
#if CONFIG_TINYUSB_CDC_ENABLED
    if (s_initialized && tud_cdc_connected()) {
        tud_cdc_write_flush();
    }
#endif
    fflush(stdout);
}

// ============================================================================
// Console Hooks
// ============================================================================

void console_register_output_hook(console_output_hook_t hook) {
    s_output_hook = hook;
}

void console_register_input_hook(console_input_available_hook_t avail_hook,
                                  console_input_getchar_hook_t getchar_hook) {
    s_input_avail_hook = avail_hook;
    s_input_getchar_hook = getchar_hook;
}
