---
title: "[MEDIUM] NVS_READ command lacks proper argument parsing validation"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-interface
labels:
  - "request-validation"
  - "serial-commands"
  - "nvs"
---

## Summary

The `NVS_READ` command in `components/serial_cmd/src/SerialCmd.cpp` parses namespace and key arguments using `sscanf` with fixed-width format specifiers, but does not validate that the parsed strings are properly null-terminated or check for buffer overflow conditions when arguments contain unexpected whitespace or special characters.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:557-570`

## Impact

- **Data exposure risk**: Malformed arguments could potentially read from unintended namespaces
- **Buffer overflow potential**: While bounded by `sscanf`, the lack of explicit validation makes the code harder to audit
- **Inconsistent validation**: Other commands use more robust parsing patterns

## Evidence

```cpp
// File: components/serial_cmd/src/SerialCmd.cpp:557-570
static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    if (sscanf(args, "%15s %15s", ns, key) != 2) {  // Line 568
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }
    // ...
}
```

The `sscanf` with `%s` format specifier:
1. Stops at whitespace, so `"ns1 key with spaces"` would parse as `ns="ns1"`, `key="key"` (missing "with spaces")
2. Does not validate that namespace/key characters are valid for NVS
3. Does not check for leading/trailing whitespace in individual fields

## Recommended Fix

Replace `sscanf` with explicit token parsing that:
1. Validates each field independently
2. Checks for valid character ranges (alphanumeric, underscore, hyphen)
3. Reports specific parsing errors for each field

```cpp
static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    
    // Parse namespace
    const char* p = args;
    while (*p && isspace(*p)) p++;  // Skip leading whitespace
    size_t i = 0;
    while (*p && !isspace(*p) && i < NVS_NAMESPACE_MAX_LEN) {
        // Validate character (alphanumeric, underscore, hyphen)
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            Console::printf("ERROR: Invalid character in namespace\r\n");
            return;
        }
        ns[i++] = *p++;
    }
    if (i == 0) {
        Console::printf("ERROR: Namespace required\r\n");
        return;
    }
    ns[i] = '\0';
    
    // Skip whitespace to key
    while (*p && isspace(*p)) p++;
    if (!*p) {
        Console::printf("ERROR: Key required\r\n");
        return;
    }
    
    i = 0;
    while (*p && !isspace(*p) && i < NVS_KEY_MAX_LEN) {
        if (!isalnum(*p) && *p != '_' && *p != '-') {
            Console::printf("ERROR: Invalid character in key\r\n");
            return;
        }
        key[i++] = *p++;
    }
    key[i] = '\0';
    
    // ... rest of function
}
```

## References

- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- CWE-20: Improper Input Validation
- CWE-193: Off-by-one Error
