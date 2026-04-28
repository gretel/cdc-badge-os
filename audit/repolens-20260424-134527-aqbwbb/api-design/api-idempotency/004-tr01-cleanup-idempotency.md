---
title: "[LOW] TR01_CLEANUP and TR01_WIPE commands lack idempotency tracking"
severity: LOW
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The `TR01_CLEANUP` and `TR01_WIPE` serial commands perform destructive operations without tracking whether they were already executed. Running `TR01_WIPE CONFIRM` multiple times produces different output (first run shows deleted counts, subsequent runs show 0 deleted), which can confuse users about whether the operation succeeded.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:1109-1188` (cmdTr01Wipe), `components/serial_cmd/src/SerialCmd.cpp:1115-1123` (cmdTr01Cleanup)

## Impact
- **Confusing feedback:** Subsequent runs show "Deleted: 0 ECC keys, 0 R-Memory slots" which looks like a no-op
- **No operation idempotency marker:** No way to tell if cleanup/wipe was already performed
- **Potential for accidental data loss:** User might run Wipe twice thinking first didn't work (though data is already gone)

## Evidence
```cpp
// components/serial_cmd/src/SerialCmd.cpp:1133-1188
static void cmdTr01Wipe(const char* args) {
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    if (!args || strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: This will ERASE ALL data on TROPIC01!\r\n");
        // ...
        return;
    }

    Console::printf("=== TROPIC01 Factory Reset ===\r\n");
    Console::flush();

    uint16_t eccDeleted = 0;
    uint16_t rmemDeleted = 0;

    // Delete all ECC keys
    for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
        if (se->eccSlotUsed(i)) {
            if (se->eccDelete(i) == hal::SeResult::OK) {
                eccDeleted++;
            }
        }
    }
    Console::printf("  Deleted %d ECC keys\r\n", eccDeleted);

    // Erase R-Memory slots
    for (uint16_t i = 0; i < hal::ISecureElement::RMEM_SLOT_COUNT; i++) {
        if (se->rmemSlotUsed(i)) {
            if (se->rmemErase(i) == hal::SeResult::OK) {
                rmemDeleted++;
            }
        }
    }
    Console::printf("  Deleted %d R-Memory slots\r\n", rmemDeleted);

    Console::printf("\r\n=== Factory Reset Complete ===\r\n");
    Console::printf("Deleted: %d ECC keys, %d R-Memory slots\r\n", eccDeleted, rmemDeleted);
}
```

Running this twice:
- First run: `Deleted: 15 ECC keys, 42 R-Memory slots`
- Second run: `Deleted: 0 ECC keys, 0 R-Memory slots`

## Recommended Fix
Add idempotency tracking via NVS to indicate operation completion:

1. **Store wipe timestamp** in NVS after successful wipe
2. **Check NVS** at start to report if already wiped
3. **Add `FORCE` flag** for explicit re-wiping

Example:
```cpp
#define NVS_KEY_LAST_WIPE "last_wipe_ts"

static void cmdTr01Wipe(const char* args) {
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    // Check if already wiped recently
    nvs_handle_t nvs;
    if (nvs_open("system", NVS_READONLY, &nvs) == ESP_OK) {
        uint32_t last_wipe;
        if (nvs_get_u32(nvs, NVS_KEY_LAST_WIPE, &last_wipe) == ESP_OK) {
            uint32_t age_sec = esp_timer_get_time() / 1000 - last_wipe;
            Console::printf("NOTE: Device was wiped %lu seconds ago.\r\n", (unsigned long)age_sec);
        }
    }
    nvs_close(nvs);

    if (!args || strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: This will ERASE ALL data on TROPIC01!\r\n");
        Console::printf("  Use 'TR01_WIPE CONFIRM' to proceed.\r\n");
        Console::printf("  Use 'TR01_WIPE FORCE' to re-wipe if already wiped.\r\n");
        return;
    }

    // ... perform wipe ...

    // Store wipe timestamp
    if (nvs_open("system", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u32(nvs, NVS_KEY_LAST_WIPE, esp_timer_get_time() / 1000);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    Console::printf("\r\n=== Factory Reset Complete ===\r\n");
    Console::printf("Deleted: %d ECC keys, %d R-Memory slots\r\n", eccDeleted, rmemDeleted);
    Console::printf("Timestamp: %lu seconds since epoch\r\n", (unsigned long)last_wipe);
}
```

## References
- NVS (Non-Volatile Storage) is already used for FIDO2 auth counter
- Similar pattern: `cmdNvsClear` requires "YES" confirmation
- Idempotency in destructive operations: track state to prevent confusion
