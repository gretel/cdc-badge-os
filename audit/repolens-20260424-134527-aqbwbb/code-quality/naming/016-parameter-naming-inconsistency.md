---
title: "[MEDIUM] Parameter naming inconsistency: camelCase vs snake_case"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses different naming conventions for function parameters. Some functions use `camelCase` (e.g., `currentPin`, `newPin`) while others use `snake_case` (e.g., `out_dev`, `max_len`).

**Evidence:**

1. **components/cdc_core/include/cdc_core/PinManager.h** (lines 60-68):
   ```cpp
   bool verifyBadgePin(const char* pin);
   bool changeBadgePin(const char* currentPin, const char* newPin);
   bool setBadgePin(const char* newPin);
   bool getBadgePinHash(uint8_t* hashOut) const;
   bool verifyBadgePinHash(const uint8_t* hashIn) const;
   ```
   Uses `camelCase`: `currentPin`, `newPin`, `hashOut`, `hashIn`

2. **components/cdc_hal/include/cdc_hal/II2cBus.h** (lines 20-28):
   ```cpp
   virtual esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* out_dev);
   virtual esp_err_t writeReg(I2cDeviceHandle dev, uint8_t reg,
                              const uint8_t* data, size_t len);
   virtual esp_err_t readReg(I2cDeviceHandle dev, uint8_t reg,
                             uint8_t* data, size_t len);
   ```
   Uses `snake_case`: `out_dev`

3. **components/mod_vcard/include/mod_vcard/vcard_store.h** (lines 9-21):
   ```cpp
   bool vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len);
   size_t vcard_store_get_own(char* out, size_t max_len);
   bool vcard_store_delete(uint16_t slot);
   uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots);
   ```
   Uses `snake_case`: `err_len`, `out_slots`, `max_len`

4. **components/cdc_core/include/cdc_core/TropicStorage.h**:
   ```cpp
   bool read(uint16_t startSlot, uint16_t count, TropicSlotMap* out_map);
   ```
   Uses `camelCase`: `startSlot`, `out_map` (mixed!)

## Impact
- **Inconsistency**: Developers need to remember which convention applies where
- **Visual noise**: Mixing conventions creates cognitive friction
- **Style drift**: New code may accidentally use the "wrong" convention

## Evidence
Side-by-side comparison:
- `changeBadgePin(const char* currentPin, const char* newPin)` - camelCase
- `addDevice(uint8_t addr, I2cDeviceHandle* out_dev)` - snake_case for output
- `vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len)` - snake_case
- `read(uint16_t startSlot, uint16_t count, TropicSlotMap* out_map)` - mixed!

## Recommended Fix
Choose ONE convention for all parameters and apply consistently:

**Option A (Recommended for C++): Use camelCase**
```cpp
virtual esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* outDev);
bool vcardStoreSetOwn(const char* vcard, size_t len, char* err, size_t errLen);
bool read(uint16_t startSlot, uint16_t count, TropicSlotMap* outMap);
```

**Option B: Use snake_case**
```cpp
bool changeBadgePin(const char* current_pin, const char* new_pin);
bool verifyBadgePin(const char* pin);
```

**Steps:**
1. Choose one convention (camelCase recommended for C++ code)
2. Update all parameter names in header files
3. Update all parameter names in implementation files
4. Update all call sites to use consistent names

## References
- [Google C++ Style Guide - Function Arguments](https://google.github.io/styleguide/cppguide.html#Function_Arguments)
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)
