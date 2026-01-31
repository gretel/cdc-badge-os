// USB HID Module - Composite Device
// CDC + optional HID/CCID interfaces registered by modules

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize HID-related resources (if any).
// TinyUSB init is performed by usb_cdc_init().
bool usb_hid_init(void);

enum class UsbInterfaceClass : uint8_t {
    Hid = 0,
    Ccid = 1,
};

struct UsbHidCallbacks {
    // report_type matches HID report type (input/output/feature)
    uint16_t (*onGetReport)(uint8_t report_id, uint8_t report_type,
                            uint8_t* buffer, uint16_t reqlen);
    void (*onSetReport)(uint8_t report_id, uint8_t report_type,
                        uint8_t const* buffer, uint16_t bufsize);
    void (*onReportComplete)(uint8_t const* report, uint16_t len);
};

struct UsbInterfaceDef {
    UsbInterfaceClass cls;
    const char* name;  // Interface name for USB string descriptor

    // HID fields
    const uint8_t* reportDesc;
    uint16_t reportDescLen;
    uint8_t protocol;  // HID protocol (0=none, 1=keyboard)
    bool hasOut;
    uint16_t epInSize;
    uint16_t epOutSize;
    UsbHidCallbacks callbacks;
};

// Apply active interface list (ordered). Attempts soft reconnect; sets needs_replug
// if host may require replug.
bool usb_hid_apply_config(const UsbInterfaceDef* defs, size_t count, bool* needs_replug);

// Check if USB is ready (CDC or HID).
bool usb_hid_ready(void);

// Per-instance HID helpers (instance is 0..n-1 in registration order)
bool usb_hid_instance_ready(uint8_t instance);
bool usb_hid_send_report(uint8_t instance, uint8_t report_id, const uint8_t* data, uint16_t len);

#ifdef __cplusplus
}
#endif
