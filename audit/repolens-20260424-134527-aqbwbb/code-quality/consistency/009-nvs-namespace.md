---
title: "[MEDIUM] Inconsistent NVS namespace naming across modules"
severity: MEDIUM
domain: code-quality/consistency
lens: configuration-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
Non-volatile storage (NVS) namespace names follow inconsistent naming patterns across modules. Some use `mod_<name>` prefix while others use just `<name>`.

**With `mod_` prefix:**
- `mod_gpg`: `"mod_gpg"` (in `gpg.cpp`)
- `mod_hid`: `"mod_hid"`
- `mod_grove_led`: `"mod_grove_led"`

**Without prefix:**
- `mod_fido2`: `"fido2"` (in `fido2_storage.cpp`)
- `mod_gpg/openpgp`: `"openpgp"` (in `openpgp.cpp`)

**Core modules (different pattern):**
- `cdc_os_ui/display`: `"display"`
- `cdc_os_ui/wifi`: `"wifi"`

## Impact
- **NVS namespace collisions**: Risk of overlap if modules have similar short names
- **Debugging confusion**: Harder to identify which module owns which NVS data
- **Inconsistent organization**: No clear pattern for new module authors

## Evidence
File: `components/mod_gpg/src/gpg.cpp:13`
```cpp
static constexpr const char* NVS_NAMESPACE = "mod_gpg";
```

File: `components/mod_fido2/src/fido2_storage.cpp:22`
```cpp
#define NVS_NAMESPACE           "fido2"
```

File: `components/mod_gpg/src/openpgp/openpgp.cpp:150`
```cpp
#define NVS_NAMESPACE "openpgp"
```

File: `components/grove_led/src/GroveLedModule.cpp:18`
```cpp
static constexpr const char* NVS_NAMESPACE = "mod_grove_led";
```

Also inconsistent: declaration style - some use `#define` while others use `static constexpr const char*`.

## Recommended Fix
1. Adopt consistent format: `mod_<module_name>` for all modules
2. Use `static constexpr const char*` instead of `#define` for type safety
3. Update all NVS namespace definitions:
   - `mod_fido2`: Change `"fido2"` → `"mod_fido2"`
   - `mod_gpg/openpgp`: Change `"openpgp"` → `"mod_gpg_openpgp"` or keep as `"mod_gpg"` if same module

Example consistent pattern:
```cpp
static constexpr const char* NVS_NAMESPACE = "mod_<name>";
```

## References
- NVS documentation: ESP-IDF Non-volatile storage library
- Consistent modules: `mod_gpg`, `mod_hid`, `mod_grove_led`
