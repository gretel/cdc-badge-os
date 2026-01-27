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

#ifndef FEATURE_PASSWORD
#define FEATURE_PASSWORD 1  // Password vault enabled
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

// BLE Badge vCard feature (badge2badge + QR)
#ifndef FEATURE_BLE_BADGE
#define FEATURE_BLE_BADGE 1
#endif

// BLE HID Keyboard for wireless password/TOTP typing
// Uses BLE HID-over-GATT Profile (HOGP)
#ifndef FEATURE_BLE_HID
#define FEATURE_BLE_HID 1   // BLE HID Keyboard (alternative to USB keyboard)
#endif

// Compile-time check: BLE features require Bluetooth to be enabled in sdkconfig
#if FEATURE_BLE_UART || FEATURE_BLE_BADGE || FEATURE_BLE_HID
  #if !defined(CONFIG_BT_ENABLED) || !CONFIG_BT_ENABLED
    #error "BLE features require CONFIG_BT_ENABLED=y in sdkconfig!"
  #endif
  #if !defined(CONFIG_BT_BLUEDROID_ENABLED) || !CONFIG_BT_BLUEDROID_ENABLED
    #error "BLE features require CONFIG_BT_BLUEDROID_ENABLED=y in sdkconfig!"
  #endif
#endif

// USB Keyboard for TOTP typing
// NOTE: ESP32-S3 has only 5 IN endpoints. With CCID enabled, we exceed limit.
// Temporarily disabled to test CCID. TODO: Find permanent solution.
#ifndef FEATURE_USB_KEYBOARD
#define FEATURE_USB_KEYBOARD 0  // Disabled for CCID testing (EP limit)
#endif

#ifndef FEATURE_SECURE_SERIAL
#define FEATURE_SECURE_SERIAL 1  // Require PIN auth for sensitive commands
#endif

#ifndef FEATURE_CA
#define FEATURE_CA 1  // Certificate Authority
#endif

// PKCS#11 also on GPG - planned
#ifndef FEATURE_CA_PKCS11
#define FEATURE_CA_PKCS11 0
#endif

// Note: SSH key support is provided via FIDO2 (ed25519-sk keys)
// No separate FEATURE_SSH needed - use ssh-keygen -t ed25519-sk with the badge

// ============================================================================
// GPG Key Management
// ============================================================================
#ifndef FEATURE_GPG
#define FEATURE_GPG 1  // GPG key storage and signing
#endif

// GPG over USB CCID (SmartCard interface)
// Requires FEATURE_GPG and FEATURE_USB
#ifndef FEATURE_GPG_CCID
#define FEATURE_GPG_CCID 1  // USB CCID SmartCard interface for GnuPG
#endif

#if FEATURE_GPG_CCID && !FEATURE_GPG
  #error "FEATURE_GPG_CCID requires FEATURE_GPG to be enabled!"
#endif
#if FEATURE_GPG_CCID && !FEATURE_USB
  #error "FEATURE_GPG_CCID requires FEATURE_USB to be enabled!"
#endif

// ============================================================================
// SAO Port (Shitty Add-On) Detection
// ============================================================================
#ifndef FEATURE_SAO
#define FEATURE_SAO 1  // SAO detection and info display (no drivers)
#endif

#endif
