---
title: "[LOW] Error codes lack user-facing documentation"
severity: LOW
domain: Error Handling / Error Messages
lens: Error Message Quality
labels:
  - "audit:error-handling/error-messages"
---

## Summary
The codebase uses well-defined error codes (CTAP2, CTAPHID, U2F, SeResult), but these codes are not exposed to users in a readable format. When errors occur, users only see generic messages like "Failed" instead of specific error codes that could help with troubleshooting.

## Affected Areas

### CTAP2/FIDO2 Error Codes
**Location:** `components/mod_fido2/include/mod_fido2/ctap2.h` (lines 47-79)

37 defined error codes including:
- `CTAP2_ERR_PIN_INVALID` (0x31)
- `CTAP2_ERR_PIN_BLOCKED` (0x32)
- `CTAP2_ERR_KEY_STORE_FULL` (0x28)
- `CTAP2_ERR_INVALID_CREDENTIAL` (0x22)

**Current Usage:** These codes are used internally but not exposed to users.

### CTAPHID Error Codes
**Location:** `components/mod_fido2/include/mod_fido2/ctaphid.h` (lines 32-40)

9 defined error codes:
- `CTAPHID_ERR_INVALID_CMD` (0x01)
- `CTAPHID_ERR_INVALID_PAR` (0x02)
- `CTAPHID_ERR_TIMEOUT` (0x05)

### Secure Element Results
**Location:** `components/cdc_hal/include/cdc_hal/ISecureElement.h` (lines 20-29)

```cpp
enum class SeResult : uint8_t {
    OK,
    ERROR,
    SESSION_REQUIRED,
    SLOT_EMPTY,
    SLOT_OCCUPIED,
    INVALID_PARAM,
    ALARM_MODE,
    NOT_SUPPORTED
};
```

## Impact
- **Limited Troubleshooting**: Users cannot identify specific error types from UI messages
- **Debugging Difficulty**: Serial log analysis requires understanding internal code mappings
- **API Consumers**: Serial command users don't get machine-readable error codes

## Evidence

Example - FIDO2 PIN failure shows specific message:
```cpp
// components/mod_fido2/src/Fido2Ui.cpp:359-365
static void onPinFailure(bool lockedOut) {
    if (lockedOut) {
        ui::showToastError(ui::tr(ui::StringId::TOO_MANY_ATTEMPTS), 2000);
        promptComplete(FIDO2_UP_DENIED);
    } else {
        ui::showToastError(ui::tr(ui::StringId::WRONG_PIN), 1000);
    }
}
```

But internal CTAP2 error codes (e.g., `CTAP2_ERR_PIN_INVALID` vs `CTAP2_ERR_PIN_BLOCKED`) are not exposed.

## Recommended Fix

### Option 1: Add Error Code Helper Function
Create a utility to convert error codes to user-friendly strings:

```cpp
// components/mod_fido2/include/mod_fido2/error_utils.h
/**
 * \brief Convert CTAP2 error code to user-friendly string.
 * \param code CTAP2 error code.
 * \return Static string pointer.
 */
const char* ctap2_error_to_string(uint8_t code);

// components/mod_fido2/src/error_utils.cpp
const char* ctap2_error_to_string(uint8_t code) {
    switch (code) {
        case CTAP2_ERR_PIN_INVALID: return "PIN invalid";
        case CTAP2_ERR_PIN_BLOCKED: return "PIN blocked";
        case CTAP2_ERR_KEY_STORE_FULL: return "Key store full";
        // ...
        default: return "Error";
    }
}
```

### Option 2: Add Error Code to Serial Output
For serial command users, include error codes in responses:

```cpp
// components/mod_vcard/src/VcardModule.cpp line 331
Console::printf("ERROR: %s (code: 0x%02X)\r\n", err[0] ? err : "Invalid vCard", error_code);
```

### Option 3: Expand UI Error Messages
Replace generic messages with specific ones:

```cpp
// Instead of:
ui::showToastError(ui::tr(ui::StringId::FAILED));

// Use:
ui::showToastError(ctap2_error_to_string(result.code));
```

## References
- `components/mod_fido2/include/mod_fido2/ctap2.h` - CTAP2 error codes
- `components/mod_fido2/include/mod_fido2/ctaphid.h` - CTAPHID error codes
- `components/cdc_hal/include/cdc_hal/ISecureElement.h` - SeResult enum
- CTAP2 Specification: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-common.html
