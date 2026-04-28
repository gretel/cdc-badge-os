---
title: "[MEDIUM] FIDO2 session state not cleared on module stop"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
When the FIDO2 module is stopped (via `Fido2Module::stop()`), the ClientPIN session state including `pin_token` and `pin_verified` flag remains in memory. This allows authenticated operations to continue even after the module should have been "stopped."

**File**: `components/mod_fido2/src/Fido2Module.cpp:201-204`
```cpp
void Fido2Module::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;
}
```

The `stop()` method only unregisters the USB HID interface but does NOT clear session state.

**File**: `components/mod_fido2/src/ctap2.cpp:97-116`
```cpp
static struct {
    // ECDH key pair (generated on init, regenerated on reset)
    mbedtls_ecp_keypair ecdh_key;
    bool ecdh_valid;

    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];
    bool pin_token_valid;

    // Permission flags for pinUvAuthToken
    uint8_t token_permissions;
    bool token_rp_id_set;
    char token_rp_id[64];

    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;
} g_client_pin = {};
```

The `g_client_pin` struct holds session state that persists across stop/start cycles.

**File**: `components/mod_fido2/src/fido2.cpp:27-40`
```cpp
static struct {
    bool initialized;
    bool operation_pending;
    bool cancelled;
    bool pin_verified;  // Also persists!
    uint8_t pin_uv_auth_protocols;
    uint8_t pin_uv_auth_param[64];
    size_t pin_uv_auth_param_len;
} g_fido2 = {};
```

## Impact
- **Session Persistence**: Even after "stopping" the FIDO2 module, the pin_token remains valid in RAM
- **USB Re-registration**: If the module is restarted, the old session state is still active
- **Memory Exposure**: Session key material stays in memory longer than necessary
- **Inconsistent State**: The module appears stopped but authentication state persists

## Evidence
**File**: `components/mod_fido2/src/Fido2Module.cpp:157-195`
```cpp
bool Fido2Module::start() {
    if (state_ == core::ServiceState::STARTED ||
        state_ != core::ServiceState::STOPPED) {
        return true;
    }

    // Register USB HID interface
    uint8_t instance = core::UsbManager::instance().registerInterface(
        core::UsbHidInterface::Fido, getName());
    if (instance == 0xFF) {
        LOG_E(TAG, "USB interface registration failed");
        return false;
    }

    // Initialize storage with assigned slots
    uint16_t eccCount = static_cast<uint16_t>(slotRange_.eccEnd - slotRange_.eccStart + 1);
    uint16_t rmemCount = static_cast<uint16_t>(slotRange_.rmemEnd - slotRange_.rmemStart + 1);

    fido2_storage_set_slot_range(slotRange_.eccStart, slotRange_.eccEnd,
                                 slotRange_.rmemStart, slotRange_.rmemEnd);
    fido2_init();  // Initializes but doesn't clear session state

    state_ = core::ServiceState::STARTED;
    return true;
}
```

The `start()` method calls `fido2_init()` but does not clear existing session state.

**File**: `components/mod_fido2/src/ctap2.cpp:2874-2880`
```cpp
// Initialize if needed
if (!g_client_pin.initialized) {
    client_pin_init_ecdh();
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    g_client_pin.initialized = true;
}
```

Session state is only initialized once; subsequent starts reuse existing state.

**File**: `components/mod_fido2/src/fido2.cpp:124-156`
```cpp
bool fido2_init(void) {
    LOG_I("FIDO2", "Initializing...");

    // Initialize storage layer
    uint8_t id = fido2_storage_init();
    LOG_I("FIDO2", "Storage initialized, %d credentials", id);

    // Initialize CTAP2 protocol handler
    if (!ctap2_init()) {
        LOG_E("FIDO2", "CTAP2 init failed");
        return false;
    }

    // Initialize CTAPHID transport
    if (!ctaphid_init()) {
        LOG_E("FIDO2", "CTAPHID init failed");
        return false;
    }

    // Initialize U2F attestation certificate
    if (!u2f_init_attestation()) {
        LOG_W("FIDO2", "U2F attestation init failed (non-fatal)");
    }

    g_fido2.initialized = true;
    LOG_I("FIDO2", "Initialized");
    return true;
}
```

No session clearing in `fido2_init()`.

## Recommended Fix
Clear session state in `stop()` and ensure clean initialization in `start()`:

**File**: `components/mod_fido2/src/ctap2.cpp`
```cpp
// Add function to clear ClientPIN session
static void client_pin_clear_session(void) {
    g_client_pin.pin_token_valid = false;
    g_client_pin.token_permissions = 0;
    g_client_pin.token_rp_id_set = false;
    mbedtls_platform_zeroize(g_client_pin.pin_token, PIN_TOKEN_SIZE);
    mbedtls_platform_zeroize(g_client_pin.token_rp_id, sizeof(g_client_pin.token_rp_id));
    LOG_I("PIN", "ClientPIN session cleared");
}

// Update client_pin_init_ecdh to also clear session
static bool client_pin_init_ecdh(void) {
    // Clear existing session first
    client_pin_clear_session();
    
    mbedtls_ecp_keypair_init(&g_client_pin.ecdh_key);
    int ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                                   &g_client_pin.ecdh_key,
                                   ctap2_random, NULL);
    if (ret != 0) {
        LOG_E("PIN", "ECDH key generation failed: %d", ret);
        return false;
    }

    g_client_pin.ecdh_valid = true;
    g_client_pin.pin_retries = PIN_RETRIES_MAX;
    g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
    g_client_pin.initialized = true;
    LOG_I("PIN", "ECDH key pair generated, session reset");
    return true;
}
```

**File**: `components/mod_fido2/src/fido2.cpp`
```cpp
// Add function to clear FIDO2 session state
void fido2_clear_session(void) {
    g_fido2.pin_verified = false;
    g_fido2.pin_uv_auth_param_len = 0;
    mbedtls_platform_zeroize(g_fido2.pin_uv_auth_param, sizeof(g_fido2.pin_uv_auth_param));
    LOG_I("FIDO2", "Session cleared");
}

// Update fido2_init to clear session
bool fido2_init(void) {
    LOG_I("FIDO2", "Initializing...");

    // Clear any existing session state
    fido2_clear_session();

    // Initialize storage layer
    uint8_t id = fido2_storage_init();
    LOG_I("FIDO2", "Storage initialized, %d credentials", id);

    // ... rest of existing init code ...
}
```

**File**: `components/mod_fido2/src/Fido2Module.cpp`
```cpp
#include "mod_fido2/fido2.h"
#include "mod_fido2/ctap2.h"  // For client_pin_clear_session

void Fido2Module::stop() {
    // Clear session state before stopping
    fido2_clear_session();
    
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;
    LOG_I(TAG, "FIDO2 module stopped, session cleared");
}
```

## References
- FIDO CTAP 2.1 Specification - ClientPIN Protocol
- OWASP Session Management Cheat Sheet - Session Termination
- NIST SP 800-63B - Session Management

</content>