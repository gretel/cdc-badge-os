---
title: "[LOW] NVS_DEL command has ambiguous syntax for namespace vs key deletion"
severity: LOW
domain: database
lens: query-safety
labels:
  - audit:database/query-safety
---

## Summary
The `NVS_DEL` command (line 610-654 in `components/serial_cmd/src/SerialCmd.cpp`) uses positional arguments where the second argument can be either a key name or `CONFIRM`. This creates potential for accidental namespace erasure if users type the wrong number of arguments.

**Evidence:**
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Lines: 617-634
```cpp
char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
char key[NVS_KEY_MAX_LEN + 1] = {0};
int parsed = sscanf(args, "%15s %15s", ns, key);

if (parsed < 1) {
    Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
    return;
}

// ...

if (parsed == 1 || key[0] == '\0') {
    err = nvs_erase_all(nvs);  // Erases entire namespace!
    // ...
}
```

## Impact
- **Accidental Data Loss**: User might type `NVS_DEL wifi CONFIRM` expecting to delete a key named "CONFIRM" but instead erases the entire "wifi" namespace
- **Confusing UX**: The syntax `NVS_DEL <namespace> [key]` suggests key is optional, but without a key it erases everything
- **No Safety Net**: The word "CONFIRM" is not treated specially for namespace erasure

## Recommended Fix
1. Make syntax more explicit:
   - `NVS_DEL <namespace> <key>` - deletes single key
   - `NVS_DEL <namespace> --all` or `NVS_DEL <namespace> --force` - erases namespace

2. Or keep current syntax but add special handling:
```cpp
if (parsed == 1 || key[0] == '\0') {
    // Namespace erase - require explicit confirmation
    Console::printf("WARNING: Will erase entire namespace '%s'!\r\n", ns);
    Console::printf("Type: NVS_DEL %s CONFIRM\r\n", ns);
    return;
} else if (strcmp(key, "CONFIRM") == 0 && parsed == 2) {
    // User wants to erase namespace, not a key named CONFIRM
    // (Need to handle this case differently)
}
```

3. Better: Use explicit flags
```cpp
// Usage: NVS_DEL <namespace> <key> [--force]
// Or:    NVS_DEL <namespace> --all [--force]
```

## References
- Similar pattern: `NVS_CLEAR YES` uses explicit confirmation
- ESP-IDF NVS API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
