---
title: "[MEDIUM] Hardcoded USB HID Interface Types Require Core Modification"
severity: MEDIUM
domain: architecture/extensibility
lens: extensibility-plugin
labels:
  - "switch-statements"
  - "open-closed-principle"
  - "usb-hid"
---

## Summary

The USB HID interface system uses a hardcoded `enum class UsbHidInterface` with only 3 predefined types (Fido, Keyboard, Ccid). Adding a new HID interface type (e.g., MIDI, Raw HID, Vendor-specific) requires:

1. Modifying `UsbHidInterface` enum in `components/cdc_core/include/cdc_core/UsbManager.h`
2. Adding a new entry in the `applyConfiguration()` switch at `components/cdc_core/src/UsbManager.cpp:144-146`
3. Potentially modifying `usb_hid_apply_config()` in `components/usb_badge/usb_hid.cpp`

This violates the Open/Closed Principle - the core USB manager is not closed to modification when new interface types are added.

**Evidence:**

`components/cdc_core/include/cdc_core/UsbManager.h:8-11`:
```cpp
enum class UsbHidInterface : uint8_t {
    Fido = 0,
    Keyboard = 1,
    Ccid = 2,
};
```

`components/cdc_core/src/UsbManager.cpp:144-146`:
```cpp
append_def(UsbHidInterface::Fido);
append_def(UsbHidInterface::Keyboard);
append_def(UsbHidInterface::Ccid);
```

The `UsbManager` has a fixed array `entries_[3]` sized exactly to the current interface count, with no room for expansion without code changes.

## Impact

**Maintenance Burden:** Every new HID interface type requires touching core code, increasing risk of regressions in existing interfaces.

**Module Coupling:** Modules that want to use a new HID type must depend on core modifications, breaking the module isolation pattern established elsewhere in the system.

**Scalability Limit:** The current design supports exactly 3 HID types. Adding a 4th requires:
- Changing the enum
- Resizing the `entries_[3]` array
- Adding a new `append_def()` call
- Potentially updating USB descriptor generation

## Recommended Fix

Implement a plugin-style HID interface registration system:

1. **Replace enum with dynamic registration:**
   ```cpp
   class UsbManager {
   public:
       using InterfaceId = uint8_t;
       InterfaceId registerInterfaceType(const char* name);
       bool registerInstance(InterfaceId id, const UsbInterfaceSpec& def);
   };
   ```

2. **Use std::vector or fixed-size array with runtime count:**
   ```cpp
   static constexpr uint8_t MAX_HID_TYPES = 8;  // Configurable
   InterfaceEntry entries_[MAX_HID_TYPES] = {};
   uint8_t typeCount_ = 0;
   ```

3. **Iterate over registered types in applyConfiguration:**
   ```cpp
   bool UsbManager::applyConfiguration() {
       ::UsbInterfaceDef defs[MAX_HID_TYPES] = {};
       size_t count = 0;
       for (uint8_t i = 0; i < typeCount_ && count < MAX_HID_TYPES; i++) {
           if (entries_[i].active) {
               defs[count++] = buildDef(entries_[i]);
           }
       }
       return usb_hid_apply_config(defs, count, &needsReplug_);
   }
   ```

4. **Module registers its interface type at init:**
   ```cpp
   // In module init
   static UsbManager::InterfaceId s_hidType;
   if (s_hidType == 0) {
       s_hidType = UsbManager::instance().registerInterfaceType("MyModule");
   }
   UsbManager::instance().registerInstance(s_hidType, spec);
   ```

**Scope:** This fix can be done in ~1 hour by modifying `UsbManager.h` and `UsbManager.cpp` to replace the enum-based approach with dynamic registration.

## References

- Open/Closed Principle: https://en.wikipedia.org/wiki/Open%E2%80%93closed_principle
- Strategy Pattern: https://refactoring.guru/design-patterns/strategy
- Current implementation: `components/cdc_core/include/cdc_core/UsbManager.h:8-11`
