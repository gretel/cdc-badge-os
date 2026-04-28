---
title: "[LOW] Logging TAG Naming Convention Inconsistency"
severity: LOW
domain: logging
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent naming conventions for logging TAG variables.

### Evidence

**Different naming styles:**
```cpp
// Full words
static const char* TAG = "ServiceRegistry";  // cdc_core/src/ServiceRegistry.cpp
static const char* TAG = "UsbManager";       // cdc_core/src/UsbManager.cpp
static const char* TAG = "PinManager";       // cdc_core/src/PinManager.cpp

// Abbreviated
static const char* TAG = "AttestKey";        // cdc_core/src/AttestationKeyService.cpp
static const char* TAG = "SlotMap";          // cdc_core/src/TropicSlotMap.cpp
static const char* TAG = "ModuleReg";        // cdc_core/src/ModuleRegistry.cpp

// Different patterns
static const char *TAG = "CCID";             // mod_gpg/src/openpgp/ccid.cpp (space before *)
static const char* TAG = "CCID";             // mod_gpg/src/ccid/ccid_driver.cpp
static const char* TAG = "OpenPGP";          // mod_gpg/src/openpgp/openpgp.cpp
static const char* TAG = "GPG";              // mod_gpg/src/GpgModule.cpp
```

**Inconsistent abbreviations:**
- "AttestKey" vs. "GPGStorage" (one abbreviated, one not)
- "ModuleReg" vs. "UsbManager" (one abbreviated, one not)
- "TR01_STORE" vs. "SlotMap" (different styles)

## Impact
- **Search difficulty**: Harder to grep for related logs
- **Inconsistent appearance**: Log output looks less professional
- **Maintenance friction**: Developers must guess naming convention

## Recommended Fix
1. **Establish naming convention**:
   - Use full words, no abbreviations (e.g., "AttestationKey" not "AttestKey")
   - Use PascalCase or all-caps consistently
2. **Create naming guide** for logging tags
3. **Fix existing tags** in batches

### Suggested consistent names:
- AttestationKey (not AttestKey)
- ModuleRegistry (not ModuleReg)
- TropicSlotMap (not SlotMap)
- TropicStorage (not TR01_STORE)

## References
- Project logging documentation
- ESP-IDF logging conventions
