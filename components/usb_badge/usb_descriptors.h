// USB Descriptors for CDC Badge
// Composite Device: CDC + optional FIDO HID + optional Keyboard HID
// Interfaces are conditionally included based on feature flags

#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>
#include "feature_flags.h"

// USB Device IDs
// Using Espressif's official VID with custom PID
#define USB_VID   0x303A  // Espressif Systems
#define USB_PID   0xBADE  // CDC Badge custom PID
#define USB_BCD   0x0200

// String Descriptor Indices
enum {
    STR_LANGID = 0,
    STR_MANUFACTURER,
    STR_PRODUCT,
    STR_SERIAL,
    STR_CDC,
#if FEATURE_FIDO2
    STR_FIDO,
#endif
#if FEATURE_USB_KEYBOARD
    STR_KEYBOARD,
#endif
    STR_COUNT
};

// ============================================================================
// Interface Numbers (conditional based on features)
// CDC is always interfaces 0 + 1
// FIDO and Keyboard interfaces are numbered dynamically
// ============================================================================

#define ITF_CDC           0
#define ITF_CDC_DATA      1

// Next interface after CDC
#define ITF_AFTER_CDC     2

#if FEATURE_FIDO2
#define ITF_FIDO          ITF_AFTER_CDC
#define ITF_AFTER_FIDO    (ITF_FIDO + 1)
#else
#define ITF_AFTER_FIDO    ITF_AFTER_CDC
#endif

#if FEATURE_USB_KEYBOARD
#define ITF_KEYBOARD      ITF_AFTER_FIDO
#define ITF_COUNT         (ITF_KEYBOARD + 1)
#else
#define ITF_COUNT         ITF_AFTER_FIDO
#endif

// ============================================================================
// Endpoint Numbers (conditional)
// CDC always uses: 0x81 (notif), 0x02 (out), 0x82 (in)
// FIDO uses: 0x03 (out), 0x83 (in) if enabled
// Keyboard uses: next available IN endpoint
// ============================================================================

#define EP_CDC_NOTIF      0x81    // CDC Notification IN
#define EP_CDC_OUT        0x02    // CDC Data OUT
#define EP_CDC_IN         0x82    // CDC Data IN

#if FEATURE_FIDO2
#define EP_FIDO_OUT       0x03    // FIDO HID OUT
#define EP_FIDO_IN        0x83    // FIDO HID IN
#define EP_AFTER_FIDO     0x84
#else
#define EP_AFTER_FIDO     0x83
#endif

#if FEATURE_USB_KEYBOARD
#define EP_KEYBOARD_IN    EP_AFTER_FIDO    // Keyboard HID IN (no OUT)
#endif

// Endpoint Sizes
#define EP_CDC_NOTIF_SIZE   8
#define EP_CDC_SIZE         64
#define EP_FIDO_SIZE        64    // CTAPHID packets are 64 bytes
#define EP_KEYBOARD_SIZE    8     // Keyboard reports are 8 bytes
#define EP_CCID_SIZE        64    // CCID SmartCard packets

// HID Report ID
#define REPORT_ID_KEYBOARD  1     // Keyboard uses Report ID 1
// FIDO: No Report ID per CTAPHID spec

// ============================================================================
// CCID SmartCard Interface (conditional)
// Uses TinyUSB Vendor Class with custom CCID driver
// ============================================================================

#if FEATURE_GPG_CCID

// CCID Interface - calculate based on what's enabled
// Interface order: CDC(0,1), FIDO(2), Keyboard(3), CCID(4)
#if FEATURE_USB_KEYBOARD && FEATURE_FIDO2
#define ITF_CCID          4                         // After CDC(2) + FIDO(1) + Keyboard(1)
#elif FEATURE_USB_KEYBOARD
#define ITF_CCID          3                         // After CDC(2) + Keyboard(1)
#elif FEATURE_FIDO2
#define ITF_CCID          3                         // After CDC(2) + FIDO(1)
#else
#define ITF_CCID          2                         // After CDC(2) only
#endif

// Override ITF_COUNT to include CCID
#undef ITF_COUNT
#define ITF_COUNT         (ITF_CCID + 1)

// CCID Endpoints - use next available
#if FEATURE_USB_KEYBOARD && FEATURE_FIDO2
#define EP_CCID_OUT       0x05                      // After FIDO(0x03/0x83) + Keyboard(0x84)
#define EP_CCID_IN        0x85
#elif FEATURE_USB_KEYBOARD
#define EP_CCID_OUT       0x04                      // After Keyboard(0x83)
#define EP_CCID_IN        0x84
#elif FEATURE_FIDO2
#define EP_CCID_OUT       0x04                      // After FIDO (0x03/0x83)
#define EP_CCID_IN        0x84
#else
#define EP_CCID_OUT       0x03                      // After CDC
#define EP_CCID_IN        0x83
#endif

// CCID String Index - calculate based on what's enabled
// String order: LANG(0), MANUF(1), PROD(2), SERIAL(3), CDC(4), [FIDO(5)], [KB(6)], [CCID]
#if FEATURE_FIDO2 && FEATURE_USB_KEYBOARD
#define STR_CCID          7
#elif FEATURE_FIDO2 || FEATURE_USB_KEYBOARD
#define STR_CCID          6
#else
#define STR_CCID          5
#endif

#undef STR_COUNT
#define STR_COUNT         (STR_CCID + 1)

#endif // FEATURE_GPG_CCID

// ============================================================================
// HID Instance Numbers (for tud_hid_n_* functions)
// ============================================================================

#if FEATURE_FIDO2
#define USB_HID_INSTANCE_FIDO     0
#if FEATURE_USB_KEYBOARD
#define USB_HID_INSTANCE_KEYBOARD 1
#endif
#else
#if FEATURE_USB_KEYBOARD
#define USB_HID_INSTANCE_KEYBOARD 0
#endif
#endif

#endif // USB_DESCRIPTORS_H
