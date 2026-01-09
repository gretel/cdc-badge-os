// BLE UART Service (Nordic UART Service compatible)
// Implements Serial over BLE for wireless logging/debugging

#ifndef BLE_UART_H
#define BLE_UART_H

#include <cstdint>
#include <cstddef>
#include "feature_flags.h"

// ============================================================================
// Power Management
// ============================================================================

typedef enum {
    BLE_POWER_OFF,      // BLE completely off
    BLE_POWER_IDLE,     // Slow advertising (160-320ms) - battery saving
    BLE_POWER_ACTIVE    // Fast advertising (20-40ms) - responsive
} ble_power_mode_t;

// ============================================================================
// Public API
// ============================================================================

#if FEATURE_BLE_UART

// Nordic UART Service (NUS) UUIDs
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
// RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write, Write Without Response)
// TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify)

/**
 * Initialize BLE UART service.
 * Sets up GATT service and starts advertising.
 *
 * @return true on success
 */
bool ble_uart_init(void);

/**
 * Check if BLE UART is initialized.
 */
bool ble_uart_is_initialized(void);

/**
 * Check if a client is connected.
 */
bool ble_uart_is_connected(void);

/**
 * Check if TX notifications are enabled.
 * Data can only be sent when notifications are enabled by the client.
 */
bool ble_uart_tx_ready(void);

/**
 * Send data over BLE UART (TX characteristic).
 * Data is queued and sent via notifications.
 *
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes queued (may be less than len if buffer full)
 */
size_t ble_uart_send(const uint8_t *data, size_t len);

/**
 * Send a null-terminated string over BLE UART.
 *
 * @param str String to send
 * @return Number of bytes sent
 */
size_t ble_uart_print(const char *str);

/**
 * Send formatted string over BLE UART (like printf).
 *
 * @param fmt Format string
 * @return Number of bytes sent
 */
size_t ble_uart_printf(const char *fmt, ...);

/**
 * Check if data is available to read from BLE UART (RX characteristic).
 *
 * @return Number of bytes available
 */
size_t ble_uart_available(void);

/**
 * Read data from BLE UART (RX characteristic).
 *
 * @param buf Buffer to store data
 * @param max_len Maximum bytes to read
 * @return Number of bytes read
 */
size_t ble_uart_read(uint8_t *buf, size_t max_len);

/**
 * Read a single character from BLE UART.
 *
 * @return Character read, or -1 if none available
 */
int ble_uart_getchar(void);

/**
 * Start BLE advertising.
 */
void ble_uart_start_advertising(void);

/**
 * Stop BLE advertising.
 */
void ble_uart_stop_advertising(void);

/**
 * Deinitialize BLE UART.
 */
void ble_uart_deinit(void);

/**
 * Set BLE power mode for battery optimization.
 *
 * @param mode Power mode (OFF, IDLE, ACTIVE)
 */
void ble_uart_set_power_mode(ble_power_mode_t mode);

/**
 * Get current BLE power mode.
 *
 * @return Current power mode
 */
ble_power_mode_t ble_uart_get_power_mode(void);

/**
 * Reset activity timer (call on BLE events to prevent auto-off).
 */
void ble_uart_reset_activity(void);

/**
 * Get time since last activity in milliseconds.
 *
 * @return Milliseconds since last BLE activity
 */
uint32_t ble_uart_get_idle_time_ms(void);

// Callback type for RX data
typedef void (*ble_uart_rx_callback_t)(const uint8_t *data, size_t len);

/**
 * Set callback for received data.
 * Called when data is written to RX characteristic.
 *
 * @param callback Function to call on RX data (NULL to disable)
 */
void ble_uart_set_rx_callback(ble_uart_rx_callback_t callback);

// Callback type for passkey display (called when pairing requires passkey entry)
typedef void (*ble_passkey_display_cb_t)(uint32_t passkey);

/**
 * Set callback for displaying passkey during pairing.
 * Called when a client initiates pairing and needs to enter the passkey.
 *
 * @param callback Function to display passkey (e.g., show on screen via toast)
 */
void ble_uart_set_passkey_display_callback(ble_passkey_display_cb_t callback);

/**
 * Check if the current connection is bonded (paired).
 *
 * @return true if bonded/paired
 */
bool ble_uart_is_bonded(void);

// Callback type for auth complete (to dismiss passkey toast)
typedef void (*ble_auth_complete_cb_t)(bool success);

/**
 * Set callback for authentication complete event.
 * Called when pairing succeeds or fails - use to dismiss passkey toast.
 *
 * @param callback Function to call on auth complete (success = true if paired)
 */
void ble_uart_set_auth_complete_callback(ble_auth_complete_cb_t callback);

#else // !FEATURE_BLE_UART - Stub implementations

static inline bool ble_uart_init(void) { return false; }
static inline bool ble_uart_is_initialized(void) { return false; }
static inline bool ble_uart_is_connected(void) { return false; }
static inline bool ble_uart_tx_ready(void) { return false; }
static inline size_t ble_uart_send(const uint8_t *data, size_t len) { (void)data; (void)len; return 0; }
static inline size_t ble_uart_print(const char *str) { (void)str; return 0; }
static inline size_t ble_uart_printf(const char *fmt, ...) { (void)fmt; return 0; }
static inline size_t ble_uart_available(void) { return 0; }
static inline size_t ble_uart_read(uint8_t *buf, size_t max_len) { (void)buf; (void)max_len; return 0; }
static inline int ble_uart_getchar(void) { return -1; }
static inline void ble_uart_start_advertising(void) {}
static inline void ble_uart_stop_advertising(void) {}
static inline void ble_uart_deinit(void) {}
static inline void ble_uart_set_power_mode(ble_power_mode_t mode) { (void)mode; }
static inline ble_power_mode_t ble_uart_get_power_mode(void) { return BLE_POWER_OFF; }
static inline void ble_uart_reset_activity(void) {}
static inline uint32_t ble_uart_get_idle_time_ms(void) { return 0; }
typedef void (*ble_uart_rx_callback_t)(const uint8_t *data, size_t len);
static inline void ble_uart_set_rx_callback(ble_uart_rx_callback_t callback) { (void)callback; }
typedef void (*ble_passkey_display_cb_t)(uint32_t passkey);
static inline void ble_uart_set_passkey_display_callback(ble_passkey_display_cb_t callback) { (void)callback; }
static inline bool ble_uart_is_bonded(void) { return false; }
typedef void (*ble_auth_complete_cb_t)(bool success);
static inline void ble_uart_set_auth_complete_callback(ble_auth_complete_cb_t callback) { (void)callback; }

#endif // FEATURE_BLE_UART

#endif // BLE_UART_H
