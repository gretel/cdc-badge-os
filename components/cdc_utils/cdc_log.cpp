#include "cdc_log.h"
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#if CONFIG_TINYUSB_CDC_ENABLED
#include "tusb.h"
#endif

// BLE UART callbacks (registered via console_register_ble_uart)
static ble_uart_send_func_t s_ble_uart_send = nullptr;
static ble_uart_ready_func_t s_ble_uart_ready = nullptr;
static ble_uart_getchar_func_t s_ble_uart_getchar = nullptr;

static log_level_t s_log_level = LOG_LEVEL_DEBUG;
#if CONFIG_TINYUSB_CDC_ENABLED
static log_backend_t s_log_backend = LOG_BACKEND_CDC;  // Default to CDC if available
#else
static log_backend_t s_log_backend = LOG_BACKEND_PRINTF;  // Fallback to printf
#endif
static bool s_console_initialized = false;

// Line length for formatted log lines (used regardless of ring buffer setting)
#define LOG_RING_LINE_LEN 100

#if CDC_LOG_RING_BUFFER
// Ring buffer for recent log lines
#define LOG_RING_LINES 100
static char s_log_ring[LOG_RING_LINES][LOG_RING_LINE_LEN];
static size_t s_log_ring_head = 0;
static size_t s_log_ring_count = 0;
#endif

// Track which input source was last used for proper echo routing
typedef enum {
    INPUT_SOURCE_NONE,
    INPUT_SOURCE_CDC,
    INPUT_SOURCE_UART,
    INPUT_SOURCE_BLE
} input_source_t;
static input_source_t s_last_input_source = INPUT_SOURCE_NONE;

// Level prefixes
static const char* level_str[] = {
    "",      // NONE
    "E",     // ERROR
    "W",     // WARN
    "I",     // INFO
    "D",     // DEBUG
    "V"      // VERBOSE
};

#if CDC_LOG_RING_BUFFER
static void log_ring_add(const char *line) {
    if (!line) return;
    size_t idx = s_log_ring_head;
    strncpy(s_log_ring[idx], line, LOG_RING_LINE_LEN - 1);
    s_log_ring[idx][LOG_RING_LINE_LEN - 1] = '\0';
    s_log_ring_head = (s_log_ring_head + 1) % LOG_RING_LINES;
    if (s_log_ring_count < LOG_RING_LINES) {
        s_log_ring_count++;
    }
}
#endif

// ============================================================================
// Error Log (WARNING and ERROR only)
// Uses dynamically allocated linked list to avoid pre-allocation
// ============================================================================
#if CDC_ERROR_LOG

typedef struct error_log_node {
    error_log_entry_t entry;
    struct error_log_node* next;
} error_log_node_t;

static error_log_node_t* s_error_log_head = nullptr;
static error_log_node_t* s_error_log_tail = nullptr;
static size_t s_error_log_count = 0;

