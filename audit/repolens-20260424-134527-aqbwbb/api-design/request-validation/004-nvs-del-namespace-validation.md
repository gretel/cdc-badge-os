---
title: "[MEDIUM] NVS_DEL command lacks namespace/key character validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "nvs"
  - "destructive-operations"
---

## Summary

The `NVS_DEL` command in `components/serial_cmd/src/SerialCmd.cpp` accepts namespace and optional key arguments without validating that they contain only valid NVS characters. This could lead to accidental deletion of unexpected entries or difficulty in scripting due to special characters.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:609-650`

## Impact

- **Accidental data loss**: Special characters in namespace/key could match unintended entries
- **Scripting issues**: Shell metacharacters in arguments could cause issues when commands are scripted
- **Inconsistent behavior**: Different from NVS_READ which uses similar parsing but no validation

## Evidence

```cpp
// File: components/serial_cmd/src/SerialCmd.cpp:609-640
static void cmdNvsDel(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        Console::printf("  Without key: erases entire namespace\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    int parsed = sscanf(args, "%15s %15s", ns, key);  // Line 623

    if (parsed < 1) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        return;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);  // Opens namespace directly
    // ...
}
```

Issues:
1. `sscanf` with `%s` accepts any non-whitespace characters
2. No validation of valid NVS namespace/key character set
3. No check for trailing characters after the key
4. Namespace and key length limits are implicit (15 chars) but not validated with clear error

## Recommended Fix

Add character validation for namespace and key:

```cpp
static void cmdNvsDel(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        Console::printf("  Without key: erases entire namespace\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    
    // Parse namespace
    const char* p = args;
    size_t i = 0;
    while (*p && !isspace(*p) && i < NVS_NAMESPACE_MAX_LEN) {
        // NVS namespaces: alphanumeric, underscore, hyphen
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            Console::printf("ERROR: Invalid character in namespace\r\n");
            return;
        }
        ns[i++] = *p++;
    }
    ns[i] = '\0';
    
    if (i == 0) {
        Console::printf("ERROR: Namespace required\r\n");
        return;
    }
    
    // Skip whitespace to key
    while (*p && isspace(*p)) p++;
    
    if (*p) {
        // Parse key
        i = 0;
        while (*p && !isspace(*p) && i < NVS_KEY_MAX_LEN) {
            // NVS keys: alphanumeric, underscore, hyphen
            if (!isalnum(*p) && *p != '_' && *p != '-') {
                Console::printf("ERROR: Invalid character in key\r\n");
                return;
            }
            key[i++] = *p++;
        }
        key[i] = '\0';
        
        // Check for trailing characters
        if (*p && !isspace(*p)) {
            Console::printf("ERROR: Extra characters after key\r\n");
            return;
        }
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    // ... rest of function ...
}
```

## References

- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- CWE-20: Improper Input Validation
- CWE-22: Improper limitation of a path to a restricted directory (path traversal)
