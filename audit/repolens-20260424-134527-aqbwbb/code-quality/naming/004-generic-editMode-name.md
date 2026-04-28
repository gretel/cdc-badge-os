---
title: "[LOW] Generic struct member name 'editMode' lacks context in TOTP wizard"
severity: LOW
domain: code-quality/naming
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary

The `WizardState` struct in the TOTP module uses a generic boolean member name `editMode` without a clear prefix or context to indicate what it represents.

### Affected Files

| File | Location |
|------|----------|
| `components/mod_totp/src/TotpModule.cpp:566` | `WizardState` struct definition |

## Impact

**Readability**: The name `editMode` alone doesn't clarify what is being edited. Is it edit mode for an account? A slot? The wizard itself?

**Maintenance**: Future developers may need to trace usage to understand the context.

**Consistency**: The struct uses descriptive names for other members (`editSlot`, `name`, `issuer`, `secret`) but `editMode` is less specific.

## Evidence

From `components/mod_totp/src/TotpModule.cpp:559-568`:
```cpp
struct WizardState {
    char name[TotpStore::NAME_LEN + 1];
    char secret[128];
    char issuer[TotpStore::ISSUER_LEN + 1];
    uint8_t digits;
    uint8_t algorithm;
    uint32_t period;
    bool editMode;       // Generic - what is being edited?
    uint16_t editSlot;   // More specific - which slot
};
```

Usage examples (lines 749, 772, 883):
```cpp
s_wizard.editMode = false;
s_wizard.editMode = true;
if (s_wizard.editMode) {
```

## Recommended Fix

Rename `editMode` to be more descriptive:

**Option A (Recommended):**
```cpp
bool isEditingAccount_ = false;  // Clear what is being edited
```

**Option B:**
```cpp
bool inEditMode = false;  // Slightly more descriptive
```

Process:
1. Rename `editMode` to `isEditingAccount_` (or chosen name) in struct definition
2. Update all usages: `s_wizard.editMode` → `s_wizard.isEditingAccount_`
3. Verify compilation and test

## References

- [Clean Code: Meaningful Names](https://github.com/unclebob/clean-code-swift/blob/master/README.md)
- [Google C++ Style Guide - Boolean Names](https://google.github.io/styleguide/cppguide.html#Boolean_Names)

</content>