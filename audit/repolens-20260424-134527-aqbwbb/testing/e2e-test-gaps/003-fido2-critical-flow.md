---
title: "[CRITICAL] No E2E Tests for FIDO2/WebAuthn Registration and Authentication"
severity: CRITICAL
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The **FIDO2/WebAuthn module** (`components/mod_fido2/`) is the primary feature of this hardware security key, yet it has **zero E2E tests**. The module includes:

- FIDO2 credential registration with site confirmation
- FIDO2 authentication assertion generation
- Client PIN protocol (FIDO 2.1)
- U2F legacy authentication
- PIN-based protection for FIDO2 operations

**Critical user flow:**
1. User navigates to website (e.g., github.com)
2. Website requests FIDO2 credential
3. Badge displays site name, user presses Y to confirm
4. Badge prompts for PIN
5. User enters PIN on keypad
6. Badge generates assertion and sends to computer

This entire flow is untested end-to-end.

## Impact

**Primary Risk:**
- FIDO2 is the main use case for a hardware security key
- Bugs in registration could prevent users from adding credentials
- Bugs in authentication could lock users out of accounts
- PIN protection bugs could allow unauthorized sign-ins

**No Regression Protection:**
- Changes to CTAP2 protocol handling (122KB file!) have no test coverage
- FIDO2 storage logic (33KB) is untested
- UI confirmation flow is manual-only

## Evidence

**Module structure** (`components/mod_fido2/`):
- `ctap2.cpp` - 12,255 lines (FIDO2 protocol implementation)
- `fido2_storage.cpp` - 33,942 lines (credential storage)
- `fido2.cpp` - 9,360 lines (core FIDO2 logic)
- `ctaphid.cpp` - 22,657 lines (HID transport)
- `Fido2Ui.cpp` - 19,930 lines (UI flows)
- `Fido2Module.cpp` - 9,311 lines (module integration)
- `u2f.cpp` - 28,908 lines (legacy U2F)
- `pin_storage.cpp` - 1,447 lines (FIDO2 PIN cache)
- `cbor_helpers.cpp` - 17,837 lines (encoding)

**FIDO2 UI flow** (`components/mod_fido2/src/Fido2Ui.cpp`):
```cpp
// Simplified flow from source
void Fido2Ui::onAuthRequest(const char* site) {
    // 1. Display site name
    display->showAuthRequest(site);
    
    // 2. Wait for Y/N
    if (waitForConfirmation()) {
        // 3. Prompt for PIN
        const char* pin = showPinEntry();
        
        // 4. Verify PIN
        if (PinManager::instance().verifyBadgePin(pin)) {
            // 5. Generate assertion
            fido2_generateAssertion(site);
        }
    }
}
```

**No test files exist** for any of these 10 source files.

**Existing tests only cover vCard** (`test/` directory):
- `test_vcard_store.cpp` - 21 lines
- `test_ble_vcard_symbols.cpp` - 19 lines
- `test_vcard_module_link.cpp` - 19 lines

## Recommended Fix

Create E2E test file `test/e2e/e2e_fido2_flow.cpp`:

