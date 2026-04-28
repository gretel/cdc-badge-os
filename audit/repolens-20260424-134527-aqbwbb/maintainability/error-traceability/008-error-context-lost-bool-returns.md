---
title: "[MEDIUM] Error context lost when converting specific errors to generic booleans"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
Many functions convert specific error codes (e.g., `esp_err_t`, `lt_ret_t`, `SeResult`) to generic `bool` returns, losing the specific error information. When these functions are called, callers cannot determine *why* a function failed, only *that* it failed.

## Impact
- **Lost error specificity**: A `false` return could mean 10+ different failure reasons
- **Debugging difficulty**: Cannot distinguish between transient errors (retryable) and permanent errors (fatal)
- **Poor error propagation**: Root cause is hidden behind generic boolean returns
- **Inability to classify errors**: No way to know if error is hardware, system, or user-related

## Evidence

### Pattern 1: Functions returning bool instead of error code

`components/cdc_hal/src/Tropic01Element.cpp:140-155`:
```cpp
bool Tropic01Element::init() {
    // ...
    psa_status_t psaStatus = psa_crypto_init();
    if (psaStatus != PSA_SUCCESS && psaStatus != PSA_ERROR_BAD_STATE) {
        LOG_E(TAG, "PSA crypto init failed (status=%ld)", (long)psaStatus);
        state_ = core::ServiceState::ERROR;
        return false;  // loses psaStatus value
    }

    mutex_ = xSemaphoreCreateRecursiveMutex();
    if (!mutex_) {
        LOG_E(TAG, "Failed to create mutex");
        state_ = core::ServiceState::EERROR;
        return false;  // no specific error code
    }

    lt_ret_t ret = lt_init();
    if (ret != LT_OK) {
        LOG_E(TAG, "lt_init failed (%s)", lt_ret_verbose(ret));
        state_ = core::ServiceState::ERROR;
        return false;  // loses lt_ret_t value
    }
    return true;
}
```

**Problem**: The specific error codes (`psa_status_t`, `lt_ret_t`) are logged but only `bool` is returned. Callers cannot programmatically distinguish between:
- PSA crypto failure
- Mutex creation failure  
- libtropic init failure

### Pattern 2: Session management losing error context

`components/cdc_hal/src/Tropic01Element.cpp:200-215`:
```cpp
bool Tropic01Element::sessionStart() {
    lock();
    lt_ret_t ret = lt_secure_session_start();
    if (ret != LT_OK) {
        LOG_E(TAG, "Secure session failed (%s)", lt_ret_verbose(ret));
        sessionActive_ = false;
        unlock();
        return false;  // loses specific session error
    }
    sessionActive_ = true;
    unlock();
    return true;
}
```

**Problem**: Session failures could be:
- Timeout
- Hardware error
- Protocol mismatch
- Pairing issue

All return `false` with no way for caller to know which.

### Pattern 3: Boolean validation functions with no error detail

`components/cdc_hal/src/Tropic01Element.cpp:280-295`:
```cpp
bool Tropic01Element::validateHeader(const RMemHeader& header) const {
    if (header.magic != RMEM_HEADER_MAGIC) {
        return false;  // why? magic mismatch?
    }
    if (header.checksum != computeHeaderChecksum(header)) {
        return false;  // why? checksum error?
    }
    return true;
}
```

**Problem**: Two different validation failures, both return `false`.

### Pattern 4: Generic error returns in storage layer

`components/mod_totp/src/TotpStore.cpp`:
```cpp
bool TotpStore::saveAccount(const char* name, const char* secretBase32,
                            uint8_t digits, uint32_t period) {
    if (!name || !secretBase32) return false;  // null pointer?
    if (!hasSlotRange_) return false;  // no slots?
    
    // ...
    
    SeResult result = se_->rmemWrite(slot, data, len);
    if (result != SeResult::OK) {
        LOG_E(TAG, "Failed to save account to R-Memory");
        return false;  // loses SeResult value
    }
    return true;
}
```

