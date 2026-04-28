---
title: "[MEDIUM] GpgStorage C API lacks error codes and return value semantics"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `GpgStorage` C API (components/mod_gpg/include/mod_gpg/GpgStorage.h) defines functions that return `bool` for success/failure but don't provide error codes or detailed failure reasons:

```c
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin);
bool gpg_storage_load_dec_privkey(uint8_t* privkey_out, const char* pin);
bool gpg_storage_has_dec_privkey(void);
bool gpg_storage_delete_dec_privkey(void);
```

Consumers (e.g., `GpgModule.cpp:580`) can only check success/failure without knowing WHY a operation failed:
```cpp
if (!gpg_storage_save_dec_privkey(...)) {
    // No way to know if it was: wrong PIN, slot error, encryption failure, etc.
    core::ModuleRegistry::instance().reportModuleError(getName(), "GPG init failed");
}
```

## Impact
- **Debugging difficulty**: Cannot distinguish between different failure modes
- **Poor error handling**: Consumers must use generic error messages
- **Security**: Cannot differentiate between transient errors and permanent failures
- **API evolution**: Adding new error types later requires breaking changes

## Evidence
- API definition: components/mod_gpg/include/mod_gpg/GpgStorage.h:41-67
- Usage in GpgModule: components/mod_gpg/src/GpgModule.cpp:580
- Return type: `bool` (true/false only)

## Recommended Fix
Replace `bool` returns with error code enum:

1. **Define error codes** (components/mod_gpg/include/mod_gpg/GpgStorage.h):
```c
typedef enum {
    GPG_STORAGE_OK = 0,
    GPG_STORAGE_ERR_NOT_FOUND = 1,
    GPG_STORAGE_ERR_INVALID_PIN = 2,
    GPG_STORAGE_ERR_SLOT_EMPTY = 3,
    GPG_STORAGE_ERR_SLOT_FULL = 4,
    GPG_STORAGE_ERR_ENCRYPTION = 5,
    GPG_STORAGE_ERR_DECRYPTION = 6,
    GPG_STORAGE_ERR_INVALID_PARAM = 7,
    GPG_STORAGE_ERR_NOT_INITIALIZED = 8,
} gpg_storage_result_t;
```

2. **Update function signatures**:
```c
gpg_storage_result_t gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin);
gpg_storage_result_t gpg_storage_load_dec_privkey(uint8_t* privkey_out, const char* pin);
```

3. **Update consumers** (components/mod_gpg/src/GpgModule.cpp):
```cpp
auto result = gpg_storage_save_dec_privkey(...);
if (result != GPG_STORAGE_OK) {
    const char* msg = get_gpg_storage_error_string(result);
    core::ModuleRegistry::instance().reportModuleError(getName(), msg);
}
```

## References
- GpgStorage API: components/mod_gpg/include/mod_gpg/GpgStorage.h
- Error handling pattern: components/cdc_core/IService.h (ServiceState enum)
