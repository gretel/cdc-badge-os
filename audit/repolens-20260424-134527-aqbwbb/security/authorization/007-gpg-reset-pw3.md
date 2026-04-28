---
title: "[LOW] GPG reset command does not require PW3 (Admin PIN) verification"
severity: LOW
domain: authorization
lens: gpg-reset
labels:
  - "gpg"
  - "reset"
---

## Summary

The `GPG_RESET` serial command (defined in `components/mod_gpg/src/GpgModule.cpp:196-202`) resets all GPG key material but **does not require PW3 (Admin PIN) verification** before execution. The command is registered with `requiresAuth = true`, but this only checks for serial authentication, not the OpenPGP-specific Admin PIN.

Command registration:
```cpp
// Line 119: GPG_RESET command
registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
```

Command handler:
```cpp
// Lines 196-202
static void cmd_gpg_reset(const char* args) {
    (void)args;
    bool ok = gpg_reset();
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The `gpg_reset()` function is called without first verifying PW3:
```cpp
// components/mod_gpg/src/gpg.cpp (gpg_reset implementation)
bool gpg_reset() {
    // Reset logic without PW3 check
    // ...
}
```

Compare this to the `cmd_gpg_generate` function which correctly checks PW3 via the APDU interface:
```cpp
// Line 1472 in openpgp.cpp
if (apdu->p1 == 0x80) {  // Generate new key
    if (!pw3_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }
    // ...
}
```

## Impact

1. **Unauthorized reset** - Any user authenticated via serial (not OpenPGP PW3) can reset all GPG keys.

2. **Data loss** - Resetting GPG keys destroys all key material, requiring re-generation and re-distribution of public keys.

3. **Inconsistent authorization** - The APDU interface (via CCID) requires PW3 for key generation, but the serial command bypasses this check.

## Evidence

**File: `components/mod_gpg/src/GpgModule.cpp`**
- Lines 119-121: Command registration with `requiresAuth = true` (serial auth only)
- Lines 196-202: `cmd_gpg_reset` handler without PW3 verification

**File: `components/mod_gpg/src/openpgp/openpgp.cpp`**
- Line 1472: PW3 check in APDU `GENERATE_KEYPAIR` command (for comparison)
- Lines 895-900: `cmd_generate_attributes` requires PW3

**File: `components/mod_gpg/src/gpg.cpp`**
- `gpg_reset()` implementation without PW3 check

## Recommended Fix

1. **Add PW3 verification to GPG_RESET**:
   ```cpp
   static void cmd_gpg_reset(const char* args) {
       // Check PW3 verification status (from OpenPGP session)
       // Note: Need to expose PW3 status to serial commands
       bool pw3_verified = pin_storage_openpgp_pw3_retries() < 3; // Not accurate
       
       // Better approach: Require explicit PW3 entry
       // Or check if OpenPGP session has PW3 verified
       
       if (!gpg_is_pw3_verified()) {  // New function to check PW3 status
           cdc::serial::Console::printf("ERROR: Admin PIN (PW3) required\r\n");
           return;
       }
       
       bool ok = gpg_reset();
       cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
   }
   ```

2. **Add a confirmation prompt**:
   ```cpp
   static void cmd_gpg_reset(const char* args) {
       if (!args || strcmp(args, "CONFIRM") != 0) {
           cdc::serial::Console::printf("WARNING: This will reset all GPG keys!\r\n");
           cdc::serial::Console::printf("To proceed, type: GPG_RESET CONFIRM\r\n");
           return;
       }
       // ... rest of reset logic
   }
   ```

3. **Expose PW3 verification status** - Add a function to check if PW3 is verified:
   ```cpp
   // In gpg.h
   bool gpg_is_pw3_verified(void);
   
   // In gpg.cpp
   bool gpg_is_pw3_verified(void) {
       return pin_storage_openpgp_pw3_retries() < 3; // Or track session state
   }
   ```

## References

- [OpenPGP Card Specification 3.4.1 - Key Generation](https://g10code.com/docs/openpgp-card-3.4.pdf)
- [OpenPGP Card Specification 3.4.1 - Resetting Keys](https://g10code.com/docs/openpgp-card-3.4.pdf)