**Problem**: `SeResult` is converted to `bool`, losing:
- `SE_ERR_SLOT_FULL`
- `SE_ERR_BUSY`
- `SE_ERR_INVALID_PARAM`
- `SE_ERR_TIMEOUT`

### Pattern 5: Error-to-boolean conversion in module registry

`components/cdc_core/src/ModuleRegistry.cpp:178-198`:
```cpp
bool ModuleRegistry::startModule(uint8_t index) {
    if (index >= count_) return false;
    IModule* module = modules_[index];
    if (!module) return false;

    if (hasModuleSlotError(index)) {
        LOG_E(TAG, "Module '%s' blocked: %s", module->getName(),
              getModuleSlotError(index) ? getModuleSlotError(index) : "slot map error");
        return false;  // slot error detail exists but not returned
    }

    if (module->getState() == ServiceState::UNINITIALIZED) {
        if (!module->init()) {
            LOG_E(TAG, "Failed to init module '%s'", module->getName());
            return false;  // loses module-specific error
        }
    }

    if (!module->start()) {
        LOG_E(TAG, "Failed to start module '%s'", module->getName());
        return false;  // loses module-specific error
    }

    return true;
}
```

**Problem**: Module start errors have different causes:
- Slot allocation failure
- Hardware initialization failure
- Memory allocation failure
- Configuration error

All return `false`.

## Recommended Fix

### Fix 1: Return error codes instead of bool

```cpp
// Instead of:
bool sessionStart();

// Use:
SeResult sessionStart();  // or esp_err_t, or lt_ret_t
```

**Example for Tropic01Element**:
```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h
class ISecureElement {
public:
    // Change from bool to SeResult
    virtual SeResult sessionStart();
    virtual SeResult init();
    virtual SeResult start();
};
```

### Fix 2: Create error-result wrapper for bool operations

For operations that truly return boolean (validation, checks):

```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h
struct ValidationResult {
    bool valid;
    SeResult error;  // SeResult::OK if valid
};

// Instead of:
virtual bool validateHeader(const RMemHeader& header) const;

// Use:
virtual ValidationResult validateHeader(const RMemHeader& header) const;
```

### Fix 3: Use SeResult consistently

`components/cdc_hal/include/cdc_hal/ISecureElement.h`:
```cpp
typedef enum {
    SE_OK = 0,
    SE_ERR_GENERAL = 1,
    SE_ERR_INVALID_PARAM = 2,
    SE_ERR_SLOT_FULL = 3,
    SE_ERR_TIMEOUT = 4,
    SE_ERR_BUSY = 5,
    SE_ERR_HW = 6,
    SE_ERR_NOT_FOUND = 7,
    SE_ERR_NOT_SUPPORTED = 8
} SeResult;

// Update all boolean methods to return SeResult
```

### Fix 4: Preserve error context in call chains

```cpp
// Good: Error propagates with context
SeResult Tropic01Element::sessionStart() {
    lt_ret_t ret = lt_secure_session_start();
    if (ret != LT_OK) {
        return mapResult(ret);  // preserves error code
    }
    return SE_OK;
}

// Bad: Error lost
bool Tropic01Element::sessionStart() {
    lt_ret_t ret = lt_secure_session_start();
    if (ret != LT_OK) {
        return false;  // lost!
    }
    return true;
}
```

### Fix 5: Add error detail to logging when bool is unavoidable

```cpp
// If bool must be returned, log with detail before returning
bool moduleStart() {
    SeResult result = doStart();
    if (result != SeResult::OK) {
        LOG_E(TAG, "Module start failed: %s", seResultToString(result));
        // Still log the detail
        return false;
    }
    return true;
}
```

## References
- `components/cdc_hal/include/cdc_hal/ISecureElement.h` - SeResult definition
- `components/cdc_hal/src/Tropic01Element.cpp` - Current implementation
- `components/mod_totp/src/TotpStore.cpp` - Boolean conversion example
- `components/cdc_core/src/ModuleRegistry.cpp` - Error propagation pattern

</content>