---
title: "[MEDIUM] USB HID Configuration Has No Version for Host Compatibility"
severity: MEDIUM
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The USB HID configuration API (`components/usb_badge/include/usb_badge/usb_hid.h`) has no version field. When interface definitions change, hosts cannot detect compatibility or require re-enumeration.

**Evidence:**
- `components/usb_badge/include/usb_badge/usb_hid.h:43-48` - `UsbInterfaceDef` struct has no version
- `components/usb_badge/include/usb_badge/usb_hid.h:54` - `usb_hid_apply_config()` returns `needs_replug` but no version info
- `docs/SERIAL_COMMANDS.md` - No USB configuration version documented

## Impact
- Hosts cannot cache correct descriptors for future connections
- Breaking changes to HID report descriptors require manual replug
- No way to advertise capability versions to host software

## Evidence
Current interface definition:
```cpp
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
```

No version field, no capability flags.

## Recommended Fix
Add version and capability tracking:

1. Add version to interface definition:
```cpp
struct UsbInterfaceDef {
    UsbInterfaceClass cls;
    const char* name;
    uint8_t version;           // Interface version (for compatibility)
    uint32_t capabilities;     // Bitfield of supported features
    
    const uint8_t* reportDesc;
    uint16_t reportDescLen;
    uint8_t protocol;
    bool hasOut;
    uint16_t epInSize;
    uint16_t epOutSize;
    UsbHidCallbacks callbacks;
};
```

2. Add USB version query command:
```cpp
// Add to serial commands
{ "USB_CONFIG", "Get USB configuration version", cmdUsbConfig, "system", false }
```

3. Update `usb_cdc.h` with version query:
```cpp
/**
 * Get USB configuration version
 * @return Version string (e.g., "1.0.0")
 */
const char* usb_cdc_get_version(void);
```

## References
- USB HID Specification: https://usb.org/hid
- USB Descriptors: https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-descriptors
