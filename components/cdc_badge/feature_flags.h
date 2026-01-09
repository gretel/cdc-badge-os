#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include "sdkconfig.h"

// CDC Badge Feature Flags
// All features enabled by default
// Disable via -D flags: -DFEATURE_FIDO2=0

// ============================================================================
// DEBUG MODE: Disables security features like lockout for development
// ============================================================================
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // Set to 0 for production
#endif

// ============================================================================
// USB MODE: TinyUSB (custom device) vs USB-Serial-JTAG (debug)
// ============================================================================
// FEATURE_USB enables TinyUSB for CDC/HID composite device.
// This is MUTUALLY EXCLUSIVE with USB-Serial-JTAG debug!
// To use TinyUSB, disable CONFIG_USJ_ENABLE_USB_SERIAL_JTAG in sdkconfig.

#ifndef FEATURE_USB
#define FEATURE_USB 1  // TinyUSB enabled for CDC serial
#endif

// Note: TinyUSB and USB-Serial-JTAG are mutually exclusive at runtime.
// CONFIG_USJ_ENABLE_USB_SERIAL_JTAG must be disabled in sdkconfig for TinyUSB to work.

// Compile-time check: USB features require FEATURE_USB base
#if FEATURE_USB_KEYBOARD && !FEATURE_USB
  #error "FEATURE_USB_KEYBOARD requires FEATURE_USB to be enabled!"
#endif
#if FEATURE_FIDO2_USB && !FEATURE_USB
  #error "FEATURE_FIDO2_USB requires FEATURE_USB to be enabled!"
#endif

// ============================================================================
// TEST MODE: All optional features disabled for minimal build testing
// ============================================================================

#ifndef FEATURE_TOTP
#define FEATURE_TOTP 1  // TOTP enabled
#endif

// FIDO2 over USB HID
#ifndef FEATURE_FIDO2_USB
#define FEATURE_FIDO2_USB 1  // FIDO2 over USB HID (requires FEATURE_USB)
#endif

// Convenience: FIDO2 enabled if USB transport is enabled
#define FEATURE_FIDO2 FEATURE_FIDO2_USB

// BLE UART (Serial over Bluetooth)
#ifndef FEATURE_BLE_UART
#define FEATURE_BLE_UART 1   // BLE UART for wireless serial console
#endif

// Compile-time check: BLE UART requires Bluetooth to be enabled in sdkconfig
#if FEATURE_BLE_UART
  #if !defined(CONFIG_BT_ENABLED) || !CONFIG_BT_ENABLED
    #error "FEATURE_BLE_UART requires CONFIG_BT_ENABLED=y in sdkconfig!"
  #endif
  #if !defined(CONFIG_BT_BLUEDROID_ENABLED) || !CONFIG_BT_BLUEDROID_ENABLED
    #error "FEATURE_BLE_UART requires CONFIG_BT_BLUEDROID_ENABLED=y in sdkconfig!"
  #endif
#endif

#ifndef FEATURE_USB_KEYBOARD
#define FEATURE_USB_KEYBOARD 1  // USB keyboard for TOTP typing
#endif

#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 0  // DISABLED FOR TESTING
#endif

#ifndef FEATURE_CA
#define FEATURE_CA 1  // Certificate Authority
#endif

// PKCS#11 requires wolfPKCS11 evaluation - disabled until proven viable
#ifndef FEATURE_CA_PKCS11
#define FEATURE_CA_PKCS11 0
#endif

// Note: SSH key support is provided via FIDO2 (ed25519-sk keys)
// No separate FEATURE_SSH needed - use ssh-keygen -t ed25519-sk with the badge

#endif
