---
title: "[MEDIUM] TR01_CACHE_REBUILD exposes slot names and module ownership without authentication"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `TR01_CACHE_REBUILD` serial command rebuilds the TROPIC01 R-Memory cache and outputs slot names and module ownership information without authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker with serial access can enumerate all populated R-Memory slots, revealing the structure of stored data (TOTP accounts, password entries, etc.).

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1479` (registration), `components/serial_cmd/src/SerialCmd.cpp:1088-1112` (handler)

## Impact
- **Data Structure Disclosure**: Reveals which R-Memory slots are populated and their names
- **Reconnaissance**: Attacker can map out:
  - TOTP account names (slots 32-131)
  - Password vault entries (slots 159-511)
  - FIDO2 credential names (slots 132-158)
- **Targeted Attacks**: Knowledge of slot structure enables more targeted attacks on specific data
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at line 1479
reg.registerCommand({"TR01_CACHE_REBUILD", "Rebuild TR01 cache", cmdTr01CacheRebuild, "tr01", false});
// The last 'false' means requiresAuth = false

// Handler at lines 1088-1112
static void cmdTr01CacheRebuild(const char* args) {
    (void)args;
    auto& storage = core::TropicStorage::instance();
    Console::printf("Rebuilding TR01 cache...\r\n");

    auto logFn = [](uint16_t slot, const char* message, void* ctx) {
        (void)ctx;
        if (!message) return;
        if (strcmp(message, "invalid header") == 0 ||
            strcmp(message, "mismatched module") == 0 ||
            strcmp(message, "nvs write failed") == 0 ||
            strcmp(message, "session start failed") == 0 ||
            strcmp(message, "read failed") == 0) {
            Console::printf("  slot %u: %s\r\n", slot, message);
        } else {
            Console::printf("  slot %u: found %s\r\n", slot, message);  // <-- Exposes slot name!
        }
    };

    if (storage.rebuildVerbose(logFn, nullptr)) {
        Console::printf("OK: Cache rebuilt\r\n");
    } else {
        Console::printf("ERROR: Cache rebuilt failed\r\n");
    }
}
```

The `rebuildVerbose` function at `components/cdc_core/src/TropicStorage.cpp:254` calls the log function with slot names:
```cpp
// components/cdc_core/src/TropicStorage.cpp:240-254
cdc::hal::ISecureElement::RMemHeader header = {};
uint16_t payloadLen = 0;
auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
if (res == cdc::hal::SeResult::OK) {
    if (!isEntryAllowed(slot, header.moduleId)) {
        if (logFn) logFn(slot, "mismatched module", ctx);
        continue;
    }

    CacheEntry& entry = chunk[i];
    entry.moduleId = header.moduleId;
    entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
    strncpy(entry.name, header.name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    if (logFn) logFn(slot, entry.name, ctx);  // <-- Logs the name!
    continue;
}
```

Example output revealing data structure:
```
Rebuilding TR01 cache...
  slot 32: found google
  slot 33: found github
  slot 34: found amazon
  slot 132: found ssh-key-2024
  slot 133: found work-ssh
  slot 159: found aws-prod
  slot 160: found db-password
OK: Cache rebuilt
```

## Recommended Fix
Add authentication requirement to the `TR01_CACHE_REBUILD` command:

```cpp
// Change registration from:
reg.registerCommand({"TR01_CACHE_REBUILD", "Rebuild TR01 cache", cmdTr01CacheRebuild, "tr01", false});

// To:
reg.registerCommand({"TR01_CACHE_REBUILD", "Rebuild TR01 cache", cmdTr01CacheRebuild, "tr01", true});
```

Alternatively, modify the handler to only print slot names when authenticated:
```cpp
static void cmdTr01CacheRebuild(const char* args) {
    (void)args;
    auto& storage = core::TropicStorage::instance();
    Console::printf("Rebuilding TR01 cache...\r\n");

    auto logFn = [](uint16_t slot, const char* message, void* ctx) {
        (void)ctx;
        if (!message) return;
        if (strcmp(message, "invalid header") == 0 ||
            strcmp(message, "mismatched module") == 0 ||
            strcmp(message, "nvs write failed") == 0 ||
            strcmp(message, "session start failed") == 0 ||
            strcmp(message, "read failed") == 0) {
            Console::printf("  slot %u: %s\r\n", slot, message);
        } else {
#if FEATURE_SECURE_SERIAL
            if (SerialCmd::isAuthenticated()) {
                Console::printf("  slot %u: found %s\r\n", slot, message);
            } else {
                Console::printf("  slot %u: found (auth required for name)\r\n", slot);
            }
#else
            Console::printf("  slot %u: found %s\r\n", slot, message);
#endif
        }
    };

    if (storage.rebuildVerbose(logFn, nullptr)) {
        Console::printf("OK: Cache rebuilt\r\n");
    } else {
        Console::printf("ERROR: Cache rebuilt failed\r\n");
    }
}
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1479` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:1088-1112` - Command handler
- `components/cdc_core/src/TropicStorage.cpp:217-275` - rebuildVerbose implementation
- `components/cdc_core/src/TropicStorage.cpp:254` - Slot name logging
- CWE-200: Exposure of sensitive information to an unauthorized actor
- CWE-497: Exposure of system information to an outside actor