```cpp
#include <unity.h>
#include "mod_fido2/fido2.h"
#include "mod_fido2/Fido2Module.h"
#include "cdc_core/PinManager.h"
#include "cdc_core/TropicStorage.h"

using namespace cdc::mod_fido2;

void setUp() {
    Fido2Module::instance().init();
    Fido2Module::instance().start();
}

void test_fido2_module_initialization() {
    TEST_ASSERT_EQUAL(ServiceState::RUNNING, Fido2Module::instance().getState());
}

void test_fido2_registration_flow() {
    // Setup: Prepare for registration
    // Note: In real E2E, this would involve USB HID simulation
    
    // 1. Start registration
    // fido2_beginRegistration("test.github.com");
    
    // 2. Verify confirmation prompt is shown
    // TEST_ASSERT_TRUE(ui_isWaitingForConfirmation());
    
    // 3. Simulate Y press
    // ui_simulateConfirm();
    
    // 4. Verify PIN prompt
    // TEST_ASSERT_TRUE(ui_isWaitingForPin());
    
    // 5. Enter correct PIN
    // const char* pin = "123456";
    // fido2_submitPin(pin);
    
    // 6. Verify credential created
    // TEST_ASSERT_TRUE(fido2_isRegistrationComplete());
}

void test_fido2_authentication_flow() {
    // Prerequisite: Register a credential first
    // fido2_register("test.github.com", "user123");
    
    // 1. Start authentication
    // fido2_beginAuthentication("test.github.com");
    
    // 2. Verify confirmation prompt
    // TEST_ASSERT_TRUE(ui_isWaitingForConfirmation());
    
    // 3. Simulate Y press
    // ui_simulateConfirm();
    
    // 4. Enter PIN
    // fido2_submitPin("123456");
    
    // 5. Verify assertion generated
    // TEST_ASSERT_TRUE(fido2_hasAssertion());
}

void test_fido2_wrong_pin_fails() {
    // 1. Start authentication
    // fido2_beginAuthentication("test.github.com");
    // ui_simulateConfirm();
    
    // 2. Enter wrong PIN
    // fido2_submitPin("wrong123");
    
    // 3. Verify authentication failed
    // TEST_ASSERT_FALSE(fido2_hasAssertion());
}

void test_fido2_pin_lockout() {
    // 1. Trigger PIN lockout (3 wrong attempts)
    // for (int i = 0; i < 3; i++) {
    //     fido2_beginAuthentication("test.github.com");
    //     ui_simulateConfirm();
    //     fido2_submitPin("wrong");
    // }
    
    // 2. Verify FIDO2 operations blocked
    // TEST_ASSERT_TRUE(PinManager::instance().isBadgeBlocked());
    
    // 3. Try authentication
    // fido2_beginAuthentication("test.github.com");
    // ui_simulateConfirm();
    
    // 4. Should fail immediately
    // TEST_ASSERT_FALSE(fido2_hasAssertion());
}

void test_fido2_credential_storage() {
    // 1. Register credential
    // fido2_register("site1.com", "user1");
    
    // 2. Verify stored
    // uint8_t count = fido2_getCredentialCount();
    // TEST_ASSERT_EQUAL(1, count);
    
    // 3. Register second credential
    // fido2_register("site2.com", "user2");
    
    // 4. Verify count
    // TEST_ASSERT_EQUAL(2, fido2_getCredentialCount());
}

void test_fido2_u2f_compatibility() {
    // Test U2F legacy flow
    // u2f_beginRegistration();
    // ui_simulateConfirm();
    // fido2_submitPin("123456");
    // TEST_ASSERT_TRUE(u2f_hasRegistration());
}

extern "C" void app_main() {
    UNITY_BEGIN();
    RUN_TEST(test_fido2_module_initialization);
    RUN_TEST(test_fido2_registration_flow);
    RUN_TEST(test_fido2_authentication_flow);
    RUN_TEST(test_fido2_wrong_pin_fails);
    RUN_TEST(test_fido2_pin_lockout);
    RUN_TEST(test_fido2_credential_storage);
    RUN_TEST(test_fido2_u2f_compatibility);
    UNITY_END();
}
```

**Additional considerations:**
- Tests may need USB HID simulation (mock or hardware)
- TROPIC01 secure element interaction must be tested
- Consider using `DEBUG_MODE=1` for test environment (disables lockouts)

## References

- [FIDO2 Module Source](components/mod_fido2/src/) - 10 files, 200KB+ total
- [FIDO2 UI](components/mod_fido2/src/Fido2Ui.cpp) - Lines 50-150 (confirmation flow)
- [FIDO2 Core](components/mod_fido2/src/fido2.h) - API reference
- [UI Flows](docs/UI_FLOWS.md) - FIDO2 authentication (lines 66-94)
- [Serial Commands](docs/SERIAL_COMMANDS.md) - FIDO2 via serial
