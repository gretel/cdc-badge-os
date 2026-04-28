---
title: "[MEDIUM] No error classification taxonomy for retryability and severity"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The codebase lacks a consistent error classification system to distinguish between transient (retryable) and permanent (fatal) errors, or to categorize errors by type for trend analysis and prioritization.

## Impact
- **No retry strategy**: Cannot programmatically determine which errors should be retried
- **Poor error prioritization**: All errors treated equally, no severity levels beyond logging
- **Difficult trend analysis**: Cannot group related errors for pattern detection
- **Limited automation**: Error monitoring systems cannot automatically categorize or route errors

## Evidence

### Current error handling patterns:

**CTAPHID has error codes but no classification:**
`components/mod_fido2/include/mod_fido2/ctaphid.h`:
```cpp
#define CTAPHID_ERR_INVALID_CMD     0x01
#define CTAPHID_ERR_INVALID_PAR     0x02
#define CTAPHID_ERR_INVALID_LEN     0x03
// ... etc
```
These codes exist but are not classified by retryability or severity.

**Module errors reported without classification:**
`components/cdc_core/src/ModuleRegistry.cpp`:
```cpp
void reportModuleError(const char* moduleName, const char* errorType) {
    // errorType is just a string, no structured classification
    snprintf(buffer, bufSize, "%s for %s", errorType, mapName);
}
```

**Generic retry logic without error categorization:**
`components/cdc_core/src/ModuleRegistry.cpp`:
```cpp
bool ModuleRegistry::retryModule(uint8_t index) {
    // No check if error is retryable - just tries again
    LOG_I(TAG, "Retrying module '%s'...", name);
    // ...
}
```

**No error taxonomy defined:**
- No enum or struct for error categories (transient, permanent, user-error, system-error, hardware-error)
- No severity levels (info, warning, critical, fatal)
- No retryability flags

## Recommended Fix

1. **Define error classification structure:**

   ```cpp
   // components/cdc_core/include/cdc_core/ErrorCode.h
   typedef enum {
       ERROR_TYPE_TRANSIENT,      // Retryable (timeout, busy)
       ERROR_TYPE_PERMANENT,      // Fatal (invalid config, slot full)
       ERROR_TYPE_USER,           // User input error
       ERROR_TYPE_SYSTEM,         // Memory, storage
       ERROR_TYPE_HARDWARE        // I2C, SPI, secure element
   } error_type_t;
   
   typedef enum {
       ERROR_SEV_INFO,
       ERROR_SEV_WARNING,
       ERROR_SEV_CRITICAL,
       ERROR_SEV_FATAL
   } error_severity_t;
   
   typedef struct {
       uint16_t code;
       const char* name;
       error_type_t type;
       error_severity_t severity;
       bool retryable;
   } error_definition_t;
   ```

2. **Create error registry:**

   ```cpp
   // Error catalog for all modules
   static const error_definition_t error_catalog[] = {
       {1001, "TIMEOUT", ERROR_TYPE_TRANSIENT, ERROR_SEV_WARNING, true},
       {1002, "SLOT_FULL", ERROR_TYPE_PERMANENT, ERROR_SEV_CRITICAL, false},
       {1003, "INVALID_INPUT", ERROR_TYPE_USER, ERROR_SEV_INFO, false},
       // ...
   };
   ```

3. **Update error reporting:**

   ```cpp
   // Instead of:
   LOG_E(TAG, "Operation failed");
   
   // Use:
   reportError(ERROR_CODE_TIMEOUT, "FIDO2 authentication timeout");
   ```

4. **Implement retry logic based on classification:**

   ```cpp
   bool shouldRetry(error_code_t code) {
       const error_definition_t* def = getErrorDefinition(code);
       return def && def->retryable;
   }
   ```

## References
- [CTAP2 Error Codes](components/mod_fido2/include/mod_fido2/ctap2.h) - Existing error codes that could be classified
- [CTAPHID Error Codes](components/mod_fido2/include/mod_fido2/ctaphid.h) - Protocol errors
