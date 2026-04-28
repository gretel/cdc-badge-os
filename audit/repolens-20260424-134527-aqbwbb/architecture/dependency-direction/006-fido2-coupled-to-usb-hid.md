---
title: "[MEDIUM] FIDO2 module coupled to USB HID implementation"
severity: MEDIUM
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
---

## Summary

The FIDO2 module (`mod_fido2`) directly depends on `usb_badge/usb_hid.h` for transport, creating tight coupling to the USB HID implementation instead of using an abstraction.

**Affected file:**
- `components/mod_fido2/src/Fido2Module.cpp:9` - `#include "usb_badge/usb_hid.h"`
- `components/mod_fido2/src/Fido2Module.cpp:256,282` - Direct calls to `usb_hid_instance_ready()`, `usb_hid_send_report()`

**Evidence:**
```cpp
// components/mod_fido2/src/Fido2Module.cpp
#include "usb_badge/usb_hid.h"

bool fido2_usb_ready() {
    return usb_hid_instance_ready(s_hid_instance);  // Direct USB call
}

bool fido2_usb_write(const uint8_t* buffer) {
    return usb_hid_send_report(s_hid_instance, 0, buffer, CTAPHID_PACKET_SIZE);  // Direct USB call
}
```

## Impact

1. **Transport coupling**: FIDO2 is tied to USB HID transport
2. **Limited flexibility**: Hard to add alternative transports (e.g., BLE HID)
3. **Testing difficulty**: Requires USB subsystem for testing

## Evidence

**In `components/mod_fido2/src/Fido2Module.cpp:250-285`:**
```cpp
bool fido2_usb_ready() {
    return usb_hid_instance_ready(s_hid_instance);
}

uint16_t fido2_usb_read(uint8_t* buffer) {
    if (!s_rx_queue || !buffer) return 0;
    FidoPacket pkt;
    if (xQueueReceive(s_rx_queue, &pkt, 0) == pdTRUE) {
        memcpy(buffer, pkt.data, CTAPHID_PACKET_SIZE);
        return CTAPHID_PACKET_SIZE;
    }
    return 0;
}

bool fido2_usb_write(const uint8_t* buffer) {
    if (!buffer) return false;
    return usb_hid_send_report(s_hid_instance, 0, buffer, CTAPHID_PACKET_SIZE);
}
```

## Recommended Fix

1. Create `IFidoTransport` interface in `mod_fido2` or `cdc_core`
2. Implement `UsbHidTransport` that wraps `usb_badge/usb_hid.h`
3. FIDO2 core depends on `IFidoTransport` interface

**Scope estimate:** 1 hour

## References

- Dependency Inversion: FIDO2 business logic should not depend on USB infrastructure
- Transport abstraction: Similar to `IKeyboardProvider` pattern
