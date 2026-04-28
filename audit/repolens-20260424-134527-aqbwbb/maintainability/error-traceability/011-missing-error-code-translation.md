---
title: "[MEDIUM] Missing error code to string translation utilities"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
The codebase lacks centralized error code to string translation utilities for `SeResult` (Secure Element errors). While libtropic provides `lt_ret_verbose()`, the project's own `SeResult` enum has no equivalent string conversion function, making error messages less informative.

## Impact
- **Debugging difficulty**: Cannot easily convert `SeResult` codes to readable strings
- **Inconsistent logging**: Error messages are hardcoded strings instead of using a central catalog
- **UI limitations**: Cannot display user-friendly error messages from error codes
- **Maintenance burden**: Adding new error codes requires updating multiple string locations

## Evidence

### SeResult enum lacks string conversion:
`components/cdc_hal/include/cdc_hal/ISecureElement.h:20-29`:
```cpp
enum class SeResult : uint8_t {
    OK,                 // Success
    ERROR,              // Generic error
    SESSION_REQUIRED,   // No active session
    SLOT_EMPTY,         // Key slot is empty
    SLOT_OCCUPIED,      // Slot already has a key
    INVALID_PARAM,      // Invalid parameter
    ALARM_MODE,         // Chip in alarm mode (tamper detected)
    NOT_SUPPORTED       // Operation not supported
};
```

**No utility function exists** to convert `SeResult` to string:
```cpp
// Missing:
const char* seResultToString(SeResult result);
```

### libtropic has string conversion (for reference):
`third_party/libtropic/include/libtropic.h`:
```cpp
/**
 * \brief Get verbose error string
 */
const char* lt_ret_verbose(lt_ret_t ret);
```

**Used in code:**
`components/cdc_hal/src/Tropic01Element.cpp:152`:
```cpp
LOG_E(TAG, "lt_init failed (%s)", lt_ret_verbose(ret));
```

### SeResult used without string conversion:
`components/cdc_hal/src/Tropic01Element.cpp:330-362`:
```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= TROPIC01_ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;  // No string conversion
    }
    
    if (!ensureSession("eccGenerate")) {
        return SeResult::SESSION_REQUIRED;  // No string conversion
    }
    
    // ...
    return SeResult::OK;  // No string conversion
}
```

### Error messages use hardcoded strings:
`components/mod_totp/src/TotpStore.cpp`:
```cpp
SeResult result = se_->rmemWrite(slot, data, len);
if (result != SeResult::OK) {
    LOG_E(TAG, "Failed to save account to R-Memory");  // Generic message
    return false;
}
```

**Should be:**
```cpp
SeResult result = se_->rmemWrite(slot, data, len);
if (result != SeResult::OK) {
    LOG_E(TAG, "Failed to save account: %s", seResultToString(result));
    return false;
}
```

### No error code catalog:
- No central file defining all `SeResult` codes with descriptions
- No error code documentation
- No mapping to user-friendly messages

## Recommended Fix

### Fix 1: Add SeResult to string utility

Create `components/cdc_hal/include/cdc_hal/SeResult.h`:

```cpp
/**
 * \brief Convert SeResult to human-readable string
 * \param result Error code
 * \return String description
 */
inline const char* seResultToString(SeResult result) {
    switch (result) {
        case SeResult::OK:           return "OK";
        case SeResult::ERROR:        return "General error";
        case SeResult::SESSION_REQUIRED: return "Session required";
        case SeResult::SLOT_EMPTY:   return "Slot empty";
        case SeResult::SLOT_OCCUPIED: return "Slot occupied";
        case SeResult::INVALID_PARAM: return "Invalid parameter";
        case SeResult::ALARM_MODE:   return "Alarm mode (tamper)";
        case SeResult::NOT_SUPPORTED: return "Not supported";
        default:                     return "Unknown";
    }
}
```

### Fix 2: Add error code catalog

Create `components/cdc_hal/include/cdc_hal/ErrorCodes.h`:

```cpp
/**
 * \brief Error code catalog entry
 */
struct ErrorCodeEntry {
    uint8_t code;
    const char* name;
    const char* description;
    const char* remediation;
};

/**
 * \brief Get error code catalog entry
 * \param code Error code
 * \return Entry or NULL if not found
 */
const ErrorCodeEntry* getErrorCodeEntry(uint8_t code);

/**
 * \brief Get error name by code
 */
const char* getErrorName(uint8_t code);

/**
 * \brief Get error description by code
 */
const char* getErrorDescription(uint8_t code);
```

**Usage:**
```cpp
// Get full error details
const ErrorCodeEntry* entry = getErrorCodeEntry(SeResult::SLOT_EMPTY);
LOG_E(TAG, "Error %s: %s (fix: %s)", 
      entry->name, entry->description, entry->remediation);
```

### Fix 3: Update error logging patterns

`components/cdc_hal/src/Tropic01Element.cpp`:

