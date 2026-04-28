---
title: "[MEDIUM] ctap2.cpp combines CTAP2 protocol, PIN handling, credential management, and FIDO2 operations"
severity: MEDIUM
domain: mod_fido2
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/mod_fido2/src/ctap2.cpp` (3520 lines - the largest file in the codebase) handles multiple distinct responsibilities:
1. **CTAP2 command processing** - `authenticatorMakeCredential()`, `authenticatorGetAssertion()`, `authenticatorGetInfo()`
2. **ClientPIN protocol** - PIN token management, `PIN_CMD_GET_PIN_TOKEN`, `PIN_CMD_CHANGE_PIN`
3. **Credential management** - `cred_mgmt_get_credentials_metadata()`, `cred_mgmt_delete_credential()`
4. **PIN storage** - `pin_storage.cpp` logic integrated directly
5. **CBOR encoding helpers** - Various CBOR helpers for CTAP2 messages
6. **State management** - Global `g_ctap2`, `g_client_pin`, `g_cred_mgmt` structures

## Impact
- **Extreme complexity**: 3520 lines makes it the largest source file
- **High coupling**: PIN protocol, credential management, and CTAP2 commands all intertwined
- **Testing difficulty**: Cannot test credential management without full CTAP2 setup
- **Merge conflicts**: Multiple developers working on different CTAP2 aspects will conflict
- **Hard to maintain**: New CTAP2 features will make it even larger

## Evidence
File: `components/mod_fido2/src/ctap2.cpp`
- Lines 50-150: Global state structures (`g_ctap2`, `g_client_pin`, `g_cred_mgmt`)
- Lines 152-300: Helper functions (random, CBOR helpers, auth data building)
- Lines 300-1000: CTAP2 command processing (makeCredential, getAssertion)
- Lines 1000-2000: ClientPIN protocol (PIN token, verification)
- Lines 2000-3000: Credential management (enumerate, delete)
- Lines 3000-3520: Response formatting and finalization

Key global state showing mixed concerns:
```cpp
// CTAP2 runtime state
static struct {
    bool initialized;
    bool operation_pending;
    bool cancelled;
    uint8_t assertion_creds[FIDO2_MAX_CREDENTIALS];
    uint8_t assertion_count;
    // ...
} g_ctap2 = {};

// ClientPIN state
static struct {
    bool initialized;
    mbedtls_ecp_keypair ecdh_key;
    uint8_t pin_token[PIN_TOKEN_SIZE];
    uint8_t token_permissions;
    uint8_t pin_retries;
    // ...
} g_client_pin = {};

// Credential management state
static struct {
    uint8_t rp_slots[FIDO2_MAX_CREDENTIALS];
    uint8_t rp_count;
    uint8_t cred_slots[FIDO2_MAX_CREDENTIALS];
    // ...
} g_cred_mgmt = {};
```

## Recommended Fix
Split into focused modules:
1. **CtapCore** - CTAP2 protocol commands in `components/mod_fido2/src/CtapCore.cpp`
2. **CtapPin** - ClientPIN protocol in `components/mod_fido2/src/CtapPin.cpp`
3. **CtapCredMgmt** - Credential management in `components/mod_fido2/src/CtapCredMgmt.cpp`
4. **CtapHelpers** - CBOR helpers and common functions in `components/mod_fido2/src/CtapHelpers.cpp`

Each module should:
- Have its own header file with specific interface
- Share state via explicit dependencies, not globals
- Be testable in isolation

Example split structure:
```
components/mod_fido2/src/
  ctap2.cpp         // Main orchestration, delegates to sub-modules
  CtapCore.cpp      // CTAP2 command processing
  CtapPin.cpp       // ClientPIN protocol
  CtapCredMgmt.cpp  // Credential management
  CtapHelpers.cpp   // CBOR helpers, common functions
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- CTAP2 Specification: https://fidoalliance.org/specs/fido2/
- Clean Architecture: https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html
