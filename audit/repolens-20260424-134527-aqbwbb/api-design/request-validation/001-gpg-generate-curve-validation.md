---
title: "[MEDIUM] GPG_GENERATE command lacks curve parameter validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `GPG_GENERATE` serial command in `components/mod_gpg/src/GpgModule.cpp:175` parses the curve parameter using `atoi()` without validating that the input is a recognized curve value. Invalid or out-of-range curve values silently default to ED25519 (curve=0), which may lead to unexpected behavior.

**File**: `components/mod_gpg/src/GpgModule.cpp`  
**Line**: 175  
**Function**: `cmd_gpg_generate()`

## Impact
- **User confusion**: Users providing invalid curve values (e.g., `GPG_GENERATE 99 MyKey`) get ED25519 without knowing their input was invalid
- **Silent data loss**: The curve parameter is accepted but silently converted, making debugging difficult
- **Inconsistent behavior**: Unlike other commands (e.g., `TR01_ECC_DEL` which validates slot ranges), this command doesn't provide explicit validation feedback

## Evidence
From `components/mod_gpg/src/GpgModule.cpp:175`:

```cpp
uint8_t curveBuf[8] = {};
// ... parsing curveBuf from args ...
uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
```

Valid curves are defined in `components/mod_gpg/include/mod_gpg/gpg.h`:
- `CDC_CURVE_ED25519 = 0`
- `CDC_CURVE_P256 = 1`

The code only checks for `atoi(curveBuf) == 2` to select P-256, but:
- Input "0" → `atoi` returns 0 → curve = ED25519 ✓
- Input "1" → `atoi` returns 1 → curve = ED25519 (intended P-256?) ✗
- Input "2" → `atoi` returns 2 → curve = P-256 ✓
- Input "99" → `atoi` returns 99 → curve = ED25519 (silent fallback) ✗
- Input "abc" → `atoi` returns 0 → curve = ED25519 (silent fallback) ✗

## Recommended Fix
Add explicit validation for the curve parameter before using it:

```cpp
uint8_t curve = CDC_CURVE_ED25519;  // default
int curveVal = atoi(curveBuf);

if (curveVal == 0 || curveVal == 1) {
    // Accept numeric 0 or 1
    curve = (curveVal == 1) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
} else if (curveVal == 2) {
    // Accept 2 as P-256 (backward compatibility)
    curve = CDC_CURVE_P256;
} else {
    cdc::serial::Console::printf("ERROR: Invalid curve (use 0=ED25519, 1=P-256, or 2=P-256)\r\n");
    return;
}
```

Or better, support both numeric and string formats:
```cpp
char curveLower[8] = {};
// Copy and lowercase curveBuf into curveLower
if (strcmp(curveLower, "ed25519") == 0 || strcmp(curveLower, "0") == 0) {
    curve = CDC_CURVE_ED25519;
} else if (strcmp(curveLower, "p256") == 0 || strcmp(curveLower, "p-256") == 0 || 
           strcmp(curveLower, "1") == 0 || strcmp(curveLower, "2") == 0) {
    curve = CDC_CURVE_P256;
} else {
    cdc::serial::Console::printf("ERROR: Invalid curve (use 0/ed25519 or 1/2/p256)\r\n");
    return;
}
```

## References
- Similar validation pattern in `components/serial_cmd/src/SerialCmd.cpp:169` (parseSlotArg with explicit range check)
- GPG header: `components/mod_gpg/include/mod_gpg/gpg.h:11-12` (curve definitions)
