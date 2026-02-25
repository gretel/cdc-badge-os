#pragma once

#include <cstdint>
#include <cstddef>

namespace cdc::serial {

/**
 * Command handler function type
 * @param args Arguments after command name (trimmed, null-terminated)
 */
using CommandHandler = void (*)(const char* args);

/**
 * Command entry for registration
 */
struct Command {
    const char* name;           // Command name (e.g., "TOTP_LIST")
    const char* help;           // Help text
    CommandHandler handler;     // Handler function
    const char* moduleName;     // Module that registered this command (for grouping in HELP)
    bool requiresAuth;          // Requires authentication (when FEATURE_SECURE_SERIAL)
};

/**
 * Command Registry Interface
 *
 * Modules register their commands here. The registry handles:
 * - Command lookup and dispatch
 * - Help text generation
 * - Optional authentication requirements
 */
class ICommandRegistry {
public:
    virtual ~ICommandRegistry() = default;

    /**
     * Register a command
     * @param cmd Command definition
     * @return true if registered successfully
     */
    virtual bool registerCommand(const Command& cmd) = 0;

    /**
     * Unregister all commands from a module
     * @param moduleName Module name used in registration
     */
    virtual void unregisterModule(const char* moduleName) = 0;

    /**
     * Process a command line
     * @param line Full command line (command + arguments)
     * @return true if command was found and executed
     */
    virtual bool processCommand(const char* line) = 0;

    /**
     * Show help for all commands
     */
    virtual void showHelp() = 0;

    /**
     * Get number of registered commands
     */
    virtual size_t getCommandCount() const = 0;

    /**
     * Set authentication provider callback
     * @param authCheck Function that returns true if session is authenticated
     */
    virtual void setAuthProvider(bool (*authCheck)()) = 0;

    /**
     * Set callback for successful command execution
     * Used to reset auth timer when FEATURE_SECURE_SERIAL is enabled
     */
    virtual void setOnCommandExecuted(void (*callback)()) = 0;

    /**
     * Line interceptor for multiline input modes (e.g., vCard paste).
     * When set, called before normal command dispatch.
     * Return true to consume the line, false for normal processing.
     */
    using LineInterceptor = bool (*)(const char* line);

    /**
     * Set or clear the line interceptor
     * @param interceptor Callback, or nullptr to clear
     */
    virtual void setLineInterceptor(LineInterceptor interceptor) { (void)interceptor; }
};

// Get global command registry instance
ICommandRegistry& getCommandRegistry();

} // namespace cdc::serial
