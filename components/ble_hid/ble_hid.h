// BLE HID Keyboard Service (HID-over-GATT Profile)
// Implements keyboard HID for wireless password/TOTP typing

#ifndef BLE_HID_H
#define BLE_HID_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "feature_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

#if FEATURE_BLE_HID

/**
 * Initialize BLE HID Keyboard service.
 * Requires ble_core to be initialized first.
 *
 * @return true on success
 */
bool ble_hid_init(void);

/**
 * Deinitialize BLE HID service.
 */
void ble_hid_deinit(void);

/**
 * Check if BLE HID is initialized.
 *
 * @return true if initialized
 */
bool ble_hid_is_initialized(void);

/**
 * Check if a HID host is connected and ready.
 *
 * @return true if connected and notifications enabled
 */
bool ble_hid_ready(void);

/**
 * Check if HID host is connected.
 *
 * @return true if connected
 */
bool ble_hid_is_connected(void);

/**
 * Type a string as keyboard input.
 * Uses US keyboard layout.
 *
 * @param str Null-terminated string to type
 * @return true on success
 */
bool ble_hid_type(const char* str);

/**
 * Type a string and optionally press Enter.
 *
 * @param str String to type
 * @param press_enter true to press Enter after string
 * @return true on success
 */
bool ble_hid_type_with_enter(const char* str, bool press_enter);

/**
 * Send a single keystroke.
 *
 * @param modifier Modifier byte (CTRL, SHIFT, etc.)
 * @param keycode HID keycode
 * @return true on success
 */
bool ble_hid_send_key(uint8_t modifier, uint8_t keycode);

/**
 * Release all keys (send empty report).
 */
void ble_hid_release_keys(void);

/**
 * Press Enter key.
 *
 * @return true on success
 */
bool ble_hid_press_enter(void);

#else // Stubs when FEATURE_BLE_HID is disabled

static inline bool ble_hid_init(void) { return false; }
static inline void ble_hid_deinit(void) {}
static inline bool ble_hid_is_initialized(void) { return false; }
static inline bool ble_hid_ready(void) { return false; }
static inline bool ble_hid_is_connected(void) { return false; }
static inline bool ble_hid_type(const char* str) { (void)str; return false; }
static inline bool ble_hid_type_with_enter(const char* str, bool press_enter) {
    (void)str; (void)press_enter; return false;
}
static inline bool ble_hid_send_key(uint8_t m, uint8_t k) { (void)m; (void)k; return false; }
static inline void ble_hid_release_keys(void) {}
static inline bool ble_hid_press_enter(void) { return false; }

#endif // FEATURE_BLE_HID

#ifdef __cplusplus
}
#endif

#endif // BLE_HID_H
