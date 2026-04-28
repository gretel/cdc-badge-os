---
title: "[HIGH] No End-to-End Tests for Critical User Flows (FIDO2, TOTP, Passwords, GPG)"
severity: HIGH
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The CDC Badge OS firmware has **zero end-to-end (E2E) tests** for its critical user-facing workflows. The codebase contains only 3 basic smoke tests for the vCard module (`test_vcard_store`, `test_ble_vcard_symbols`, `test_vcard_module_link`) that verify symbol linking and trivial function calls.

**Critical user flows without any E2E coverage:**

1. **FIDO2/WebAuthn Authentication Flow** (`components/mod_fido2/`)
   - Credential registration with site confirmation
   - PIN authentication for FIDO2 operations
   - Authentication assertion generation
   - U2F legacy authentication

2. **TOTP Authenticator Flow** (`components/mod_totp/`)
   - Account addition via T9 input wizard
   - Secret validation and storage
   - Code generation and display with countdown timer
   - Account editing and deletion

3. **Password Vault Flow** (`components/mod_password/`)
   - Entry creation (name, user, URL, password)
   - Password retrieval and display
   - Entry editing and deletion

4. **GPG/CCID Smartcard Flow** (`components/mod_gpg/`)
   - Key generation (Ed25519/P-256)
   - PW1/PW3 PIN authentication
   - Public key export
   - CCID protocol handling

5. **Device Unlock Flow** (Core)
   - Badge PIN entry with T9 input
   - PIN verification with lockout logic
   - Lock screen to main menu transition

## Impact

**Security Risk:**
- FIDO2 is the primary use case for a hardware security key
- Without E2E tests, authentication bugs may go undetected until production
- PIN lockout logic (critical for brute-force protection) is only tested at unit level

**Maintenance Burden:**
- Module changes require manual testing of full user flows
- No regression protection for multi-step workflows
- Developers must physically test on hardware for every change

**User Trust:**
- Users rely on this device for protecting critical accounts
- Unverified flows increase risk of authentication failures

## Evidence

**Existing tests only cover vCard module (3 smoke tests):**

`test/test_vcard_store/test_vcard_store.cpp:9-13`:
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

**No test files exist for:**
- `components/mod_fido2/` (10 source files, 200+ lines each)
- `components/mod_totp/` (2 source files, 450+ lines each)
- `components/mod_password/` (2 source files, 400+ lines each)
- `components/mod_gpg/` (5 source files, 600+ lines each)
- `components/cdc_core/src/PinManager.cpp` (663 lines, critical PIN logic)

**Build workflow lacks test stage** (`.github/workflows/build.yml`):
```yaml
# Only builds firmware, no test execution
- name: Build firmware
  run: pio run
```

**UI flow documentation exists but is untested** (`docs/UI_FLOWS.md`):
- Documents 8+ screens and 15+ user interactions
- All flows are documented but none are automated

## Recommended Fix

Create a test framework structure with initial E2E tests for the highest-priority flow:

1. **Set up test infrastructure** (30 min):
   - Create `test/e2e/` directory structure
   - Add test runner configuration for ESP32 (similar to existing unit tests)
   - Document test setup in `docs/TESTING.md`

2. **Implement FIDO2 PIN Authentication Test** (30 min):
   - Test PIN entry flow from lock screen
   - Verify PIN validation with correct/incorrect codes
   - Verify lockout after 3 failures
   - Verify lockout timer expiration

**Test file structure:**
```
test/e2e/
  e2e_fido2_pin_auth.cpp    // FIDO2 PIN flow test
  e2e_totp_add_account.cpp  // TOTP account creation
  e2e_password_crud.cpp     // Password CRUD operations
  e2e_gpg_key_gen.cpp       // GPG key generation
```

**Example test skeleton:**
```cpp
#include <unity.h>
#include "cdc_core/PinManager.h"
#include "mod_fido2/fido2.h"

void test_pin_entry_and_fido2_auth() {
    // Setup
    PinManager::instance().init();
    
    // Test correct PIN
    TEST_ASSERT_TRUE(PinManager::instance().verifyBadgePin("123456"));
    
    // Test wrong PIN
    TEST_ASSERT_FALSE(PinManager::instance().verifyBadgePin("654321"));
    
    // Test lockout after 3 failures
    PinManager::instance().verifyBadgePin("654321");
    PinManager::instance().verifyBadgePin("654321");
    PinManager::instance().verifyBadgePin("654321");
    TEST_ASSERT_TRUE(PinManager::instance().isBadgeBlocked());
}
```

## References

- [UI Flows Documentation](docs/UI_FLOWS.md) - Documents all user interactions
- [Serial Commands Reference](docs/SERIAL_COMMANDS.md) - Command interface for testing
- [PinManager Implementation](components/cdc_core/src/PinManager.cpp) - PIN logic (lines 290-330, 600-660)
- [FIDO2 Module](components/mod_fido2/) - Main authentication module
- [Existing Test Structure](test/) - 3 vCard smoke tests for reference