```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= TROPIC01_ECC_SLOT_COUNT) {
        LOG_E(TAG, "Invalid slot %d: %s", slot, seResultToString(SeResult::INVALID_PARAM));
        return SeResult::INVALID_PARAM;
    }
    
    if (!ensureSession("eccGenerate")) {
        LOG_E(TAG, "Session needed: %s", seResultToString(SeResult::SESSION_REQUIRED));
        return SeResult::SESSION_REQUIRED;
    }
    
    // ...
    return SeResult::OK;
}
```

### Fix 4: Add module-level error translation

Create `components/cdc_core/ErrorCatalog.h`:

```cpp
/**
 * Module error codes
 */
typedef enum {
    // GPG module (1000-1099)
    GPG_SLOT_FULL = 1001,
    GPG_KEY_EXISTS = 1002,
    
    // FIDO2 module (2000-2099)
    FIDO2_SLOT_FULL = 2001,
    FIDO2_CREDENTIAL_NOT_FOUND = 2002,
    
    // TOTP module (3000-3099)
    TOTP_SLOT_FULL = 3001,
    TOTP_ACCOUNT_EXISTS = 3002,
    
    // Password module (4000-4099)
    PASS_SLOT_FULL = 4001,
    PASS_ACCOUNT_EXISTS = 4002,
} ModuleErrorCode;

/**
 * \brief Get module error name
 */
const char* getModuleErrorName(uint16_t code);

/**
 * \brief Get module error description
 */
const char* getModuleErrorDescription(uint16_t code);
```

### Fix 5: Add error translation for UI

```cpp
// components/cdc_ui/ErrorUi.h
class ErrorUi {
public:
    /**
     * Get user-friendly error message
     */
    static const char* getUserMessage(uint16_t error_code);
    
    /**
     * Get remediation hint
     */
    static const char* getRemediation(uint16_t error_code);
    
    /**
     * Show error toast with full context
     */
    static void showErrorToast(uint16_t error_code, const char* context);
};
```

**Usage:**
```cpp
// Instead of:
ui::showToastError(ui::tr(ui::StringId::FAILED));

// Use:
ErrorUi::showErrorToast(SeResult::SLOT_EMPTY, "Key generation");
// Shows: "Key generation failed: Slot empty"
```

### Fix 6: Add error code documentation

Create `docs/ERROR_CODES.md`:

```markdown
# Error Code Reference

## Secure Element Errors (SeResult)

| Code | Name | Description | Remediation |
|------|------|-------------|-------------|
| 0 | OK | Success | - |
| 1 | ERROR | General error | Check logs for details |
| 2 | SESSION_REQUIRED | Session not active | Call sessionStart() |
| 3 | SLOT_EMPTY | Slot has no key | Generate or import key |
| 4 | SLOT_OCCUPIED | Slot has key | Erase slot first |
| 5 | INVALID_PARAM | Invalid parameter | Check slot range |
| 6 | ALARM_MODE | Tamper detected | Reset badge |
| 7 | NOT_SUPPORTED | Operation not supported | Check chip version |

## Module Error Codes

### GPG (1000-1099)
### FIDO2 (2000-2099)
### TOTP (3000-3099)
### Password (4000-4099)
```

## Example Implementation

```cpp
// components/cdc_hal/src/SeResult.cpp
#include "cdc_hal/SeResult.h"

const char* seResultToString(SeResult result) {
    static const char* names[] = {
        "OK",              // 0
        "General error",   // 1
        "Session required",// 2
        "Slot empty",      // 3
        "Slot occupied",   // 4
        "Invalid param",   // 5
        "Alarm mode",      // 6
        "Not supported"    // 7
    };
    
    uint8_t idx = static_cast<uint8_t>(result);
    return (idx < sizeof(names)/sizeof(names[0])) ? names[idx] : "Unknown";
}

// Error catalog
const ErrorCodeEntry errorCatalog[] = {
    {1, "ERROR", "General error", "Check detailed logs"},
    {2, "SESSION_REQUIRED", "Secure session not active", "Call sessionStart()"},
    {3, "SLOT_EMPTY", "No key in slot", "Generate or import a key"},
    {4, "SLOT_OCCUPIED", "Slot already has a key", "Erase slot before writing"},
    {5, "INVALID_PARAM", "Invalid parameter", "Check slot range (0-31)"},
    {6, "ALARM_MODE", "Chip in alarm mode (tamper)", "Reset badge"},
    {7, "NOT_SUPPORTED", "Operation not supported", "Check chip version"},
};

const ErrorCodeEntry* getErrorCodeEntry(uint8_t code) {
    for (size_t i = 0; i < sizeof(errorCatalog)/sizeof(errorCatalog[0]); i++) {
        if (errorCatalog[i].code == code) {
            return &errorCatalog[i];
        }
    }
    return nullptr;
}
```

## References
- SeResult definition: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- libtropic error handling: `third_party/libtropic/include/libtropic.h`
- TROPIC01Element: `components/cdc_hal/src/Tropic01Element.cpp`
