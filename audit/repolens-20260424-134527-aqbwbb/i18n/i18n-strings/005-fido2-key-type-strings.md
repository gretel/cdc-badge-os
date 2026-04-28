---
title: "[LOW] Hardcoded strings in FIDO2 key type detection"
severity: LOW
domain: i18n/strings
lens: i18n-strings
labels:
  - "audit:i18n/i18n-strings"
---

## Summary
The FIDO2 module uses hardcoded English strings for key type display in the detail view. While these are technical terms, they could be externalized for consistency.

**components/mod_fido2/src/Fido2Ui.cpp:187-188**
```cpp
const char* key_type = (strncmp(info.rp_id, "ssh:", 4) == 0) ? "SSH" : "WebAuthn";
const char* algo_name = (info.curve == CDC_CURVE_ED25519) ? "Ed25519" : "P-256";
```

## Impact
- "SSH" and "WebAuthn" are hardcoded in detail view
- "Ed25519" and "P-256" are also hardcoded
- These strings are already registered in i18n (STR_WEB_AUTHN, STR_CURVE_ED25519, STR_CURVE_P256) but not used here
- Minor inconsistency: the module registers these strings but doesn't use them in this location

## Evidence
```
components/mod_fido2/src/Fido2Ui.cpp:187-188: Hardcoded key type strings
```

## Recommended Fix
Use the already-registered i18n strings instead of hardcoded literals:

```cpp
// Before:
const char* key_type = (strncmp(info.rp_id, "ssh:", 4) == 0) ? "SSH" : "WebAuthn";
const char* algo_name = (info.curve == CDC_CURVE_ED25519) ? "Ed25519" : "P-256";

// After:
const char* key_type = (strncmp(info.rp_id, "ssh:", 4) == 0) ? "SSH" : mstr(STR_WEB_AUTHN);
const char* algo_name = (info.curve == CDC_CURVE_ED25519) ? mstr(STR_CURVE_ED25519) : mstr(STR_CURVE_P256);
```

**Note**: "SSH" might need a new string ID if translation is desired. Consider adding `STR_SSH` if needed.

## References
- `components/mod_fido2/src/Fido2Ui.cpp:56-81` - Existing FIDO2 string registration
- `components/mod_fido2/src/Fido2Ui.cpp:187` - Location of hardcoded strings
