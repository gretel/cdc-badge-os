---
title: "[MEDIUM] FIDO2 CTAPHID protocol lacks integration tests for request/response cycle"
severity: MEDIUM
domain: protocols
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:mod_fido2"
  - "area:fido2"
---

## Summary
The `Fido2Ui` component (`components/mod_fido2/src/Fido2Ui.cpp`) implements the FIDO2 UI workflow including CTAPHID protocol handling, but **no integration tests** verify complete request/response cycles from HID input through to signing and response output.

## Impact
- **Protocol bugs**: CTAPHID packet parsing may fail for edge cases
- **User presence**: UP confirmation flow may not work correctly
- **PIN auth**: PIN protocol v2 may not be implemented correctly
- **Credential storage**: Credentials may not persist correctly
- **Response formatting**: CTAPHID responses may not match spec

## Evidence

**FIDO2 UI flow** (`components/mod_fido2/src/Fido2Ui.cpp:100-400`):
```cpp
static SemaphoreHandle_t s_promptSem;
static volatile fido2_user_presence_result_t s_promptResult;

// User presence prompt
static bool promptUserPresence(fido2_action_t action, const char* rpId) {
    // Show UI for user to press Y key
    // Wait for confirmation
    xSemaphoreTake(s_promptSem, portMAX_DELAY);
    return s_promptResult == FIDO2_UP_CONFIRMED;
}

// Sign callback
static void onSignComplete(uint8_t* sig, size_t sigLen) {
    // Send response back via CTAPHID
}
```

**CTAPHID protocol** (`components/mod_fido2/src/ctaphid.cpp`):
```cpp
// HID packet handling
void ctaphid_init();
void ctaphid_process();
void ctaphid_send(uint16_t cmd, const uint8_t* data, uint16_t len);
void ctaphid_recv(uint8_t* buf, uint16_t* len);
```

**FIDO2 operations** (`components/mod_fido2/src/fido2.cpp`):
```cpp
// Registration
bool fido2_register(const uint8_t* rpId, uint8_t rpIdLen,
                    const uint8_t* challenge, uint8_t challengeLen);

// Authentication
bool fido2_sign(const uint8_t* rpId, uint8_t rpIdLen,
                const uint8_t* challenge, uint8_t challengeLen,
                const uint8_t* credentialId, uint8_t credIdLen);
```

**PIN protocol** (`components/mod_fido2/src/pin_storage.cpp`):
```cpp
// PIN protocol v2
bool pinStorage_encryptPin(const char* pin, uint8_t* encrypted, uint8_t* key);
bool pinStorage_decryptPin(const uint8_t* encrypted, const uint8_t* key, char* pin);
```

**Usage in Fido2Module** (`components/mod_fido2/src/Fido2Module.cpp`):
```cpp
// Module integrates CTAPHID with USB HID
Fido2Ui::init();
Fido2Ui::start();
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_fido2_protocol/` that verifies:

1. **CTAPHID init**: HID interface initializes correctly
2. **Register flow**: Complete registration with storage
3. **Sign flow**: Complete authentication with signing
4. **User presence**: UP prompt and confirmation works
5. **PIN auth**: PIN encryption/decryption for auth
6. **Credential operations**: Get info, list credentials

**Test structure** (example):
```cpp
// test/test_fido2_protocol/test_fido2_flow.cpp
#include "mod_fido2/Fido2Ui.h"
#include "mod_fido2/fido2.h"

void test_fido2_register_and_sign() {
    // Initialize
    mod_fido2::Fido2Ui::init();
    mod_fido2::Fido2Ui::start();
    
    // Register credential
    const char* rpId = "example.com";
    uint8_t challenge[32];
    esp_fill_random(challenge, 32);
    
    uint8_t credId[32];
    bool success = mod_fido2::fido2_register(
        (uint8_t*)rpId, strlen(rpId),
        challenge, sizeof(challenge),
        credId
    );
    ASSERT_TRUE(success);
    
    // Sign with credential
    uint8_t signature[64];
    success = mod_fido2::fido2_sign(
        (uint8_t*)rpId, strlen(rpId),
        challenge, sizeof(challenge),
        credId, sizeof(credId),
        signature
    );
    ASSERT_TRUE(success);
}

void test_fido2_user_presence() {
    // Simulate UP prompt
    bool confirmed = mod_fido2::Fido2Ui::promptUserPresence(
        mod_fido2::FIDO2_ACTION_AUTHENTICATE,
        "example.com"
    );
    // In test, can simulate key press
    ASSERT_TRUE(confirmed);
}
```

## References
- [Fido2Ui implementation](components/mod_fido2/src/Fido2Ui.cpp)
- [CTAPHID protocol](components/mod_fido2/src/ctaphid.cpp)
- [FIDO2 operations](components/mod_fido2/src/fido2.cpp)
- [PIN storage](components/mod_fido2/src/pin_storage.cpp)

</content>