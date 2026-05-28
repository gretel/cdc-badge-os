#pragma once

#include "ICommandRegistry.h"
#include <cstdint>

namespace cdc::serial {

/**
 * Callbacks for external notifications
 */
using TextChangeCallback = void (*)(const char* field, const char* value);
using TimeChangeCallback = void (*)();

/**
 * Serial Command Processor
 *
 * Main entry point for serial command handling. Handles:
 * - Input buffering and line editing
 * - Command dispatching via ICommandRegistry
 * - Optional authentication (FEATURE_SECURE_SERIAL)
 * - Special input modes (vCard, CSR, etc.)
 */
class SerialCmd {
public:
    static constexpr size_t CMD_BUFFER_SIZE = 256;
    static constexpr uint32_t AUTH_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes

    /**
     * Initialize serial command system
     */
    static void init();

    /**
     * Process one character from input
     * Call this regularly from main loop
     * @return true if a complete command was executed
     */
    static bool process();

    /**
     * Get command registry for module registration
     */
    static ICommandRegistry& getRegistry();

    // === Callbacks ===

    /**
     * Set callback for text changes (name, info, info2)
     */
    static void setTextCallback(TextChangeCallback callback);

    /**
     * Set callback for time changes
     */
    static void setTimeCallback(TimeChangeCallback callback);

    // === Authentication (FEATURE_SECURE_SERIAL) ===

    /**
     * Check if currently authenticated
     */
    static bool isAuthenticated();

    /**
     * Authenticate with PIN
     * @return true if PIN is correct
     */
    static bool authenticate(const char* pin);

    /**
     * Logout (clear authentication)
     */
    static void logout();

    /**
     * Refresh the authentication-timeout timestamp so a long-running serial
     * activity (binary upload, multi-line paste) can keep the session alive
     * without having to execute periodic dummy commands.
     */
    static void touchAuthSession();

    // === Built-in Commands ===

    /**
     * Register built-in system commands (HELP, PING, STATUS, etc.)
     */
    static void registerBuiltinCommands();

private:
    SerialCmd() = delete;  // Static-only class

    static void executeCommand(char* cmd);
    static char* trim(char* str);

    /**
     * \brief Direction selector for command-history navigation.
     */
    enum class HistoryDirection : uint8_t { OLDER, NEWER };

    /**
     * \brief Replaces the current input line with a history entry.
     * \param dir Navigation direction (older = arrow up, newer = arrow down).
     */
    static void handleHistoryNav(HistoryDirection dir);

    /**
     * \brief Processes one input character while in an active escape sequence.
     * \param c Input character.
     * \return `true` if the character was consumed by escape handling.
     */
    static bool handleEscape(int c);

    /**
     * \brief Processes one input character against the special-key dispatch table.
     * \param c Input character.
     * \param[out] commandReady Set to `true` when a complete line was submitted.
     */
    static void handleSpecialChar(int c, bool& commandReady);
};

} // namespace cdc::serial
