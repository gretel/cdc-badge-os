#include "app_system.h"

#include "app_fido.h"
#include "app_globals.h"
#include "app_render.h"

#include "badge_settings.h"
#include "cdc_log.h"
#include "pin_storage.h"
#include "power_management.h"
#include "serial_cmd.h"
#include "tropic01.h"
#include "tropic01_cache.h"
#include "usb_hid.h"

#if FEATURE_TOTP
#include "totp_store.h"
#endif
#if FEATURE_FIDO2
#include "fido2.h"
#endif
#if FEATURE_BLE_UART
#include "ble_uart.h"
#include "views.h"
#include <cstdio>
#endif
#if FEATURE_CA
#include "ca.h"
#endif

#include "esp_system.h"

// ============================================================================
// BLE Callbacks
// ============================================================================

#if FEATURE_BLE_UART
void ble_passkey_display(uint32_t passkey) {
    // Display the 6-digit passkey via toast
    // User needs to enter this code on their phone to complete pairing
    // Toast can be dismissed early with Y/N key
    char msg[32];
    snprintf(msg, sizeof(msg), "PIN: %06lu", (unsigned long)passkey);
    LOG_I("BLE", "Pairing passkey: %06lu", (unsigned long)passkey);
    view_toast_show(msg, 60000);  // 60 seconds max, dismissible with Y/N
}

void ble_auth_complete(bool success) {
    // Called from BLE callback after pairing completes
    // The passkey toast is already dismissed (user pressed Y/N or timeout)
    LOG_I("BLE", "Auth complete: %s", success ? "paired" : "failed");
}
#endif

// ============================================================================
// Serial Callbacks
// ============================================================================

void on_text_change(int line, const char *text) {
    if (line == 0) {
        badge_settings_set_name(text);
    } else if (line == 1) {
        badge_settings_set_info(text);
    } else if (line == 2) {
        badge_settings_set_info2(text);
    }
    badge_settings_save();

    if (g_app_state == APP_STATE_LOCK_SCREEN) {
        update_lock_screen_data();
        render_current_state(true);
    }
}

void on_time_change(void) {
    g_last_minute = -1;
    if (g_app_state == APP_STATE_LOCK_SCREEN) {
        update_lock_screen_data();
        render_current_state(true);
    }
}

// ============================================================================
// System Initialization
// ============================================================================

void system_init(void) {
    LOG_I("INIT", "system_init()");

    // Initialize TROPIC01
    if (tropic01_init()) {
        g_hw_status.tropic01_ok = true;
        LOG_I("INIT", "TROPIC01 OK");

        // Start secure session
        if (tropic01_session_start()) {
            g_hw_status.tropic01_session_ok = true;
            LOG_I("INIT", "TROPIC01 session OK");

            // Load cache from TROPIC01 (prevents chip lockout)
            tropic01_cache_init();

            // Load PIN from TROPIC01
            pin_storage_load();

#if FEATURE_CA
            // Initialize CA module (loads state from TROPIC01)
            ca_init();
#endif
        } else {
            LOG_E("INIT", "TROPIC01 session failed");
        }
    } else {
        LOG_E("INIT", "TROPIC01 init failed");
    }

    // Initialize USB (TinyUSB composite device: CDC + optional HID)
    // Requires FEATURE_USB=1 and CONFIG_USJ_ENABLE_USB_SERIAL_JTAG=n in sdkconfig
#if FEATURE_USB
    if (usb_hid_init()) {
        LOG_I("INIT", "USB OK (TinyUSB)");
    } else {
        LOG_E("INIT", "USB init failed");
    }
#endif

    // Initialize console (TinyUSB CDC or JTAG/UART fallback)
    console_init();

    // Initialize TOTP store (uses TROPIC01 cache)
#if FEATURE_TOTP
    uint8_t totp_count = totp_store_init();
    LOG_I("INIT", "TOTP: %d accounts", totp_count);
#endif

    // Initialize FIDO2 module
#if FEATURE_FIDO2
    // Create semaphore for user presence callback
    g_fido_prompt_sem = xSemaphoreCreateBinary();
    if (!g_fido_prompt_sem) {
        LOG_E("INIT", "Failed to create FIDO2 semaphore");
    }

    if (fido2_init()) {
        // Register user presence callback
        fido2_set_user_presence_callback(fido2_user_presence_callback);
        LOG_I("INIT", "FIDO2: %d credentials", fido2_get_credential_count());
    } else {
        LOG_E("INIT", "FIDO2 init failed");
    }
#endif

    // BLE UART stays OFF at boot; user can enable via UI.
#if FEATURE_BLE_UART
    // Register BLE callbacks (will be used when BLE is enabled later)
    ble_uart_set_passkey_display_callback(ble_passkey_display);
    ble_uart_set_auth_complete_callback(ble_auth_complete);
    LOG_I("INIT", "BLE UART disabled at boot (passkey auth enabled)");
#endif

    LOG_I("INIT", "system_init() complete");
}
