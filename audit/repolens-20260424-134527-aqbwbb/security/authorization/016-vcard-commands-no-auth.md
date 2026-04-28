---
title: "[LOW] VCard serial commands lack authorization"
severity: LOW
domain: authorization
lens: serial-command-auth
labels:
  - "low:vcard-auth"
---

## Summary
VCard module commands (`VCARD_SET`, `VCARD_GET`, `VCARD_DELETE`) are registered with `requiresAuth = false`, allowing anyone with serial access to read and modify vCard data without authentication.

**File:** `components/mod_vcard/src/VcardModule.cpp`  
**Lines:** 436-438

## Evidence

**Command registration (lines 436-438):**
```cpp
static void registerSerialCommands() {
    auto& reg = serial::getCommandRegistry();
    reg.registerCommand({"VCARD_SET",    "Set own vCard (multiline paste)", cmdVcardSet,    "vcard", false});
    reg.registerCommand({"VCARD_GET",    "Show own vCard",                  cmdVcardGet,    "vcard", false});
    reg.registerCommand({"VCARD_DELETE", "Delete own vCard",                cmdVcardDelete, "vcard", false});
}
```

All three commands use `requiresAuth = false`.

## Impact
An attacker can:
1. Read the device's vCard (information disclosure)
2. Modify the vCard with arbitrary data
3. Delete the vCard

While vCard data is relatively low-security (contact information), this still allows unauthorized data modification.

## Recommended Fix
Add authorization to all VCard commands:

```cpp
static void registerSerialCommands() {
    auto& reg = serial::getCommandRegistry();
    reg.registerCommand({"VCARD_SET",    "Set own vCard (multiline paste)", cmdVcardSet,    "vcard", true});
    reg.registerCommand({"VCARD_GET",    "Show own vCard",                  cmdVcardGet,    "vcard", true});
    reg.registerCommand({"VCARD_DELETE", "Delete own vCard",                cmdVcardDelete, "vcard", true});
}
```

## References
- CWE-287: Improper Authentication
- CWE-319: Cleartext Transmission of Sensitive Information
