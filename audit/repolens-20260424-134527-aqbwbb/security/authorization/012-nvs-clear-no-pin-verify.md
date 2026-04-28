---
title: "[HIGH] NVS_CLEAR destructive command lacks fresh PIN verification"
severity: HIGH
domain: authorization
lens: destructive-command-auth
labels:
  - "high:nvs-auth"
---

## Summary
The `NVS_CLEAR` command erases all Non-Volatile Storage (NVS) data but only relies on the `requiresAuth` flag. When `FEATURE_SECURE_SERIAL` is disabled (default), no authentication is required, allowing any serial connection to wipe all device settings.

**File:** `components/serial_cmd/src/SerialCmd.cpp`  
**Lines:** 493-516, 1453

## Evidence

**Command registration (line 1453):**
```cpp
reg.registerCommand({"NVS_CLEAR", "Erase entire NVS (NVS_CLEAR YES)", cmdNvsClear, "nvs", true});
```

**Handler implementation (lines 493-516):**
```cpp
static void cmdNvsClear(const char* args) {
    if (!args || strcmp(args, "YES") != 0) {
        Console::printf("WARNING: This will ERASE ALL NVS data!\r\n");
        Console::printf("  - All module settings\r\n");
        Console::printf("  - All stored preferences\r\n");
        Console::printf("  - WiFi credentials\r\n");
        Console::printf("  - Timezone settings\r\n");
        Console::printf("\r\nTo proceed, type: NVS_CLEAR YES\r\n");
        return;
    }

    Console::printf("Clearing NVS...\r\n");
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
        return;
    }
    err = nvs_flash_init();
    // ...
}
```

The command uses `requiresAuth = true`, which only checks `FEATURE_SECURE_SERIAL` session authentication. When the feature is disabled, no authentication is required.

## Impact
An attacker with serial access can:
1. Erase all module settings (GPG, FIDO2, TOTP, Password configurations)
2. Delete WiFi credentials
3. Reset timezone and other preferences
4. Force full device reconfiguration

While less severe than wiping the secure element, this causes complete loss of all non-volatile settings.

## Recommended Fix
Add explicit PIN verification before executing the NVS clear:

```cpp
static void cmdNvsClear(const char* args) {
    if (!args || strcmp(args, "YES") != 0) {
        Console::printf("WARNING: This will ERASE ALL NVS data!\r\n");
        Console::printf("  - All module settings\r\n");
        Console::printf("  - All stored preferences\r\n");
        Console::printf("  - WiFi credentials\r\n");
        Console::printf("  - Timezone settings\r\n");
        Console::printf("\r\nTo proceed, type: NVS_CLEAR YES\r\n");
        return;
    }

    // Require fresh PIN verification
    auto& pm = core::PinManager::instance();
    char pinBuf[16] = {};
    Console::printf("Enter Badge PIN to confirm: ");
    // Read PIN silently (implement silent input)
    if (!pm.verifyBadgePin(pinBuf)) {
        Console::printf("ERROR: PIN verification failed\r\n");
        return;
    }

    Console::printf("Clearing NVS...\r\n");
    esp_err_t err = nvs_flash_erase();
    // ...
}
```

## References
- CWE-287: Improper Authentication
- CWE-663: Use of a One-Time Key for a Long-Lived Key
- ESP32 NVS Documentation
