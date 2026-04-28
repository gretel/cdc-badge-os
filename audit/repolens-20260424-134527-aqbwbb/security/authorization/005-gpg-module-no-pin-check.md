---
title: "[MEDIUM] GPG module serial commands lack PIN verification for sensitive operations"
severity: MEDIUM
domain: Authorization & Access Control
lens: authorization
labels:
  - audit:security/authorization
---

## Summary
The GPG module's `GPG_GENERATE` and `GPG_RESET` commands are registered with `requiresAuth = true`, which only checks the serial session PIN. However, these operations should also verify the OpenPGP PW1 (User PIN) or PW3 (Admin PIN) for proper access control to cryptographic functions.

**Location:** `components/mod_gpg/src/GpgModule.cpp:118-124`

## Impact
- **Insufficient authorization:** Serial session authentication doesn't verify OpenPGP-specific PINs
- **Key generation exposure:** Anyone with serial access can generate new GPG keys, potentially overwriting existing keys
- **Key reset exposure:** `GPG_RESET` can clear all GPG key material without PW3 (Admin PIN) verification
- **Missing role-based check:** User PIN (PW1) and Admin PIN (PW3) provide different privilege levels that are not enforced

## Evidence
Command registration at `components/mod_gpg/src/GpgModule.cpp:118-124`:
```cpp
static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;
    auto& registry = cdc::core::getCommandRegistry();
    registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
    registry.registerCommand({"GPG_GENERATE", "Generate GPG keys", cmd_gpg_generate, CMD_MODULE, true});
    registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});
    registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
}
```

All commands use `requiresAuth = true`, which only checks the serial session (badge PIN), not OpenPGP-specific PINs.

Command handler `cmd_gpg_generate` at `components/mod_gpg/src/GpgModule.cpp:151-170`:
```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... argument parsing ...
    
    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);  // No PIN check!
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Command handler `cmd_gpg_reset` at `components/mod_gpg/src/GpgModule.cpp:196-200`:
```cpp
static void cmd_gpg_reset(const char* args) {
    (void)args;
    bool ok = gpg_reset();  // No PIN check!
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

PIN verification functions exist (used by UI):
```cpp
static bool gpg_verify_pw1(const char* pin) {
    return pin_storage_openpgp_verify_pw1(pin);
}

static bool gpg_verify_pw3(const char* pin) {
    return pin_storage_openpgp_verify_pw3(pin);
}
```

But these are not called in the serial command handlers.

## Recommended Fix
Add OpenPGP PIN verification to serial command handlers. Update `components/mod_gpg/src/GpgModule.cpp`:

1. **GPG_GENERATE** should verify PW1 (User PIN):
```cpp
static void cmd_gpg_generate(const char* args) {
    // Verify User PIN (PW1) for key generation
    // Get PW1 from environment or prompt for it
    char pw1[17] = {};  // Max 16 digits + null
    Console::printf("Enter PW1 (User PIN): ");
    // ... read PW1 (hidden input) ...
    if (!gpg_verify_pw1(pw1)) {
        Console::printf("ERROR: PW1 verification failed\r\n");
        return;
    }
    
    // ... rest of command ...
}
```

2. **GPG_RESET** should verify PW3 (Admin PIN):
```cpp
static void cmd_gpg_reset(const char* args) {
    // Verify Admin PIN (PW3) for reset operation
    char pw3[17] = {};
    Console::printf("Enter PW3 (Admin PIN): ");
    // ... read PW3 (hidden input) ...
    if (!gpg_verify_pw3(pw3)) {
        Console::printf("ERROR: PW3 verification failed\r\n");
        return;
    }
    
    bool ok = gpg_reset();
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

Alternatively, create a session-based approach where PW1/PW3 verification is done once and cached:
```cpp
// Add module-level state
static bool s_pw1_verified = false;
static bool s_pw3_verified = false;

// Add AUTH_PW1 and AUTH_PW3 commands
static void cmd_auth_pw1(const char* args) {
    if (gpg_verify_pw1(args)) {
        s_pw1_verified = true;
        Console::printf("OK: PW1 verified\r\n");
    }
}

// Check in GPG_GENERATE
if (!s_pw1_verified && !gpg_verify_pw1(get_cached_pw1())) {
    Console::printf("ERROR: PW1 verification required\r\n");
    return;
}
```

## References
- OpenPGP Smart Card Application Note: https://www.opensc-project.org/opensc/w/images/opensc-card-application-note.pdf
- NIST SP 800-73-4 (PIV Interface): https://csrc.nist.gov/publications/detail/sp/800-73/4/final
- FIDO2 security key best practices: https://fidoalliance.org/specs/fido2/