static void error_log_add(log_level_t level, const char* message) {
    if (level != LOG_LEVEL_ERROR && level != LOG_LEVEL_WARN) return;
    if (!message) return;

    // Allocate new node in PSRAM (with DRAM fallback)
    error_log_node_t* node = (error_log_node_t*)heap_caps_malloc(
        sizeof(error_log_node_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!node) {
        // Fallback to DRAM if PSRAM unavailable
        node = (error_log_node_t*)malloc(sizeof(error_log_node_t));
    }
    if (!node) return;  // Out of memory

    // Fill entry
    node->entry.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    node->entry.level = level;
    strncpy(node->entry.message, message, ERROR_LOG_LINE_LEN - 1);
    node->entry.message[ERROR_LOG_LINE_LEN - 1] = '\0';
    node->next = nullptr;

    // Add to tail
    if (s_error_log_tail) {
        s_error_log_tail->next = node;
        s_error_log_tail = node;
    } else {
        s_error_log_head = node;
        s_error_log_tail = node;
    }
    s_error_log_count++;

    // Remove oldest if over limit
    while (s_error_log_count > ERROR_LOG_MAX_ENTRIES && s_error_log_head) {
        error_log_node_t* old = s_error_log_head;
        s_error_log_head = old->next;
        if (!s_error_log_head) {
            s_error_log_tail = nullptr;
        }
        heap_caps_free(old);
        s_error_log_count--;
    }
}

#endif // CDC_ERROR_LOG

size_t error_log_get_entries(error_log_entry_t* entries, size_t max_entries) {
#if CDC_ERROR_LOG
    if (!entries || max_entries == 0) return 0;

    size_t count = 0;
    error_log_node_t* node = s_error_log_head;
    while (node && count < max_entries) {
        entries[count++] = node->entry;
        node = node->next;
    }
    return count;
#else
    (void)entries; (void)max_entries;
    return 0;
#endif
}

size_t error_log_get_count(void) {
#if CDC_ERROR_LOG
    return s_error_log_count;
#else
    return 0;
#endif
}

void error_log_clear(void) {
#if CDC_ERROR_LOG
    while (s_error_log_head) {
        error_log_node_t* old = s_error_log_head;
        s_error_log_head = old->next;
        heap_caps_free(old);
    }
    s_error_log_tail = nullptr;
    s_error_log_count = 0;
#endif
}

void error_log_dump(void) {
#if CDC_ERROR_LOG
    if (s_error_log_count == 0) {
        console_printf("Error log: (empty)\r\n");
        return;
    }

    console_printf("Error log (%zu entries):\r\n", s_error_log_count);
    error_log_node_t* node = s_error_log_head;
    while (node) {
        uint32_t secs = node->entry.timestamp_ms / 1000;
        uint32_t mins = secs / 60;
        uint32_t hours = mins / 60;
        console_printf("[%02lu:%02lu:%02lu][%s] %s\r\n",
                       hours % 24, mins % 60, secs % 60,
                       node->entry.level == LOG_LEVEL_ERROR ? "E" : "W",
                       node->entry.message);
        node = node->next;
    }
#else
    console_printf("Error log disabled (CDC_ERROR_LOG=0)\r\n");
#endif
}

void log_init(void) {
    s_log_level = LOG_LEVEL_DEBUG;
    s_log_backend = LOG_BACKEND_PRINTF;
}

void log_set_level(log_level_t level) {
    s_log_level = level;
}

void log_set_backend(log_backend_t backend) {
    s_log_backend = backend;
}

log_level_t log_get_level(void) {
    return s_log_level;
}

log_backend_t log_get_backend(void) {
    return s_log_backend;
}

void log_write_v(log_level_t level, const char* tag, const char* fmt, va_list args) {
    // Always capture ERROR/WARN to error log, even if suppressed
    bool capture_error = (level == LOG_LEVEL_ERROR || level == LOG_LEVEL_WARN);

    if (level > s_log_level || s_log_backend == LOG_BACKEND_NONE) {
        if (!capture_error) return;
    }

    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= (int)sizeof(buf)) {
        buf[sizeof(buf) - 1] = '\0';
    }

    char line[LOG_RING_LINE_LEN];
    int header_len = snprintf(line, sizeof(line), "[%s][%s] ",
                              level_str[level], tag ? tag : "???");
    size_t used = 0;
    if (header_len > 0) {
        used = (header_len >= (int)sizeof(line)) ? (sizeof(line) - 1) : (size_t)header_len;
    }
    if (used < sizeof(line) - 1) {
        size_t remaining = (sizeof(line) - 1) - used;
        strncpy(line + used, buf, remaining);
        line[used + remaining] = '\0';
    } else {
        line[sizeof(line) - 1] = '\0';
    }

#if CDC_ERROR_LOG
    // Capture ERROR and WARNING to error log
    if (capture_error) {
        error_log_add(level, line);
    }
#endif

#if CDC_LOG_RING_BUFFER
    log_ring_add(line);
#endif

    // Only output if not suppressed
    if (level <= s_log_level && s_log_backend != LOG_BACKEND_NONE) {
        console_printf("%s\n", line);
    }
}

void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_v(level, tag, fmt, args);
    va_end(args);
}

void log_raw(const char* fmt, ...) {
    if (s_log_backend == LOG_BACKEND_NONE) {
        return;
    }
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        if (len >= (int)sizeof(buf)) {
            buf[sizeof(buf) - 1] = '\0';
        }
        console_print(buf);
    }
}

void log_flush(void) {
    if (s_log_backend == LOG_BACKEND_NONE) {
        return;
    }
    console_flush();
}

void log_dump_recent(size_t lines) {
#if CDC_LOG_RING_BUFFER
    if (s_log_ring_count == 0) {
        console_printf("(no logs buffered)\r\n");
        return;
    }

    if (lines == 0 || lines > s_log_ring_count) {
        lines = s_log_ring_count;
    }

    size_t start = (s_log_ring_head + LOG_RING_LINES - s_log_ring_count) % LOG_RING_LINES;
    size_t skip = s_log_ring_count - lines;
    start = (start + skip) % LOG_RING_LINES;

    for (size_t i = 0; i < lines; i++) {
        size_t idx = (start + i) % LOG_RING_LINES;
        console_printf("%s\r\n", s_log_ring[idx]);
    }
#else
    (void)lines;
    console_printf("Log ring buffer disabled (FEATURE_LOG_RING_BUFFER=0)\r\n");
#endif
}

