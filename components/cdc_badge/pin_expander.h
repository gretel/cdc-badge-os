#pragma once

// TCA9535 I/O Expander - 12-Key Keypad Driver
// The badge uses a TCA9535 (or TCA9555) as a 16-bit I/O expander.
// 12 keys are directly connected (active-low when pressed).
//
// Key mapping:
//   '0'-'9' = numeric keys
//   'Y' = Yes/OK key
//   'N' = No/Back key
//   'x' = no key pressed

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the pin expander (call after i2c_bus_init)
bool pin_expander_init(void);

// Poll keypad for new keypresses (call frequently)
// Buffers any detected keypresses for later retrieval
void pin_expander_poll(void);

// Get next key from buffer, or 'x' if empty
// Also polls for new keypresses before checking buffer
char pin_expander_get_key(void);

// Check if a specific key is currently held down
bool pin_expander_is_key_down(char key);

// Check if any key is currently pressed
bool pin_expander_any_key_down(void);

// Check if there's a key waiting in the buffer
bool pin_expander_has_key(void);

// Disable/enable ISR (for light sleep)
void pin_expander_disable_isr(void);
void pin_expander_enable_isr(void);

#ifdef __cplusplus
}
#endif
