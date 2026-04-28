---
title: "[MEDIUM] Mutable static descriptor arrays that should be const"
severity: MEDIUM
domain: code-quality
lens: immutability
labels:
  - "audit:code-quality/immutability"
---

## Summary
Several static arrays containing descriptor data or configuration are declared as mutable when they should be `const` or `constexpr` since they are initialized once and never modified. This is particularly important for USB descriptors, HID report descriptors, and other fixed data structures.

**Files affected:**
- `components/usb_badge/usb_hid.cpp` (lines 52-65): `device_descriptor` - USB device descriptor
- `components/mod_fido2/src/Fido2Module.cpp` (lines 20-37): `s_fido_report_desc` - HID report descriptor (already const, good example)
- `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 120-128): `s_openpgp_aid` - Application ID array
- `components/cdc_ui/src/I18n.cpp` (lines 201-280): String translation tables

## Impact
- **Memory placement**: Non-const arrays may be placed in RAM instead of flash, wasting precious SRAM on embedded systems
- **Accidental modification**: Mutable arrays can be accidentally modified, leading to subtle bugs
- **Compiler optimization**: `const` arrays allow the compiler to optimize better and potentially deduplicate identical values
- **Code clarity**: `const` makes intent explicit - readers know the data won't change

## Evidence
Example from `components/usb_badge/usb_hid.cpp:52`:
```cpp
static tusb_desc_device_t device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = TUSB_CLASS_MISC,
    // ... rest of fields
};
```
This descriptor is initialized once and never modified, but is declared as mutable.

Example from `components/mod_gpg/src/openpgp/openpgp.cpp:120`:
```cpp
static uint8_t s_openpgp_aid[16] = {
    0xD2, 0x76, 0x00, 0x01, 0x24, 0x01,  // RID + Application (OpenPGP)
    0x03, 0x04,                           // Version 3.4
    0x00, 0x00,                           // Manufacturer (set in init)
    0x00, 0x00, 0x00, 0x00,              // Serial number (set in init from MAC)
    0x00, 0x00                            // RFU
};
```
Note: This one is actually modified during initialization (manufacturer and serial), so it needs to remain mutable but should be reviewed for encapsulation.

Good example from `components/mod_fido2/src/Fido2Module.cpp:20`:
```cpp
static const uint8_t s_fido_report_desc[] = {
    0x06, 0xD0, 0xF1,  // Usage Page (FIDO Alliance)
    // ...
};
```
This is correctly declared as `const`.

## Recommended Fix
For truly immutable data, add `const` or `constexpr`:

1. **USB device descriptor** (`components/usb_badge/usb_hid.cpp:52`):
   ```cpp
   // If never modified after initialization:
   static constexpr tusb_desc_device_t device_descriptor = { ... };
   
   // Or if fields are set during init:
   static tusb_desc_device_t device_descriptor;  // Keep mutable but document intent
   ```

2. **Review `s_openpgp_aid`** (`components/mod_gpg/src/openpgp/openpgp.cpp:120`):
   - If the manufacturer and serial are set once during init and never change, consider using a separate init function that populates a const array
   - Or use a getter function with static const local

3. **String translation tables** (`components/cdc_ui/src/I18n.cpp:201`):
   ```cpp
   // Check if these are stored as pointers to string literals
   // If so, ensure the arrays themselves are const
   static const char* core_strings[] = { ... };
   // or
   static constexpr const char* core_strings[] = { ... };
   ```

## References
- C++ Core Guidelines [C.24](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c24-use-typedefs-or-using-for-constants-not-define): Use `const` or `constexpr` for constants
- Embedded C coding standards (MISRA C++) recommend `const` for read-only data
- ESP32 memory architecture: const data can be placed in flash (IRAM/DRAM for frequently accessed)
