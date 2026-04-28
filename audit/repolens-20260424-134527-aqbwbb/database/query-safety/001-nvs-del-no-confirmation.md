---
title: "[MEDIUM] NVS namespace deletion without confirmation in Serial command"
severity: MEDIUM
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The serial command `NVS_DEL` at `components/serial_cmd/src/SerialCmd.cpp:610-652` allows deletion of an entire NVS namespace (all keys) without requiring explicit confirmation, unlike `NVS_CLEAR` which requires `YES` confirmation.

## Impact
- **Data Loss Risk**: A single command `NVS_DEL <namespace>` can erase all keys in a namespace without confirmation
- **Critical Data at Risk**: Module settings, authentication data, and user preferences stored in NVS could be accidentally deleted
- **Inconsistent Safety**: `NVS_CLEAR` (line 489) requires `NVS_CLEAR YES` confirmation, but `NVS_DEL` for namespaces does not

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`

Lines 610-652 (cmdNvsDel function):
```cpp
static void cmdNvsDel(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        Console::printf("  Without key: erases entire namespace\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    int parsed = sscanf(args, "%15s %15s", ns, key);

    if (parsed < 1) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        return;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
        return;
    }

    if (parsed == 1 || key[0] = '\0') {
        err = nvs_erase_all(nvs);  // Erases entire namespace without confirmation!
        if (err == ESP_OK) {
            nvs_commit(nvs);
            Console::printf("OK: Namespace '%s' erased\r\n", ns);
        } else {
            Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
        }
    }
    // ...
}
```

Compare to `cmdNvsClear` at lines 489-508 which requires explicit confirmation:
```cpp
static void cmdNvsClear(const char* args) {
    if (!args || strcmp(args, "YES") != 0) {
        Console::printf("WARNING: This will ERASE ALL NVS data!\r\n");
        // ... lists what will be erased
        Console::printf("\r\nTo proceed, type: NVS_CLEAR YES\r\n");
        return;
    }
    // Only erases after YES confirmation
}
```

## Recommended Fix
Require explicit confirmation for namespace-level deletion:

1. Add confirmation check when deleting entire namespace (when `parsed == 1`):
   ```cpp
   if (parsed == 1 || key[0] = '\0') {
       // Check if confirmation provided
       char confirm[10];
       if (sscanf(args, "%15s %9s", ns, confirm) == 1 || strcmp(confirm, "CONFIRM") != 0) {
           Console::printf("WARNING: This will erase all keys in namespace '%s'!\r\n", ns);
           Console::printf("To proceed, type: NVS_DEL %s CONFIRM\r\n", ns);
           return;
       }
       // Proceed with deletion
   }
   ```

2. Alternatively, add a separate command `NVS_ERASE_NS <namespace> CONFIRM` for namespace deletion

3. Add warning message before erasing namespace:
   ```cpp
   Console::printf("WARNING: Erasing namespace '%s' (%d keys)...\r\n", ns, count);
   ```

## References
- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- Similar pattern in code: `cmdNvsClear` at `SerialCmd.cpp:489`
- Best practices for destructive operations: require explicit confirmation with `CONFIRM` keyword