void log_hex(const char* tag, const char* label, const uint8_t* data, size_t len) {
    if (s_log_backend == LOG_BACKEND_NONE) {
        return;
    }

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
    console_flush();
}

// ============================================================================
// Console I/O Implementation
// Uses TinyUSB CDC if available, otherwise falls back to printf/stdout
// ============================================================================

void console_init(void) {
    if (s_console_initialized) return;

    // Set stdin to non-blocking for UART/JTAG fallback
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    s_console_initialized = true;
#if CONFIG_TINYUSB_CDC_ENABLED
    s_log_backend = LOG_BACKEND_CDC;
#else
    s_log_backend = LOG_BACKEND_PRINTF;
#endif
}

bool console_available(void) {
    if (!s_console_initialized) return false;

#if CONFIG_TINYUSB_CDC_ENABLED
    // Try TinyUSB CDC first
    if (tud_cdc_connected() && tud_cdc_available() > 0) {
        return true;
    }
#endif

    // Fallback: check UART/JTAG via stdin (non-blocking)
    // Note: fgetc is blocking on ESP-IDF, so we use a different approach
    // For JTAG USB-Serial, data comes through stdin which uses UART driver
    return false;  // UART non-blocking check not easily available
}

int console_getchar(void) {
    if (!s_console_initialized) return -1;

#if CONFIG_TINYUSB_CDC_ENABLED
    // Try TinyUSB CDC first if connected
    if (tud_cdc_connected() && tud_cdc_available()) {
        s_last_input_source = INPUT_SOURCE_CDC;
        return tud_cdc_read_char();
    }
#endif

    // Try BLE UART if registered
    if (s_ble_uart_getchar) {
        int ble_char = s_ble_uart_getchar();
        if (ble_char >= 0) {
            s_last_input_source = INPUT_SOURCE_BLE;
            return ble_char;
        }
    }

    // Fallback: read from UART/JTAG via stdin (non-blocking)
    // This uses the ESP-IDF VFS layer which routes to the appropriate driver
    int c = getchar();
    // getchar returns EOF (-1) when no data or error
    if (c != EOF) {
        s_last_input_source = INPUT_SOURCE_UART;
        return c;
    }
    return -1;
}

void console_print(const char* str) {
    if (!str) return;
    size_t len = strlen(str);

    // Always send to BLE UART if connected (parallel output)
    if (s_ble_uart_ready && s_ble_uart_send && s_ble_uart_ready()) {
        s_ble_uart_send((const uint8_t*)str, len);
    }

#if CONFIG_TINYUSB_CDC_ENABLED
    // Always try CDC first if connected (for logs and responses)
    if (s_console_initialized && tud_cdc_connected()) {
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
    } else {
        // Fallback to JTAG/UART via printf
        printf("%s", str);
        fflush(stdout);
    }
#else
    printf("%s", str);
#endif
}

void console_printf(const char* fmt, ...) {
    if (!fmt) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len >= (int)sizeof(buf)) {
            // Truncated, but print what we have
            buf[sizeof(buf) - 1] = '\0';
        }
        console_print(buf);
    }
}

void console_putchar(char c) {
#if CONFIG_TINYUSB_CDC_ENABLED
    // Always try CDC first if connected
    if (s_console_initialized && tud_cdc_connected()) {
        tud_cdc_write_char(c);
        if (c == '\n') {
            tud_cdc_write_flush();
        }
    } else {
        putchar(c);
        fflush(stdout);  // Ensure immediate output for JTAG/UART
    }
#else
    putchar(c);
    fflush(stdout);  // Ensure immediate output
#endif
}

void console_flush(void) {
#if CONFIG_TINYUSB_CDC_ENABLED
    if (!s_console_initialized || !tud_cdc_connected()) {
        fflush(stdout);
        return;
    }
    tud_cdc_write_flush();
#else
    fflush(stdout);
#endif
}

// ============================================================================
// BLE UART Integration
// ============================================================================

void console_register_ble_uart(ble_uart_send_func_t send_func,
                               ble_uart_ready_func_t ready_func,
                               ble_uart_getchar_func_t getchar_func) {
    s_ble_uart_send = send_func;
    s_ble_uart_ready = ready_func;
    s_ble_uart_getchar = getchar_func;
}
