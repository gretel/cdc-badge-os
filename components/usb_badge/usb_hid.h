// USB HID Module - Composite Device
// CDC + optional FIDO HID + optional Keyboard HID
// Interfaces conditionally included based on feature flags

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "feature_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize USB composite device
// Interfaces depend on FEATURE_FIDO2 and FEATURE_USB_KEYBOARD
bool usb_hid_init(void);

// Check if USB is ready
bool usb_hid_ready(void);

#ifdef __cplusplus
}
#endif

// C++ namespaces for HID interfaces (outside extern "C")
// These are ALWAYS declared - they return false/no-op when USB is disabled or not initialized.
// This allows code to compile without #ifdef guards around USB calls.
#ifdef __cplusplus

// FIDO HID Interface (64-byte packets, no Report ID)
// Returns false/0 when USB not available or FEATURE_FIDO2 disabled
namespace usb_fido {
    bool ready(void);
    bool available(void);          // Check if 64-byte packet available
    uint16_t read(uint8_t *buffer); // Read 64-byte packet (non-blocking)
    uint16_t read_timeout(uint8_t *buffer, uint32_t timeout_ms); // Read with timeout
    bool write(const uint8_t *buffer); // Write 64-byte packet
}

// Keyboard HID Interface (8-byte reports, Report ID 1)
// Returns false when USB not available or FEATURE_USB_KEYBOARD disabled
namespace usb_keyboard {
    bool ready(void);
    bool type(const char *str);     // Type string (US keyboard layout)
    bool type_enter(void);          // Press Enter key
}

#endif // __cplusplus
