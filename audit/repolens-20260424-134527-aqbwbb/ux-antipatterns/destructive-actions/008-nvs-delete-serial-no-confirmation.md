---
title: "[HIGH] NVS Delete Command Erases Namespaces Without Confirmation"
severity: HIGH
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The serial command `NVS_DEL` in `components/serial_cmd/src/SerialCmd.cpp:610-654` executes immediate deletion of NVS keys or entire namespaces without any confirmation step. When called without a key argument, it erases the entire namespace using `nvs_erase_all()`, which is a high-impact destructive operation.

**Evidence:**
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Lines: 610-654
- Function: `cmdNvsDel(const char* args)`

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

    if (parsed == 1 || key[0] == '\0') {
        err = nvs_erase_all(nvs);  // Erases entire namespace!
        if (err == ESP_OK) {
            nvs_commit(nvs);
            Console::printf("OK: Namespace '%s' erased\r\n", ns);
        } else {
            Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
        }
    } else {
        err = nvs_erase_key(nvs, key);  // Deletes single key
        if (err == ESP_OK) {
            nvs_commit(nvs);
            Console::printf("OK: Key '%s.%s' deleted\r\n", ns, key);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            Console::printf("ERROR: Key '%s' not found\r\n", key);
        } else {
            Console::printf("ERROR: Delete failed (%s)\r\n", esp_err_to_name(err));
        }
    }

    nvs_close(nvs);
}
```

The command supports two modes:
1. **Namespace erase**: `NVS_DEL <namespace>` - erases ALL keys in the namespace
2. **Single key delete**: `NVS_DEL <namespace> <key>` - deletes one key

Both execute immediately without confirmation.

## Impact
- **High Data Loss Risk**: `nvs_erase_all()` can wipe entire namespaces containing configuration, module state, or cached data
- **Module-Specific Impact**: NVS namespaces store critical data for modules like:
  - GPG (key metadata, user ID)
  - Password entries
  - TOTP accounts
  - FIDO2 credentials
  - PIN states
- **No Recovery**: NVS data is stored in flash; once erased, it cannot be recovered without external backup
- **Cascade Effects**: Erasing a module's namespace may break the module entirely, requiring re-initialization

## Recommended Fix
Add confirmation for namespace-level erasures using a two-step pattern:

1. For single-key deletion, show a confirmation prompt
2. For namespace erasure, require explicit `CONFIRM` argument with warning

```cpp
static void cmdNvsDel(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_DEL <namespace> [key] [CONFIRM]\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    char confirm[16] = {0};
    int parsed = sscanf(args, "%15s %15s %15s", ns, key, confirm);

    if (parsed < 1) {
        Console::printf("Usage: NVS_DEL <namespace> [key] [CONFIRM]\r\n");
        return;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
        return;
    }

    if (parsed == 1 || key[0] == '\0') {
        // Namespace erase - requires CONFIRM
        if (parsed == 2 && strcmp(confirm, "CONFIRM") == 0) {
            err = nvs_erase_all(nvs);
            if (err == ESP_OK) {
                nvs_commit(nvs);
                Console::printf("OK: Namespace '%s' erased\r\n", ns);
            } else {
                Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
            }
        } else {
            Console::printf("WARNING: Erase entire namespace '%s'?\r\n", ns);
            Console::printf("  This will delete ALL keys in the namespace.\r\n");
            Console::printf("  To proceed, type: NVS_DEL %s CONFIRM\r\n", ns);
        }
    } else {
        // Single key delete - requires CONFIRM if no third argument
        if (parsed == 2 || strcmp(confirm, "CONFIRM") == 0) {
            err = nvs_erase_key(nvs, key);
            if (err == ESP_OK) {
                nvs_commit(nvs);
                Console::printf("OK: Key '%s.%s' deleted\r\n", ns, key);
            } else if (err == ESP_ERR_NVS_NOT_FOUND) {
                Console::printf("ERROR: Key '%s' not found\r\n", key);
            } else {
                Console::printf("ERROR: Delete failed (%s)\r\n", esp_err_to_name(err));
            }
        } else {
            Console::printf("WARNING: Delete key '%s.%s'?\r\n", ns, key);
            Console::printf("  To proceed, type: NVS_DEL %s %s CONFIRM\r\n", ns, key);
        }
    }

    nvs_close(nvs);
}
```

## References
- Similar pattern: `TR01_WIPE` command in `serial_cmd/src/SerialCmd.cpp:1132` uses `CONFIRM` argument pattern
- Related issue: `GPG_RESET` serial command lacks confirmation (see separate finding)
