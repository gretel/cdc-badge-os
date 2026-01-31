#pragma once

#include <cstdint>
#include <cstdarg>

namespace cdc::serial {

/**
 * Console I/O Abstraction
 *
 * Provides printf-style output and character input for serial commands.
 * Works over USB CDC, UART, or BLE UART depending on configuration.
 */
class Console {
public:
    /**
     * Initialize console (USB CDC or UART)
     */
    static void init();

    /**
     * Print formatted string
     */
    static void printf(const char* format, ...) __attribute__((format(printf, 1, 2)));

    /**
     * Print formatted string (va_list version)
     */
    static void vprintf(const char* format, va_list args);

    /**
     * Print string without formatting
     */
    static void print(const char* str);

    /**
     * Print character
     */
    static void putchar(char c);

    /**
     * Get character (non-blocking)
     * @return Character or -1 if no input available
     */
    static int getchar();

    /**
     * Flush output buffer
     */
    static void flush();

    /**
     * Check if data is available
     */
    static bool available();

    /**
     * Show prompt ("> ")
     */
    static void showPrompt();

private:
    Console() = delete;  // Static-only class
};

// Convenience macros
#define CONSOLE_PRINTF(...) cdc::serial::Console::printf(__VA_ARGS__)
#define CONSOLE_PRINT(s) cdc::serial::Console::print(s)

} // namespace cdc::serial
