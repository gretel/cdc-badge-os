---
title: "[013] [MEDIUM] Unclear function signatures with multiple boolean flags"
severity: MEDIUM
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
Several functions use multiple boolean or integer flags as parameters that are not self-explanatory at the call site.

## Impact
- Callers must jump to the function definition to understand what each parameter means
- Easy to mix up parameter order
- Refactoring becomes risky as parameter changes affect all call sites

## Evidence
**File: `components/mod_gpg/src/GpgStorage.cpp:132-159`**
```cpp
static bool derive_key_from_pin(const char* pin, uint8_t* key_out) {
    if (!key_out) return false;

    // If no PIN provided, use device-specific key
    if (!pin || pin[0] == '\0') {
        return derive_device_key(key_out);
    }

    // Get chip ID as salt (unique per device)
    uint8_t salt[16] = {};
    auto* se = get_se();
    if (se) {
        se->getChipId(salt, sizeof(salt));
    }

    // HKDF: PIN -> 32-byte key
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;

    int ret = mbedtls_hkdf(
        md,
        salt, sizeof(salt),
        reinterpret_cast<const uint8_t*>(pin), strlen(pin),
        reinterpret_cast<const uint8_t*>(HKDF_INFO), strlen(HKDF_INFO),
        key_out, 32
    );

    return ret == 0;
}
```

Call sites at lines 246 and 342:
```cpp
if (!derive_key_from_pin(pin, enc_key)) {  // What does 'pin' mean here?
    LOG_E(TAG, "Failed to derive encryption key");
    return false;
}
```

The `pin` parameter can be `nullptr` - this is documented in a comment but not in the signature.

**File: `components/cdc_hal/src/BleAdvParser.cpp:81-106`**
```cpp
bool findManufacturerData(const uint8_t* advData, uint8_t len,
                          uint16_t* companyId, const uint8_t** data, uint8_t* dataLen) {
    if (companyId == nullptr || data == nullptr || dataLen == nullptr) {
        return false;
    }
    // ...
}
```

Five parameters with two output pointers makes the function signature dense and hard to read.

## Recommended Fix
Use a struct for complex parameter groups or use an options pattern:

**Option 1: Struct for output parameters**
```cpp
struct ManufacturerData {
    uint16_t companyId;
    const uint8_t* data;
    uint8_t dataLen;
};

bool findManufacturerData(const uint8_t* advData, uint8_t len, ManufacturerData* out);
```

**Option 2: Options struct for optional parameters**
```cpp
struct DeriveKeyOptions {
    const char* pin;  // Can be nullptr for device key
    uint8_t* key_out;
};

bool derive_key_from_pin(DeriveKeyOptions opts);
```

**Option 3: Explicit naming**
```cpp
bool deriveEncryptionKeyFromPin(const char* pin, uint8_t* key_out);
bool deriveEncryptionKeyFromDevice(uint8_t* key_out);
```

## References
- Clean Code: "Functions should have few arguments" - Robert C. Martin
- C++ Core Guidelines: F.6 - Use explicit type conversions for clarity
