---
title: "[MEDIUM] GPG module settings lack explanations for technical options"
severity: MEDIUM
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The GPG module (`components/mod_gpg/src/GpgModule.cpp`) exposes settings like "User PIN", "Admin PIN", "Curve", and "Generate Keys" but provides no inline help explaining what each option does or the implications of changing them.

**Evidence** (`components/mod_gpg/src/GpgModule.cpp:36-48`):
```cpp
static constexpr uint16_t STR_GPG = 0;
static constexpr uint16_t STR_STATUS = 1;
static constexpr uint16_t STR_GENERATE = 2;
static constexpr uint16_t STR_EXPORT = 3;
static constexpr uint16_t STR_RESET = 4;
static constexpr uint16_t STR_SETTINGS = 5;
static constexpr uint16_t STR_USER_PIN = 6;
static constexpr uint16_t STR_ADMIN_PIN = 7;
static constexpr uint16_t STR_SLOT_ERROR = 8;
static constexpr uint16_t STR_NAME = 9;
static constexpr uint16_t STR_EMAIL = 10;
static constexpr uint16_t STR_CURVE = 11;
static constexpr uint16_t STR_CURVE_ED25519 = 12;
static constexpr uint16_t STR_CURVE_P256 = 10;
static constexpr uint16_t STR_NO_KEY = 14;
static constexpr uint16_t STR_CONFIRM_RESET = 15;
```

The string registrations (lines 69-104) show basic labels:
- "User PIN" - no explanation of what it protects
- "Admin PIN" - no explanation of admin privileges
- "Curve" - no explanation of Ed25519 vs P-256 differences
- "Generate Keys" - no explanation of key lifecycle

## Impact
Users may:
1. Not understand the difference between User and Admin PIN
2. Choose a curve without knowing security/performance tradeoffs
3. Reset keys without understanding data loss implications
4. Not know what "Export Public" means

## Evidence
- File: `components/mod_gpg/src/GpgModule.cpp`
- Lines: 69-104 (string registration)
- No descriptive text for technical options
- No "Learn more" or info icons in the menu

## Recommended Fix
Add contextual help to GPG settings:

1. **Enhanced menu labels** with brief descriptions:
   ```
   User PIN (protects signing keys)
   Admin PIN (full key management)
   Curve: Ed25519 (faster) or P-256 (wider support)
   ```

2. **Info key action** (key '3' or 'I'):
   - Shows detailed explanation of each setting
   - Explains curve differences
   - Warns about reset implications

3. **Confirmation dialogs** with more context:
   ```
   Reset all GPG keys?
   (Signatures, encryption keys will be lost)
   (User ID: John Doe <john@example.com>)
   ```

## References
- OpenPGP card specification: https://gnu-pa.github.io/gpg-it-manual/OpenPGP-card.html
- Ed25519 vs P-256 comparison: https://en.wikipedia.org/wiki/EdDSA
