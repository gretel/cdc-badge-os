#pragma once

// Serial Command Interface for CDC Badge
// Allows setting time, date, and display text via USB serial

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize serial command interface
void serial_cmd_init(void);

// Process incoming serial data (call from main loop)
// Returns true if a command was processed
bool serial_cmd_process(void);

// Callback type for display text changes
typedef void (*serial_cmd_text_callback_t)(int line, const char *text);

// Set callback for when display text is changed via serial
void serial_cmd_set_text_callback(serial_cmd_text_callback_t callback);

// Callback type for time changes
typedef void (*serial_cmd_time_callback_t)(void);

// Set callback for when time is changed (to update display)
void serial_cmd_set_time_callback(serial_cmd_time_callback_t callback);

#ifdef __cplusplus
}
#endif
