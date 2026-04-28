---
title: "[LOW] PIN System Hardcodes Module-Specific PIN Logic"
severity: LOW
domain: extensibility
lens: pin-management
labels:
  - "audit:architecture/extensibility"
---

## Summary
The `PinManager` in `components/cdc_core/include/cdc_core/PinManager.h` hardcodes PIN logic for specific modules (Badge, FIDO2, OpenPGP PW1/PW3). Adding new modules requiring PIN protection requires modifying the core `PinManager` class.

**Evidence:**
- `components/cdc_core/include/cdc_core/PinManager.h`:
  ```cpp
  class PinManager {
  public:
      // PIN constraints - hardcoded for specific modules
      static constexpr uint8_t BADGE_PIN_MIN = 4;
      static constexpr uint8_t BADGE_PIN_MAX = 8;
      static constexpr uint8_t PW1_MIN = 6;  // OpenPGP User PIN
      static constexpr uint8_t PW3_MIN = 8;  // OpenPGP Admin PIN
      
      // Storage - single slot for all PINs
      static constexpr uint16_t RMEM_SLOT_PIN = 0;
      
      // Defaults - hardcoded per module
      static constexpr const char* DEFAULT_BADGE_PIN = "123456";
      static constexpr const char* DEFAULT_PW1 = "123456";
      static constexpr const char* DEFAULT_PW3 = "12345678";
  };
  ```

- `components/cdc_core/src/PinManager.cpp`: Implementation has module-specific logic:
  ```cpp
  bool PinManager::changeBadgePIN(const char* oldPin, const char* newPin) { ... }
  bool PinManager::changePGPPW1(const char* newPin) { ... }
  bool PinManager::changePGPPW3(const char* newPin) { ... }
  ```

## Impact
**Limited PIN Extensibility:** Adding a new module that needs PIN protection (e.g., password vault, TOTP with PIN) requires:
1. Adding new constants to `PinManager.h`
2. Adding new methods to `PinManager.cpp`
3. Potentially reorganizing R-Memory slot allocation
4. Core firmware rebuild for module-specific PIN logic

The system doesn't support:
- Dynamic PIN registration by modules
- Variable PIN constraints per module
- Multiple PINs per module (e.g., admin/user)

## Evidence
Files affected:
- `components/cdc_core/include/cdc_core/PinManager.h` (PIN definitions)
- `components/cdc_core/src/PinManager.cpp` (PIN logic)
- `components/mod_gpg/src/GpgModule.cpp` (uses PW1/PW3)
- `components/cdc_os_ui/src/views/PinChangeView.cpp` (PIN change UI)

PIN storage in `PinManager.cpp`:
```cpp
// All PINs stored in single R-Memory slot with different offsets
struct PinStorage {
    uint8_t badgePinHash[16];
    uint8_t pw1Hash[32];
    uint8_t pw3Hash[32];
    uint8_t salt[8];
};
```

## Recommended Fix
Implement a pluginable PIN system:

1. **PIN definition struct:**
   ```cpp
   struct PinDefinition {
       const char* name;           // "badge", "gpg_user", "gpg_admin"
       uint8_t minLength;
       uint8_t maxLength;
       uint8_t hashSize;
       uint16_t storageOffset;     // Offset in R-Memory slot
       const char* defaultValue;
   };
   ```

2. **PIN registry:**
   ```cpp
   class PinRegistry {
   public:
       void registerPin(const PinDefinition& def);
       bool validate(const char* name, const char* pin);
       bool change(const char* name, const char* oldPin, const char* newPin);
   };
   ```

3. **Modules register their PINs:**
   ```cpp
   // In GPG module init
   PinRegistry::instance().registerPin({
       "gpg_user", 6, 12, 32, 0, "123456"
   });
   PinRegistry::instance().registerPin({
       "gpg_admin", 8, 12, 32, 32, "12345678"
   });
   ```

This allows modules to define their own PIN requirements without modifying core code.

## References
- Strategy Pattern for PIN validation
- Plugin architecture for security features
- Configurable authentication systems
