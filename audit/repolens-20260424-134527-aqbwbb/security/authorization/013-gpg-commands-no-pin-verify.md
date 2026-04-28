---
title: "[MEDIUM] GPG commands rely on session auth instead of explicit PIN verification"
severity: MEDIUM
domain: authorization
lens: crypto-command-auth
labels:
  - "medium:gpg-auth"
---

## Summary
GPG module commands (`GPG_STATUS`, `GPG_GENERATE`, `GPG_EXPORT`, `GPG_RESET`) use `requiresAuth = true` but do not verify the OpenPGP PINs (PW1/PW3) directly. They rely on session authentication via `FEATURE_SECURE_SERIAL`, which may be disabled by default.

**File:** `components/mod_gpg/src/GpgModule.cpp`  
**Lines:** 118-125, 150-179, 196-204

## Evidence

**Command registration (lines 118-125):**
```cpp
static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;
    auto& registry = cdc::serial::getCommandRegistry();
    registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
    registry.registerCommand({"GPG_GENERATE", "Generate GPG keys", cmd_gpg_generate, CMD_MODULE, true});
    registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});
    registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
}
```

**GPG Generate handler (lines 150-179):**
```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    // ... parse arguments ...
    
    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The commands use `requiresAuth = true`, which only checks `FEATURE_SECURE_SERIAL` session authentication. When disabled, no PIN verification occurs.

## Impact
When `FEATURE_SECURE_SERIAL` is disabled:
1. Anyone can generate new GPG keys (replacing existing ones)
2. Anyone can reset GPG keys
3. Anyone can export public keys (less critical)

For key generation and reset, explicit PW1 (User PIN) or PW3 (Admin PIN) verification should be required per OpenPGP card specification.

## Recommended Fix
Add explicit PIN verification for sensitive operations:

```cpp
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};
    
    // ... parse arguments ...

    // Verify PW1 before generating keys
    auto& pm = core::PinManager::instance();
    char pinBuf[16] = {};
    Console::printf("Enter PW1 (User PIN): ");
    // Read PIN silently
    if (!pm.verifyPW1(pinBuf)) {
        Console::printf("ERROR: PW1 verification failed\r\n");
        return;
    }

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

static void cmd_gpg_reset(const char* args) {
    // Verify PW3 (Admin PIN) for reset
    auto& pm = core::PinManager::instance();
    char pinBuf[16] = {};
    Console::printf("Enter PW3 (Admin PIN) to reset: ");
    if (!pm.verifyPW3(pinBuf)) {
        Console::printf("ERROR: PW3 verification failed\r\n");
        return;
    }
    
    bool ok = gpg_reset();
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## References
- CWE-287: Improper Authentication
- OpenPGP Card Specification v3.0
- NIST SP 800-132: Password-Based Authentication
