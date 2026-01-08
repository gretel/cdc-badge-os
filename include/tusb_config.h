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
#define TUSB_HID_COUNT 2    // Default: FIDO2 + Keyboard (both enabled by default)
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

#ifdef __cplusplus
}
#endif
