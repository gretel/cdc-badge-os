/**
 * Console I/O Implementation
 * Wraps cdc_log console functions
 */

#include "serial_cmd/Console.h"
#include "cdc_log.h"
#include <cstdio>
#include <cstring>

namespace cdc::serial {

static bool s_initialized = false;

void Console::init() {
    if (s_initialized) return;
    // cdc_log console_init() is called by log_init()
    s_initialized = true;
}

void Console::printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void Console::vprintf(const char* format, va_list args) {
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0) {
        print(buffer);
    }
}

void Console::print(const char* str) {
    if (!str) return;
    console_print(str);
}

void Console::putchar(char c) {
    console_putchar(c);
}

int Console::getchar() {
    return console_getchar();
}

void Console::flush() {
    console_flush();
}

bool Console::available() {
    return console_available();
}

void Console::showPrompt() {
    print("> ");
    flush();
}

} // namespace cdc::serial
