---
title: "[MEDIUM] Some error log messages lack error codes for debugging"
severity: MEDIUM
domain: observability
lens: structured-logging
labels:
  - "audit:observability/structured-logging"
---

## Summary
Several error log messages in the codebase log "Failed to..." without including the underlying error code or return value. This makes debugging difficult because the root cause of failures is not visible in the logs.

**Files with missing error context:**
- `components/cdc_core/src/AttestationKeyService.cpp` (lines 122, 138, 157)
- `components/mod_totp/src/TotpModule.cpp` (line 58)
- `components/mod_gpg/src/GpgModule.cpp` (line 66)
- `components/cdc_core/src/EventBus.cpp` (line 33)
- And others...

## Impact
1. **Debugging difficulty**: Cannot determine why a function failed without adding more logging or breakpoints.
2. **Production troubleshooting**: When issues occur in the field, error logs without codes require reproducing the issue locally.
3. **Inconsistent error reporting**: Some functions log error codes (e.g., `GCM encrypt failed: %d`), others don't.

## Evidence
Examples of error logs without error codes:

**AttestationKeyService.cpp:122**
```cpp
if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) !=
    hal::SeResult::OK) {
    LOG_E(TAG, "Failed to generate attestation key");  // Missing error code!
}
```

**TotpModule.cpp:58**
```cpp
if (!I18n::registerModule("TOTP", s_i18n_strings, 10)) {
    LOG_E(TAG, "Failed to register i18n strings");  // Missing error code!
}
```

**EventBus.cpp:33**
```cpp
s_eventQueue = xQueueCreate(10, sizeof(Event));
if (s_eventQueue == NULL) {
    LOG_E(TAG, "Failed to create event queue");  // Missing error code!
}
```

Compare with good examples that include error codes:
```cpp
LOG_E(TAG, "GCM encrypt failed: %d", ret);
LOG_E(TAG, "USB PHY init failed: %s", esp_err_to_name(err));
```

## Recommended Fix
Add error codes to all error log messages where a return value or status is available:

1. **AttestationKeyService.cpp** (lines 122, 138, 157):
   ```cpp
   auto result = secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256);
   if (result != hal::SeResult::OK) {
       LOG_E(TAG, "Failed to generate attestation key: %s", seResultToString(result));
   }
   ```

2. **TotpModule.cpp:58**:
   ```cpp
   bool registered = I18n::registerModule("TOTP", s_i18n_strings, 10);
   if (!registered) {
       LOG_E(TAG, "Failed to register i18n strings (count=%zu)", 10);
   }
   ```

3. **EventBus.cpp:33**:
   ```cpp
   s_eventQueue = xQueueCreate(10, sizeof(Event));
   if (s_eventQueue == NULL) {
       LOG_E(TAG, "Failed to create event queue (config: queue_size=10, item_size=%zu)",
             sizeof(Event));
   }
   ```

**Helper function suggestion**: Add a utility to convert common error types to strings:
```cpp
// In cdc_core or cdc_hal
const char* seResultToString(cdc::hal::SeResult result);
```

**Estimated effort:** ~45-60 minutes (find all instances + add error codes + test).

## References
- Good examples in codebase: `components/mod_gpg/src/GpgStorage.cpp`, `components/usb_badge/usb_cdc.cpp`
- ESP error code formatting: `esp_err_to_name()` used correctly in `components/usb_badge/usb_cdc.cpp`

</content>