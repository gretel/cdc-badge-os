---
title: "[HIGH] USB CDC/HID integration lacks tests for communication interface"
severity: HIGH
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:usb_badge"
  - "area:usb"
---

## Summary
The `usb_badge` component provides USB CDC (serial) and HID (keyboard/mouse) interfaces for communication with the host, but **no integration tests** verify that USB enumeration, data transmission, and reception work correctly.

## Evidence

**USB CDC API** (`components/usb_badge/include/usb_badge/usb_cdc.h`):
```cpp
bool usb_cdc_init();
void usb_cdc_start();
int usb_cdc_read(char* buf, int len);
void usb_cdc_write(const char* buf, int len);
```

**USB HID API** (`components/usb_badge/include/usb_badge/usb_hid.h`):
```cpp
bool usb_hid_init();
bool usb_hid_send_keyboard(uint8_t modifiers, uint8_t keys[6]);
bool usb_hid_send_media(uint8_t media_key);
```

**FIDO2 HID integration** (`components/mod_fido2/src/Fido2Module.cpp:1-100`):
```cpp
// FIDO2 uses USB HID with specific report descriptor
static const uint8_t s_fido_report_desc[] = {
    0x06, 0xD0, 0xF1,  // Usage Page (FIDO Alliance)
    0x09, 0x01,        // Usage (U2F HID Authenticator Device)
    // ...
};

static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    // Incoming HID reports queued here
}
```

**TinyUSB integration** (`managed_components/espressif__tinyusb/`):
- Uses ESP-IDF TinyUSB component
- Multiple HID interfaces (keyboard, media, FIDO)

**Current test coverage**: Only unit tests in TinyUSB managed component, no integration tests for badge-specific usage

## Impact
- **USB enumeration failures**: Multiple HID interfaces may conflict
- **Data corruption**: CDC transmit/receive could lose bytes
- **FIDO2 compatibility**: CTAPHID transport not verified with real hosts
- **Keyboard reporting**: HID keyboard may not work with all hosts

## Recommended Fix

Create integration test `test_usb_cdc_hid/` that verifies:

1. **USB enumeration**: CDC and HID interfaces enumerate correctly
2. **CDC transmit/receive**: Data sent via `usb_cdc_write()` received on host
3. **HID keyboard**: Keyboard reports sent correctly
4. **FIDO2 HID**: CTAPHID packets sent/received
5. **Multiple interfaces**: All HID interfaces work simultaneously

**Test structure** (example):
```cpp
// test/test_usb_cdc_hid/test_usb_interface.cpp
#include "usb_badge/usb_cdc.h"
#include "usb_badge/usb_hid.h"

void test_usb_cdc_transmit() {
    usb_cdc_init();
    usb_cdc_start();
    
    const char* msg = "Hello USB CDC";
    usb_cdc_write(msg, strlen(msg));
    
    // Wait for transmission, verify sent
}

void test_usb_hid_keyboard() {
    usb_hid_init();
    
    uint8_t keys[6] = {'H', 'e', 'l', 'l', 'o', 0};
    usb_hid_send_keyboard(0, keys);
    
    // Verify keyboard report sent
}
```

## References
- [usb_cdc implementation](components/usb_badge/usb_cdc.cpp)
- [usb_hid implementation](components/usb_badge/usb_hid.cpp)
- [FIDO2 HID integration](components/mod_fido2/src/Fido2Module.cpp)
