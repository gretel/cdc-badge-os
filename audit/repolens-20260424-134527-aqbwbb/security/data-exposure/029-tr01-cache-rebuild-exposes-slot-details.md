---
title: "[MEDIUM] TR01_CACHE_REBUILD command exposes detailed slot metadata"
severity: MEDIUM
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `TR01_CACHE_REBUILD` serial command outputs detailed information about each secure element slot including:
- Slot numbers
- Module associations
- Data type (ECC vs R-Memory)
- Metadata validation status

This information reveals the internal structure of how modules use the TROPIC01 secure element.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:1080-1112`

Example output from `cmdTr01CacheRebuild`:
```cpp
Console::printf("  slot %u: found %s\r\n", slot, message);
Console::printf("  slot %u: %s\r\n", slot, message);
```

## Impact
- **Slot enumeration**: Attacker can map which slots are used by which modules
- **Structure fingerprinting**: Reveals how FIDO2, GPG, TOTP, Password modules organize data
- **Metadata leakage**: Shows module IDs and slot associations
- **Debug info in production**: Detailed rebuild info may reveal internal state

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp:1080-1112`
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
            Console::printf("  slot %u: found %s\r\n", slot, message);  // Exposes module data
        }
    };
    ...
}
```

## Recommended Fix
1. Make TR01_CACHE_REBUILD a privileged command (requires authentication)
2. Reduce verbosity of slot information in production builds
3. Add a build flag to suppress detailed slot info
4. Consider providing only summary statistics instead of per-slot details

Example fix:
```cpp
// Add to registerCommand for privileged status
reg.registerCommand({"TR01_CACHE_REBUILD", "Rebuild TR01 cache", cmdTr01CacheRebuild, "tr01", true});

// Or add conditional output
#if DEBUG_MODE
    Console::printf("  slot %u: found %s\r\n", slot, message);
#else
    Console::printf("  slot %u: OK\r\n", slot);
#endif
```

## References
- TROPIC01 slot allocation: `main/tropic_slot_map.h`
- Related to issue #18 (NVS data exposure)
- Secure element commands: `docs/SERIAL_COMMANDS.md`
