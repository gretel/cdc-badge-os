---
title: "[MEDIUM] Missing overflow check for NVS key/namespace length in serial commands"
severity: MEDIUM
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `cmdNvsRead` and `cmdNvsDel` functions in `components/serial_cmd/src/SerialCmd.cpp` use `sscanf()` to parse namespace and key names with a fixed width of 15 characters. However, if a user provides longer input, `sscanf()` will read up to 15 characters but the remaining input is silently discarded. More importantly, the parsing doesn't validate that the input contains only valid characters for NVS keys and namespaces.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:570-580` (cmdNvsRead)
**Location:** `components/serial_cmd/src/SerialCmd.cpp:610-625` (cmdNvsDel)

## Impact
- **Silent truncation**: If a user provides a namespace or key name longer than 15 characters, it's silently truncated without error, potentially leading to reading/writing the wrong key
- **Invalid characters**: NVS keys have restrictions on allowed characters (alphanumeric, underscore, etc.), but the commands accept any printable characters
- **Confusion**: Users might think they're accessing a specific key but actually accessing a truncated version

## Evidence
```cpp
// components/serial_cmd/src/SerialCmd.cpp:570-580
static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};  // 15 chars
    char key[NVS_KEY_MAX_LEN + 1] = {0};       // 15 chars
    
    // sscanf with width specifier silently truncates longer input
    if (sscanf(args, "%15s %15s", ns, key) != 2) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }
    // ... rest of function
}
```

```cpp
// components/serial_cmd/src/SerialCmd.cpp:610-625
static void cmdNvsDel(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    
    // Same issue - silent truncation
    int parsed = sscanf(args, "%15s %15s", ns, key);
    // ... rest of function
}
```

The constants are defined at line 36-37:
```cpp
static constexpr size_t NVS_KEY_MAX_LEN = 15;
static constexpr size_t NVS_NAMESPACE_MAX_LEN = 15;
```

## Recommended Fix
Add validation to check that the input doesn't exceed the maximum length and contains only valid characters:

```cpp
// Helper function to validate NVS key/namespace format
static bool isValidNvsName(const char* name, size_t maxLen) {
    if (!name || !*name) return false;
    
    size_t len = strlen(name);
    if (len > maxLen) return false;  // Explicit length check
    
    // NVS keys should be alphanumeric, underscore, hyphen
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    
    // Parse with width specifier
    int count = sscanf(args, "%15s %15s", ns, key);
    if (count != 2) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }
    
    // Validate names
    if (!isValidNvsName(ns, NVS_NAMESPACE_MAX_LEN)) {
        Console::printf("ERROR: Invalid namespace format or too long (max %d chars)\r\n", 
                       NVS_NAMESPACE_MAX_LEN);
        return;
    }
    if (!isValidNvsName(key, NVS_KEY_MAX_LEN)) {
        Console::printf("ERROR: Invalid key format or too long (max %d chars)\r\n", 
                       NVS_KEY_MAX_LEN);
        return;
    }
    
    // ... rest of function
}
```

For `cmdNvsDel`, add similar validation after the parsing.

## References
- ESP-IDF NVS documentation on key naming
- CWE-20: Improper Input Validation
- CWE-120: Buffer copy without checking size of input
