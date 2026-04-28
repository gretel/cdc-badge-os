---
title: "[MEDIUM] No E2E Tests for Multi-Step Wizard Flows (TOTP, Passwords)"
severity: MEDIUM
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The firmware implements **multi-step wizard flows** for adding TOTP accounts and password entries, but these have **no E2E test coverage**. These flows involve:

1. **TOTP Add Account Wizard** (`components/mod_totp/`)
   - Step 1: Account name input (T9)
   - Step 2: Secret input (Base32, T9)
   - Step 3: Issuer input (optional, T9)
   - Step 4: Digits selection (6/7/8)
   - Step 5: Algorithm selection (SHA1/SHA256/SHA512)
   - Step 6: Period selection (30s/60s)
   - Step 7: Confirmation

2. **Password Add Entry Wizard** (`components/mod_password/`)
   - Step 1: Entry name input (T9)
   - Step 2: Username input (T9)
   - Step 3: URL input (T9)
   - Step 4: Password input (T9)
   - Step 5: Confirmation

These wizards involve state persistence across steps, back-navigation, and validation at each stage.

## Impact

**User Experience Risk:**
- Wizard state may not persist correctly across steps
- Back-navigation might lose previously entered data
- Validation might not catch invalid inputs (e.g., non-Base32 secrets)
- Edge cases (empty fields, max-length inputs) untested

**Data Integrity:**
- TOTP secrets must be stored correctly for codes to generate
- Password entries need all fields to be useful
- Invalid data could lead to unusable entries

## Evidence

**TOTP Module** (`components/mod_totp/`):
- `TotpModule.cpp` - 34,046 lines (includes UI wizard)
- `TotpStore.cpp` - 15,332 lines (storage logic)

**Password Module** (`components/mod_password/`):
- `PasswordModule.cpp` - 30,209 lines (includes UI wizard)
- `PasswordStore.cpp` - 11,527 lines (storage logic)

**UI Flow Documentation** (`docs/UI_FLOWS.md:106-127`):
```markdown
### Add Account Wizard

1. **Account Name** → T9 Input
2. **Secret (Base32)** → T9 Input
3. **Issuer** → T9 Input (optional)
4. **Digits** → Select (6/7/8)
5. **Algorithm** → Select (SHA1/SHA256/SHA512)
6. **Period** → Select (30s/60s)
```

**T9 Input View** (used by both wizards):
- Located in `components/cdc_views/`
- Handles multi-tap input, symbols, backspace
- Must work correctly for wizard data entry

**No E2E tests exist** for wizard flows.

## Recommended Fix

Create E2E test file `test/e2e/e2e_wizard_flows.cpp`:

