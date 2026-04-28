---
title: "[MEDIUM] AttestationKeyService and TropicStorage init()/start() return values not checked"
severity: MEDIUM
domain: error-handling
lens: unhandled-return-values
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `main/main.cpp` at lines 178-186, `init()` and `start()` methods for `AttestationKeyService` and `TropicStorage` are called but their return values are not checked. These services are registered with `ServiceRegistry::instance()` regardless of whether initialization succeeded.

**Location:** `main/main.cpp:178-186`
```cpp
s_attestationService.init();
s_attestationService.start();
ServiceRegistry::instance().registerService("attestation_key", &s_attestationService);

auto& tropicStorage = cdc::core::TropicStorage::instance();
tropicStorage.setSecureElement(s_secureElement);
tropicStorage.init();
tropicStorage.start();
ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
```

## Impact
- Services may be registered in `ServiceRegistry` in an uninitialized/failed state
- Later code calling these services may encounter undefined behavior if they assume valid state
- `AttestationKeyService::init()` depends on `s_secureElement` being valid - if SE failed, attestation will fail silently
- `TropicStorage::init()` loads cache from NVS - if this fails, cache operations may behave unexpectedly
- No visibility into initialization failures during boot (no error log)

## Evidence
**main.cpp:178-186**
```cpp
// Attestation Key Service
s_attestationService.setSecureElement(s_secureElement);
s_attestationService.init();  // Return value ignored
s_attestationService.start(); // Return value ignored
ServiceRegistry::instance().registerService("attestation_key", &s_attestationService);

// Tropic Storage
auto& tropicStorage = cdc::core::TropicStorage::instance();
tropicStorage.setSecureElement(s_secureElement);
tropicStorage.init();  // Return value ignored
tropicStorage.start(); // Return value ignored
ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
```

**AttestationKeyService init signature (from header):**
```cpp
bool init() override;
bool start() override;
```

**TropicStorage init signature (from header):**
```cpp
bool init() override;
bool start() override;
```

**Contrast with proper error handling at main.cpp:118-121:**
```cpp
if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
    LOG_I(TAG, "Power Management ready (BQ25895)");
} else {
    LOG_E(TAG, "Power Management init failed!");
}
```

## Recommended Fix
Check return values and log errors for both services:

```cpp
// Attestation Key Service
s_attestationService.setSecureElement(s_secureElement);
if (s_attestationService.init() && s_attestationService.start()) {
    ServiceRegistry::instance().registerService("attestation_key", &s_attestationService);
    LOG_I(TAG, "Attestation Key Service ready");
} else {
    LOG_E(TAG, "Attestation Key Service init failed!");
}

// Tropic Storage
auto& tropicStorage = cdc::core::TropicStorage::instance();
tropicStorage.setSecureElement(s_secureElement);
if (tropicStorage.init() && tropicStorage.start()) {
    ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
    LOG_I(TAG, "TropicStorage cache ready");
} else {
    LOG_E(TAG, "TropicStorage init failed!");
}
```

## References
- Project convention: All other HAL services check return values in main boot sequence
- IService interface defines init/start as returning bool for success/failure indication
