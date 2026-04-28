---
title: "[MEDIUM] Module-specific integration tests missing for GPG, FIDO2, TOTP, Password modules"
severity: MEDIUM
domain: modules
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:modules"
  - "area:module-integration"
---

## Summary
Each module (GPG, FIDO2, TOTP, Password, vCard) has complex integration with services (Secure Element, Display, Keypad, UI) but **no integration tests** verify the complete workflow from user input to storage to output.

## Evidence

**GPG Module** (`components/mod_gpg/src/GpgModule.cpp:26-661`):
- Generates ECC keys in TROPIC01 slots 1-3
- Exports public keys via serial command
- Integrates with PinManager for PIN storage
- No integration tests beyond basic linking

**FIDO2 Module** (`components/mod_fido2/src/Fido2Module.cpp:1-297`):
- Implements CTAPHID protocol over USB HID
- Stores credentials in TROPIC01 slots 5-31
- Handles registration and authentication flows
- No integration tests

**TOTP Module** (`components/mod_totp/src/TotpModule.cpp:1-1020`):
- Stores secrets in TROPIC01 R-Memory slots 32-131
- Generates time-based codes
- Integrates with display for QR codes
- No integration tests

**Password Module** (`components/mod_password/src/PasswordModule.cpp:1-856`):
- Stores entries in TROPIC01 R-Memory slots 150-511
- Integrates with HID for typing passwords
- No integration tests

**Current test coverage**: Only `test_vcard_module_link` which just calls register function

## Impact
- **Workflow bugs**: Complete user flows may have untested edge cases
- **Storage integration**: Module-specific TROPIC01 usage not verified
- **UI integration**: Views may not integrate correctly with module logic

## Recommended Fix

Create integration tests for each module:

**test_gpg_integration/**:
- Generate key pair and verify stored in TROPIC01
- Export public key via serial command
- PIN verification flow

**test_fido2_integration/**:
- CTAPHID initialization and packet exchange
- Registration flow with credential storage
- Authentication flow with signing

**test_totp_integration/**:
- Add secret and verify stored in R-Memory
- Generate code and verify correctness
- Time synchronization

**test_password_integration/**:
- Add password entry and verify storage
- Retrieve and type via HID
- Integration with TOTP slot reference

**Test structure** (example):
```cpp
// test/test_gpg_integration/test_gpg_workflow.cpp
#include "mod_gpg/GpgModule.h"
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/TropicStorage.h"

void test_gpg_key_generation_workflow() {
    // 1. Initialize module
    mod_gpg::GpgModule& gpg = mod_gpg::GpgModule::instance();
    gpg.init();
    gpg.start();
    
    // 2. Generate key
    GpgStorage::generateKey(0, EccCurve::Ed25519);
    
    // 3. Verify stored in TROPIC01
    ISecureElement* se = getSecureElementInstance();
    ASSERT_TRUE(se->eccSlotUsed(1));  // Slot 1 for GPG slot 0
}
```

## References
- [GPG Module](components/mod_gpg/src/GpgModule.cpp)
- [FIDO2 Module](components/mod_fido2/src/Fido2Module.cpp)
- [TOTP Module](components/mod_totp/src/TotpModule.cpp)
- [Password Module](components/mod_password/src/PasswordModule.cpp)