```cpp
#include <unity.h>
#include "mod_totp/TotpModule.h"
#include "mod_totp/TotpStore.h"
#include "mod_password/PasswordModule.h"
#include "mod_password/PasswordStore.h"

using namespace cdc::mod_totp;
using namespace cdc::mod_password;

void setUp() {
    TotpModule::instance().init();
    PasswordModule::instance().init();
}

// ============================================
// TOTP Wizard Tests
// ============================================

void test_totp_add_account_minimal() {
    // Add account with required fields only
    // totp_wizard_start();
    // wizard_inputName("GitHub");
    // wizard_inputSecret("JBSWY3DPEHPK3PXP");
    // wizard_next();  // Skip issuer
    // wizard_selectDigits(6);
    // wizard_selectAlgo(TOTP_SHA1);
    // wizard_selectPeriod(30);
    // wizard_confirm();
    
    // Verify account created
    // uint8_t count = totp_getAccountCount();
    // TEST_ASSERT_EQUAL(1, count);
    
    // Verify code generation
    // const char* code = totp_getCode(0);
    // TEST_ASSERT_NOT_NULL(code);
    // TEST_ASSERT_EQUAL_STRING_LEN("847293", code, 6);
}

void test_totp_add_account_full() {
    // Add account with all fields
    // totp_wizard_start();
    // wizard_inputName("GitHub");
    // wizard_inputSecret("JBSWY3DPEHPK3PXP");
    // wizard_inputIssuer("GitHub");
    // wizard_selectDigits(6);
    // wizard_selectAlgo(TOTP_SHA256);
    // wizard_selectPeriod(30);
    // wizard_confirm();
    
    // Verify account created with all fields
    // TotpAccount acc = totp_getAccount(0);
    // TEST_ASSERT_EQUAL_STRING("GitHub", acc.name);
    // TEST_ASSERT_EQUAL_STRING("GitHub", acc.issuer);
}

void test_totp_wizard_back_navigation() {
    // Start wizard, enter data
    // totp_wizard_start();
    // wizard_inputName("Test");
    // wizard_inputSecret("JBSWY3DPEHPK3PXP");
    
    // Go back to first step
    // wizard_back();
    // wizard_back();
    
    // Enter different data
    // wizard_inputName("Other");
    // wizard_next();
    // wizard_inputSecret("HXDMVJECJJWSRB3H");
    // wizard_confirm();
    
    // Verify only "Other" account exists
    // TEST_ASSERT_EQUAL(1, totp_getAccountCount());
}

void test_totp_wizard_invalid_secret() {
    // Add account with invalid secret
    // totp_wizard_start();
    // wizard_inputName("Test");
    // wizard_inputSecret("INVALID-SECRET!");  // Not Base32
    
    // Should show error or auto-correct
    // TEST_ASSERT_TRUE(wizard_hasError());
}

void test_totp_add_max_accounts() {
    // Add 100 accounts (max capacity)
    // for (int i = 0; i < 100; i++) {
    //     char name[32];
    //     sprintf(name, "Account%d", i);
    //     totp_addAccount(name, "JBSWY3DPEHPK3PXP");
    // }
    
    // TEST_ASSERT_EQUAL(100, totp_getAccountCount());
    
    // Try adding 101st
    // bool added = totp_addAccount("Account101", "JBSWY3DPEHPK3PXP");
    // TEST_ASSERT_FALSE(added);
}

// ============================================
// Password Wizard Tests
// ============================================

void test_password_add_minimal() {
    // Add password with required fields only
    // password_wizard_start();
    // wizard_inputName("GitHub");
    // wizard_inputUser("user@example.com");
    // wizard_inputUrl("github.com");
    // wizard_inputPassword("secret123");
    // wizard_confirm();
    
    // Verify entry created
    // TEST_ASSERT_EQUAL(1, password_getCount());
}

void test_password_add_full() {
    // Add password with all fields
    // password_wizard_start();
    // wizard_inputName("GitHub");
    // wizard_inputUser("user@example.com");
    // wizard_inputUrl("https://github.com");
    // wizard_inputPassword("secret123");
    // wizard_confirm();
    
    // Verify all fields stored
    // PasswordEntry entry = password_get(0);
    // TEST_ASSERT_EQUAL_STRING("GitHub", entry.name);
    // TEST_ASSERT_EQUAL_STRING("user@example.com", entry.user);
    // TEST_ASSERT_EQUAL_STRING("https://github.com", entry.url);
    // TEST_ASSERT_EQUAL_STRING("secret123", entry.password);
}

void test_password_wizard_edit() {
    // Add entry
    // password_add("GitHub", "user", "github.com", "oldpass");
    
    // Edit entry
    // password_edit(0);
    // wizard_inputPassword("newpass");
    // wizard_confirm();
    
    // Verify updated
    // PasswordEntry entry = password_get(0);
    // TEST_ASSERT_EQUAL_STRING("newpass", entry.password);
}

void test_password_wizard_delete() {
    // Add entry
    // password_add("GitHub", "user", "github.com", "pass123");
    
    // Delete entry
    // password_delete(0);
    
    // Verify deleted
    // TEST_ASSERT_EQUAL(0, password_getCount());
}

void test_password_max_entries() {
    // Add 353 entries (max capacity)
    // for (int i = 0; i < 353; i++) {
    //     char name[32];
    //     sprintf(name, "Entry%d", i);
    //     password_add(name, "user", "url", "pass");
    // }
    
    // TEST_ASSERT_EQUAL(353, password_getCount());
    
    // Try adding 354th
    // bool added = password_add("Entry354", "user", "url", "pass");
    // TEST_ASSERT_FALSE(added);
}

extern "C" void app_main() {
    UNITY_BEGIN();
    
    // TOTP tests
    RUN_TEST(test_totp_add_account_minimal);
    RUN_TEST(test_totp_add_account_full);
    RUN_TEST(test_totp_wizard_back_navigation);
    RUN_TEST(test_totp_wizard_invalid_secret);
    RUN_TEST(test_totp_add_max_accounts);
    
    // Password tests
    RUN_TEST(test_password_add_minimal);
    RUN_TEST(test_password_add_full);
    RUN_TEST(test_password_wizard_edit);
    RUN_TEST(test_password_wizard_delete);
    RUN_TEST(test_password_max_entries);
    
    UNITY_END();
}
```

**Helper functions needed:**
- Create wizard test helpers in `test/e2e/wizard_helpers.h`
- Mock T9 input for programmatic testing
- Simulate key presses for UI tests

## References

- [TOTP Module](components/mod_totp/src/TotpModule.cpp) - Wizard implementation
- [Password Module](components/mod_password/src/PasswordModule.cpp) - Wizard implementation
- [UI Flows](docs/UI_FLOWS.md) - TOTP wizard (lines 106-127), Password wizard (lines 135-158)
- [T9 Input View](components/cdc_views/) - T9 input component
