// BLE CTAP2 Transport for FIDO2
// Implements FIDO2 over BLE GATT (CTAP2 BLE specification)

#ifndef BLE_CTAP_H
#define BLE_CTAP_H

#include <stdint.h>
#include <stdbool.h>
#include "feature_flags.h"

#if FEATURE_FIDO2_BLE

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FIDO2 BLE Service UUIDs (from CTAP2 BLE specification)
// ============================================================================

// FIDO2 BLE Service UUID: 0000FFFD-0000-1000-8000-00805F9B34FB
#define FIDO2_BLE_SERVICE_UUID          0xFFFD

// Characteristic UUIDs (short form, base UUID: 0000xxxx-0000-1000-8000-00805f9b34fb)
#define FIDO2_BLE_CONTROL_POINT_UUID    0xFFF1  // Write: receive commands
#define FIDO2_BLE_STATUS_UUID           0xFFF2  // Notify: send responses
#define FIDO2_BLE_CONTROL_POINT_LEN_UUID 0xFFF3 // Read: max message length
#define FIDO2_BLE_SERVICE_REVISION_UUID 0xFFF4  // Read: service revision bitfield

// ============================================================================
// BLE CTAP Frame Types
// ============================================================================

#define BLE_CTAP_FRAME_INIT     0x83    // Initial frame (has length)
#define BLE_CTAP_FRAME_CONT     0x00    // Continuation frame
#define BLE_CTAP_FRAME_CANCEL   0xBE    // Cancel command
#define BLE_CTAP_FRAME_KEEPALIVE 0x82   // Keepalive (status notification)
#define BLE_CTAP_FRAME_ERROR    0xBF    // Error frame

// Keepalive status codes
#define BLE_CTAP_KEEPALIVE_PROCESSING   0x01  // Processing
#define BLE_CTAP_KEEPALIVE_UP_NEEDED    0x02  // User presence needed

// ============================================================================
// Configuration
// ============================================================================

#define BLE_CTAP_MAX_MSG_SIZE   1024    // Max CTAP2 message size
#define BLE_CTAP_MTU_DEFAULT    23      // Default BLE MTU (ATT_MTU - 3)

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize BLE CTAP transport.
 * Sets up GATT service and starts advertising.
 *
 * @return true on success
 */
bool ble_ctap_init(void);

/**
 * Check if BLE CTAP is initialized.
 */
bool ble_ctap_is_initialized(void);

/**
 * Check if a client is connected.
 */
bool ble_ctap_is_connected(void);

/**
 * Start BLE advertising (called automatically by init).
 */
void ble_ctap_start_advertising(void);

/**
 * Stop BLE advertising.
 */
void ble_ctap_stop_advertising(void);

/**
 * Send keepalive notification to connected client.
 *
 * @param status Keepalive status (PROCESSING or UP_NEEDED)
 */
void ble_ctap_send_keepalive(uint8_t status);

/**
 * Deinitialize BLE CTAP transport.
 */
void ble_ctap_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_FIDO2_BLE

#endif // BLE_CTAP_H
