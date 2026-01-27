// BLE Core - Centralized Bluetooth stack management for multi-service support
// Enables BLE UART, BLE Badge, and BLE HID to coexist simultaneously

#ifndef BLE_CORE_H
#define BLE_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "feature_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

// Service App IDs (each service gets a unique ID)
typedef enum {
    BLE_APP_UART = 0,       // Nordic UART Service
    BLE_APP_BADGE = 1,      // vCard/GPG Exchange Service
    BLE_APP_HID = 2,        // HID Keyboard Service
    BLE_APP_MAX
} ble_app_id_t;

// Callback types
typedef void (*ble_core_passkey_cb_t)(uint32_t passkey);
typedef void (*ble_core_auth_cb_t)(bool success);
typedef void (*ble_core_nc_cb_t)(uint32_t passkey);  // Numeric comparison

// Forward declare ESP-IDF types for GAP event forwarding
struct ble_gap_cb_param_t;
typedef void (*ble_core_gap_event_cb_t)(int event, void *param);

#if FEATURE_BLE_UART || FEATURE_BLE_BADGE || FEATURE_BLE_HID

/**
 * Initialize BLE core (BT controller + Bluedroid stack).
 * Must be called once before any BLE service initialization.
 * Thread-safe - multiple calls are idempotent.
 *
 * @return true on success
 */
bool ble_core_init(void);

/**
 * Deinitialize BLE core.
 * Shuts down all BLE services and the Bluetooth stack.
 * Should only be called when all services are done.
 */
void ble_core_deinit(void);

/**
 * Check if BLE core is initialized.
 *
 * @return true if initialized
 */
bool ble_core_is_initialized(void);

/**
 * Get the shared device name.
 *
 * @return Device name string
 */
const char* ble_core_get_device_name(void);

/**
 * Set the device name for BLE advertising.
 *
 * @param name Device name (will be truncated if >29 chars)
 */
void ble_core_set_device_name(const char* name);

/**
 * Set callback for passkey display during pairing.
 * Called when a remote device needs to enter the passkey shown on badge.
 *
 * @param cb Callback function (NULL to disable)
 */
void ble_core_set_passkey_callback(ble_core_passkey_cb_t cb);

/**
 * Set callback for authentication complete.
 * Called when pairing succeeds or fails.
 *
 * @param cb Callback function (NULL to disable)
 */
void ble_core_set_auth_callback(ble_core_auth_cb_t cb);

/**
 * Set callback for numeric comparison during pairing.
 * Called when both devices need to confirm the same number.
 *
 * @param cb Callback function (NULL to disable)
 */
void ble_core_set_nc_callback(ble_core_nc_cb_t cb);

/**
 * Confirm numeric comparison pairing.
 * Call this after user confirms the number matches.
 *
 * @param addr Remote device address
 * @param accept true to accept, false to reject
 */
void ble_core_confirm_pairing(const uint8_t addr[6], bool accept);

/**
 * Start BLE advertising with current configuration.
 */
void ble_core_start_advertising(void);

/**
 * Stop BLE advertising.
 */
void ble_core_stop_advertising(void);

/**
 * Check if advertising is currently active.
 *
 * @return true if advertising
 */
bool ble_core_is_advertising(void);

/**
 * Get the local BLE address.
 *
 * @param addr Output buffer for 6-byte address
 * @return true if address was retrieved
 */
bool ble_core_get_address(uint8_t addr[6]);

/**
 * Register a GAP event listener for receiving all GAP events.
 * Used by services that need scan results or other GAP events.
 * Multiple listeners can be registered.
 *
 * @param cb Callback function (NULL to unregister)
 * @param id Unique identifier for this listener (use BLE_APP_* constants)
 */
void ble_core_register_gap_listener(ble_core_gap_event_cb_t cb, ble_app_id_t id);

#else // Stubs when no BLE feature is enabled

static inline bool ble_core_init(void) { return false; }
static inline void ble_core_deinit(void) {}
static inline bool ble_core_is_initialized(void) { return false; }
static inline const char* ble_core_get_device_name(void) { return ""; }
static inline void ble_core_set_device_name(const char* name) { (void)name; }
static inline void ble_core_set_passkey_callback(ble_core_passkey_cb_t cb) { (void)cb; }
static inline void ble_core_set_auth_callback(ble_core_auth_cb_t cb) { (void)cb; }
static inline void ble_core_set_nc_callback(ble_core_nc_cb_t cb) { (void)cb; }
static inline void ble_core_confirm_pairing(const uint8_t addr[6], bool accept) { (void)addr; (void)accept; }
static inline void ble_core_start_advertising(void) {}
static inline void ble_core_stop_advertising(void) {}
static inline bool ble_core_is_advertising(void) { return false; }
static inline bool ble_core_get_address(uint8_t addr[6]) { (void)addr; return false; }
static inline void ble_core_register_gap_listener(ble_core_gap_event_cb_t cb, ble_app_id_t id) { (void)cb; (void)id; }

#endif

#ifdef __cplusplus
}
#endif

#endif // BLE_CORE_H
