---
title: "[MEDIUM] USB Type Definition Duplication and Inconsistency"
severity: MEDIUM
domain: code-consistency
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The USB-related type definitions are duplicated and slightly inconsistent between `cdc_core/UsbManager.h` and `usb_badge/usb_hid.h`:

1. **`UsbHidCallbacks`**: Defined in both files with different signatures
2. **`UsbInterfaceSpec` vs `UsbInterfaceDef`**: Same concept, different names, different initializers
3. **`UsbInterfaceClass`**: Defined in both files

### Evidence

**cdc_core/UsbManager.h:**
```cpp
// File: components/cdc_core/include/cdc_core/UsbManager.h
enum class UsbHidInterface : uint8_t {
    Fido = 0,
    Keyboard = 1,
    Ccid = 2,
};

enum class UsbInterfaceClass : uint8_t {
    Hid = 0,
    Ccid = 1,
};

struct UsbHidCallbacks {
    uint16_t (*onGetReport)(uint8_t report_id, uint8_t report_type,
                            uint8_t* buffer, uint16_t reqlen) = nullptr;
    void (*onSetReport)(uint8_t report_id, uint8_t report_type,
                        uint8_t const* buffer, uint16_t bufsize) = nullptr;
    void (*onReportComplete)(uint8_t const* report, uint16_t len) = nullptr;
};

struct UsbInterfaceSpec {
    UsbInterfaceClass cls = UsbInterfaceClass::Hid;
    const char* name = nullptr;
    const uint8_t* reportDesc = nullptr;
    uint16_t reportDescLen = 0;
    uint8_t protocol = 0;
    bool hasOut = false;
    uint16_t epInSize = 64;
    uint16_t epOutSize = 64;
    UsbHidCallbacks callbacks = {};
};
```

**usb_badge/usb_hid.h:**
```cpp
// File: components/usb_badge/include/usb_badge/usb_hid.h
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
    // No onReportComplete callback here!
};

struct UsbInterfaceDef {
    UsbInterfaceClass cls;
    const char* name;
    const uint8_t* reportDesc;
    uint16_t reportDescLen;
    uint8_t protocol;
    bool hasOut;
    uint16_t epInSize;
    uint16_t epOutSize;
    UsbHidCallbacks callbacks;
};
```

### Key Differences:

1. **Naming**: `UsbInterfaceSpec` (UsbManager) vs `UsbInterfaceDef` (usb_hid)
2. **Default values**: `UsbInterfaceSpec` has default initializers, `UsbInterfaceDef` doesn't
3. **Callbacks**: `UsbHidCallbacks` in `UsbManager.h` includes `onReportComplete`, but `usb_hid.h` doesn't
4. **Callback initialization**: `UsbManager.h` uses `= nullptr` and `= {}`, `usb_hid.h` has no defaults
5. **Duplicate enum**: `UsbInterfaceClass` is defined in both files

## Impact
- **Type confusion**: Developers may use the wrong type depending on which header they include
- **API inconsistency**: Callback signatures differ between the two definitions
- **Maintenance burden**: Changes need to be made in two places
- **Potential bugs**: `UsbInterfaceDef` lacks default initializers, requiring explicit initialization
- **Namespace pollution**: Same type defined in different namespaces

## Recommended Fix

**Consolidate to a single definition in `UsbManager.h`** (the more complete version):

1. **Move types to `UsbManager.h`** (already has the more complete definition):
   - `UsbInterfaceClass`
   - `UsbHidCallbacks`
   - `UsbInterfaceSpec`

2. **Update `usb_hid.h`** to include `UsbManager.h` and use its types:
   ```cpp
   // File: components/usb_badge/include/usb_badge/usb_hid.h
   #include "cdc_core/UsbManager.h"  // Add this

   // Remove duplicate definitions:
   // - enum class UsbInterfaceClass
   // - struct UsbHidCallbacks
   // - struct UsbInterfaceDef

   // Use types from UsbManager.h:
   bool usb_hid_apply_config(const cdc::core::UsbInterfaceSpec* defs, size_t count, bool* needs_replug);
   ```

3. **Update function signatures** in `usb_hid.cpp`:
   ```cpp
   // BEFORE
   bool usb_hid_apply_config(const UsbInterfaceDef* defs, size_t count, bool* needs_replug)

   // AFTER
   bool usb_hid_apply_config(const cdc::core::UsbInterfaceSpec* defs, size_t count, bool* needs_replug)
   ```

4. **Ensure consistent use of `UsbHidCallbacks`**:
   - The `onReportComplete` callback should be available in both places
   - Default initialization (`= {}`) should be used consistently

## Files to Modify

1. `components/usb_badge/include/usb_badge/usb_hid.h` - Remove duplicate types, include `UsbManager.h`
2. `components/usb_badge/src/usb_hid.cpp` - Update to use types from `UsbManager.h`
3. `components/cdc_core/include/cdc_core/UsbManager.h` - Keep as source of truth (already correct)

## References
- C++ Best Practices: Avoid type duplication across headers
- DRY Principle: "Don't Repeat Yourself" - types should be defined once
- Header organization: Shared types should be in a common header

</content>