#pragma once

#include "tusb_option.h"
#include "sdkconfig.h"

// ============================================================================
// HID Interface Count
// ============================================================================
// CFG_TUD_HID must match the actual number of HID interfaces in the
// configuration descriptor. Set via build flags in platformio.ini.
//
// Example: -D TUSB_HID_COUNT=2 for FIDO2 + Keyboard
//          -D TUSB_HID_COUNT=1 for only Keyboard or only FIDO2
//          -D TUSB_HID_COUNT=0 for no HID (CDC only)

#ifndef TUSB_HID_COUNT
#define TUSB_HID_COUNT 1    // FIDO2 only (Keyboard disabled for CCID testing)
#endif

#define TUSB_FEATURE_HID (TUSB_HID_COUNT > 0)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_TINYUSB_CDC_ENABLED
#define CONFIG_TINYUSB_CDC_ENABLED 0
#endif

#ifndef CONFIG_TINYUSB_MSC_ENABLED
#define CONFIG_TINYUSB_MSC_ENABLED 0
#endif

#ifndef CONFIG_TINYUSB_HID_ENABLED
#define CONFIG_TINYUSB_HID_ENABLED 0
#endif

#ifndef CONFIG_TINYUSB_MIDI_ENABLED
#define CONFIG_TINYUSB_MIDI_ENABLED 0
#endif

#ifndef CONFIG_TINYUSB_CUSTOM_CLASS_ENABLED
#define CONFIG_TINYUSB_CUSTOM_CLASS_ENABLED 0
#endif

#ifndef CONFIG_TINYUSB_CDC_RX_BUFSIZE
#define CONFIG_TINYUSB_CDC_RX_BUFSIZE 64
#endif

#ifndef CONFIG_TINYUSB_CDC_TX_BUFSIZE
#define CONFIG_TINYUSB_CDC_TX_BUFSIZE 64
#endif

#ifndef CONFIG_TINYUSB_MSC_BUFSIZE
#define CONFIG_TINYUSB_MSC_BUFSIZE 512
#endif

#ifndef CONFIG_TINYUSB_HID_BUFSIZE
#define CONFIG_TINYUSB_HID_BUFSIZE 64
#endif

#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED
#define CFG_TUSB_OS                 OPT_OS_FREERTOS

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          TU_ATTR_ALIGNED(4)
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE      64
#endif

// Maximum number of endpoints (excluding EP0)
// CDC needs 3 (notif, out, in), FIDO needs 2, Keyboard needs 1, CCID needs 2
// Total: 8 endpoints (ESP32-S3 supports up to 6 IN + 6 OUT)
#ifndef CFG_TUD_ENDPOINT_MAX
#define CFG_TUD_ENDPOINT_MAX        8
#endif

#define CFG_TUD_CDC_RX_BUFSIZE      CONFIG_TINYUSB_CDC_RX_BUFSIZE
#define CFG_TUD_CDC_TX_BUFSIZE      CONFIG_TINYUSB_CDC_TX_BUFSIZE
#define CFG_TUD_MSC_BUFSIZE         CONFIG_TINYUSB_MSC_BUFSIZE
#define CFG_TUD_HID_BUFSIZE         CONFIG_TINYUSB_HID_BUFSIZE

// Enabled device class driver counts
#define CFG_TUD_CDC                 1   // CDC always enabled for serial console
#define CFG_TUD_MSC                 0   // MSC not used
#define CFG_TUD_HID                 TUSB_HID_COUNT  // Set via TUSB_HID_COUNT above
#define CFG_TUD_MIDI                0   // MIDI not used
#define CFG_TUD_CUSTOM_CLASS        0   // Custom class not used

// ============================================================================
// CCID SmartCard via Custom Class Driver
// ============================================================================
// CCID uses a custom TinyUSB class driver since TinyUSB doesn't have native
// CCID support. The driver is registered via usbd_app_driver_get_cb().
//
// TUSB_CCID_ENABLED is set via platformio.ini build_flags:
// -D TUSB_CCID_ENABLED=1 when FEATURE_GPG_CCID is enabled

#ifndef TUSB_CCID_ENABLED
#define TUSB_CCID_ENABLED 0  // Default: disabled (sync with feature_flags.h FEATURE_GPG_CCID)
#endif

// No CFG_TUD_VENDOR needed - we use a custom class driver for CCID
#define CFG_TUD_VENDOR              0

#ifdef __cplusplus
}
#endif
