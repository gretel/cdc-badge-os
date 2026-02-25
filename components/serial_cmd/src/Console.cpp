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

/**
 * \brief Initializes console wrapper state.
 * \return void
 */
void Console::init() {
    if (s_initialized) return;
    // cdc_log console_init() is called by log_init()
    s_initialized = true;
}

/**
 * \brief Prints formatted text to console.
 * \param format Printf-style format string.
 * \return void
 */
void Console::printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/**
 * \brief Prints formatted text with explicit varargs list.
 * \param format Printf-style format string.
 * \param args Variable argument list.
 * \return void
 */
void Console::vprintf(const char* format, va_list args) {
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0) {
        print(buffer);
    }
}

/**
 * \brief Prints raw string to console.
 * \param str Null-terminated text.
 * \return void
 */
void Console::print(const char* str) {
    if (!str) return;
    console_print(str);
}

/**
 * \brief Writes a single character to console.
 * \param c Character to write.
 * \return void
 */
void Console::putchar(char c) {
    console_putchar(c);
}

/**
 * \brief Reads one character from console input.
 * \return Character code or negative value if none available.
 */
int Console::getchar() {
    return console_getchar();
}

/**
 * \brief Flushes pending console output.
 * \return void
 */
void Console::flush() {
    console_flush();
}

/**
 * \brief Checks whether console input is available.
 * \return `true` if at least one character can be read.
 */
bool Console::available() {
    return console_available();
}

/**
 * \brief Prints standard shell prompt.
 * \return void
 */
void Console::showPrompt() {
    print("> ");
    flush();
}

} // namespace cdc::serial
