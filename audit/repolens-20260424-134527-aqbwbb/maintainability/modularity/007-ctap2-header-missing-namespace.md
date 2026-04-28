---
title: "[MEDIUM] ctap2.h declares functions in global namespace"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `ctap2.h` header declares 20+ CTAP2 functions without a namespace wrapper, exposing them to the global namespace. This is inconsistent with the project's modular architecture where all components should be namespaced (e.g., `cdc::mod_fido2`).

**Evidence:**
- File: `components/mod_fido2/include/mod_fido2/ctap2.h`
- Functions like `ctap2_init()`, `ctap2_process()`, `ctap2_reset()` are in global scope
- Other mod_fido2 headers (`fido2.h`, `fido2_storage.h`) use `namespace cdc::mod_fido2`

## Impact
1. **Name collision risk**: Functions like `reset()`, `init()`, `process()` are common names
2. **Inconsistent API**: Developers must remember which functions are namespaced
3. **Refactoring difficulty**: Moving code requires updating all call sites
4. **Documentation confusion**: Function origin isn't clear from namespace

## Evidence
**ctap2.h** (sample functions, lines 30-80):
```cpp
// No namespace wrapper!
uint8_t ctap2_init(uint8_t* params, size_t params_len, uint8_t* response, size_t response_len);
uint8_t ctap2_process(uint8_t cmd, uint8_t* params, size_t params_len, uint8_t* response, size_t response_len);
void ctap2_send_keepalive(uint8_t status);
void ctap2_cancel(void);
uint8_t ctap2_reset(uint8_t* response, size_t response_len);
```

**Compare with fido2.h** (properly namespaced):
```cpp
namespace cdc::mod_fido2 {
void fido2_init();
bool fido2_process(uint8_t cmd, uint8_t* params, size_t params_len, uint8_t* response, size_t response_len);
}
```

## Recommended Fix
1. Wrap ctap2.h declarations in `namespace cdc::mod_fido2 { }` (10 minutes)
2. Update all includes: `#include "mod_fido2/ctap2.h"` (already correct)
3. Update all callers to use `cdc::mod_fido2::ctap2_init()` etc. (20 minutes)
4. Verify compilation (5 minutes)

**Total estimated time: ~35 minutes**

## References
- Project pattern: All modules use `namespace cdc::<module_name>`
- C++ Core Guidelines [R.10](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-namespace): Use namespaces to avoid collisions
