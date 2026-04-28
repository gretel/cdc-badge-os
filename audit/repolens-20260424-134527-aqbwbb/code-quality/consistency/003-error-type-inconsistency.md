---
title: "[MEDIUM] Error Type Inconsistency: Mixed esp_err_t and SeResult usage"
severity: MEDIUM
domain: error-handling
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "error-handling"
---

## Summary

The codebase uses two different error type systems inconsistently:

1. **ESP-IDF native**: `esp_err_t` (from ESP-IDF framework)
2. **Custom HAL enum**: `cdc::hal::SeResult` (secure element specific)

**Current usage patterns:**
- **HAL interfaces** (`II2cBus.h`, `ISpiBus.h`): Use `esp_err_t`
- **Secure element operations**: Use `cdc::hal::SeResult`
- **Mixed usage in same file**: Some functions return `esp_err_t`, others return `bool`

**Evidence:**
```cpp
// components/cdc_hal/include/cdc_hal/II2cBus.h - Using esp_err_t
virtual esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* out_dev) = 0;
virtual esp_err_t writeReg(I2cDeviceHandle dev, uint8_t reg, uint8_t val) = 0;

// components/cdc_hal/include/cdc_hal/ISpiBus.h - Using esp_err_t
esp_err_t initSharedSpiBus();

// components/cdc_core/src/AttestationKeyService.cpp - Using SeResult
if (res == hal::SeResult::SLOT_EMPTY) { ... }
if (res != hal::SeResult::OK) return false;

// components/mod_totp/src/TotpStore.cpp - Using SeResult
if (res != cdc::hal::SeResult::OK) { ... }

// Components using bool for simple success/fail
if (!se->eccGetPublicKey(slot, pubkey, &curve) == cdc::hal::SeResult::OK) { ... }
```

## Impact

1. **API confusion**: Developers unsure which error type to use for new interfaces
2. **Conversion overhead**: Need to convert between `esp_err_t` and `SeResult`
3. **Inconsistent error handling**: Some functions return `bool`, others return error codes
4. **Documentation burden**: Need to document which error system each API uses

## Evidence

**HAL interfaces using esp_err_t:**
```cpp
// components/cdc_hal/include/cdc_hal/II2cBus.h
virtual esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* out_dev) = 0;
virtual esp_err_t writeReg(I2cDeviceHandle dev, uint8_t reg, uint8_t val) = 0;
virtual esp_err_t readReg(I2cDeviceHandle dev, uint8_t reg, uint8_t* val) = 0;

// components/cdc_hal/include/cdc_hal/ISpiBus.h
esp_err_t initSharedSpiBus();
```

**Secure element using SeResult:**
```cpp
// components/cdc_core/src/AttestationKeyService.cpp
if (res == hal::SeResult::SLOT_EMPTY) {
    LOG_I(TAG, "Attestation slot empty, generating key");
}
if (res != hal::SeResult::OK) return false;
```

**Namespace inconsistency:**
- `cdc::hal::SeResult` (fully qualified)
- `hal::SeResult` (partial qualification)
- Both used in same file (AttestationKeyService.cpp)

## Recommended Fix

**Option 1: Standardize on esp_err_t for all HAL interfaces**

1. Create mapping in `cdc_hal`:
   ```cpp
   // components/cdc_hal/include/cdc_hal/SeResult.h
   namespace cdc::hal {
   enum class SeResult {
       OK = ESP_OK,
       FAIL = ESP_FAIL,
       // ... other mappings
   };
   
   inline esp_err_t toEspErr(SeResult res) {
       return static_cast<esp_err_t>(res);
   }
   }
   ```

2. Update all HAL interfaces to return `esp_err_t`

**Option 2: Standardize on SeResult for all operations**

1. Create conversion utilities:
   ```cpp
   // components/cdc_hal/include/cdc_hal/ErrorTypes.h
   namespace cdc::hal {
   inline SeResult fromEspErr(esp_err_t err) {
       return err == ESP_OK ? SeResult::OK : SeResult::FAIL;
   }
   }
   ```

2. Update HAL interfaces to return `SeResult`

**Immediate fix (can be done in ~1 hour):**

1. Fix namespace consistency:
   ```bash
   # Standardize on cdc::hal::SeResult
   grep -r "hal::SeResult" components/ --include="*.cpp" | grep -v "cdc::hal::SeResult"
   ```

2. Update AttestationKeyService.cpp:
   ```cpp
   // Replace all hal::SeResult with cdc::hal::SeResult
   ```

## References

- `components/cdc_hal/include/cdc_hal/II2cBus.h`
- `components/cdc_hal/include/cdc_hal/ISpiBus.h`
- `components/cdc_core/src/AttestationKeyService.cpp`
- `components/cdc_core/include/cdc_core/ISecureElement.h`
