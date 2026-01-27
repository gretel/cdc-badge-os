// Keyboard Typing Abstraction
// Provides unified interface for typing via USB or BLE HID

#ifndef KEYBOARD_TYPING_H
#define KEYBOARD_TYPING_H

#include <stdbool.h>
#include "feature_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEYBOARD_TRANSPORT_NONE,
    KEYBOARD_TRANSPORT_USB,
    KEYBOARD_TRANSPORT_BLE
} keyboard_transport_t;

/**
 * Get the available keyboard transport.
 * Prefers USB if connected, falls back to BLE.
 *
 * @return Available transport type
 */
keyboard_transport_t keyboard_get_transport(void);

/**
 * Check if any keyboard transport is available.
 *
 * @return true if typing is possible
 */
bool keyboard_is_available(void);

/**
 * Type a string via the best available transport.
 *
 * @param str String to type
 * @param press_enter true to press Enter after string
 * @return true on success
 */
bool keyboard_type(const char* str, bool press_enter);

/**
 * Press Enter via the best available transport.
 *
 * @return true on success
 */
bool keyboard_press_enter(void);

#ifdef __cplusplus
}
#endif

#endif // KEYBOARD_TYPING_H
