#include "cdc_log.h"
#include <string.h>

static log_level_t s_log_level = LOG_LEVEL_DEBUG;
static log_backend_t s_log_backend = LOG_BACKEND_PRINTF;

// Level prefixes
static const char* level_str[] = {
    "",      // NONE
    "E",     // ERROR
    "W",     // WARN
    "I",     // INFO
    "D",     // DEBUG
    "V"      // VERBOSE
};

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
    if (level > s_log_level || s_log_backend == LOG_BACKEND_NONE) {
        return;
    }

    // For now, always use printf (USB_SERIAL_JTAG compatible)
    // CDC backend can be added later when TinyUSB is properly initialized
    printf("[%s][%s] ", level_str[level], tag ? tag : "???");
    vprintf(fmt, args);
    printf("\n");
    log_flush();
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
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    log_flush();
}

void log_flush(void) {
    if (s_log_backend == LOG_BACKEND_NONE) {
        return;
    }
    fflush(stdout);
}

void log_hex(const char* tag, const char* label, const uint8_t* data, size_t len) {
    if (s_log_backend == LOG_BACKEND_NONE) {
        return;
    }

    printf("[D][%s] %s (%zu bytes): ", tag ? tag : "HEX", label ? label : "data", len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
        if ((i + 1) % 32 == 0 && (i + 1) < len) {
            printf("\n      ");
        } else if ((i + 1) % 4 == 0 && (i + 1) < len) {
            printf(" ");
        }
    }
    printf("\n");
    log_flush();
}
