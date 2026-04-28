---
title: "[MEDIUM] Insufficient Input Length Validation in Serial Command Parameters"
severity: MEDIUM
domain: api-security
lens: session-zap-api
labels:
  - audit:toolgate/session-zap-api
---

## Summary

The serial command interface in `components/serial_cmd/src/SerialCmd.cpp` uses `sscanf` with fixed-width format specifiers for parsing command arguments, but the buffer sizes for namespace and key parsing are potentially too small for some use cases, and there's no validation of total argument length before parsing.

**Affected File:** `components/serial_cmd/src/SerialCmd.cpp`
**Lines:** 582, 619 (NVS_READ, NVS_DEL commands)

## Impact

While the current implementation uses bounded `sscanf` format specifiers (`%15s`), this creates a potential usability issue where valid NVS namespaces/keys longer than 15 characters would be silently truncated, potentially leading to:
- Confusion when reading/writing NVS data
- Accidental overwrites if a longer key is intended
- Inconsistent behavior across different commands

## Evidence

```cpp
// Line 582 - cmdNvsRead
if (sscanf(args, "%15s %15s", ns, key) != 2) {
    Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
    return;
}

// Line 619 - cmdNvsDel
int parsed = sscanf(args, "%15s %15s", ns, key);
```

Buffer declarations:
```cpp
// Line 580-581
char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};  // 16 bytes
char key[NVS_KEY_MAX_LEN + 1] = {0};       // 16 bytes
```

Constants defined at line 37-38:
```cpp
static constexpr size_t NVS_KEY_MAX_LEN = 15;
static constexpr size_t NVS_NAMESPACE_MAX_LEN = 15;
```

## Recommended Fix

1. **Increase buffer sizes** to match ESP-NVS limits (NVS_KEY_MAX is typically 15, but namespaces can be longer)
2. **Add explicit length validation** before parsing to provide better error messages
3. **Consider using safer parsing** with explicit length checks

Example fix:
```cpp
static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    // Count words and check lengths before parsing
    const char* p = args;
    while (*p && isspace(*p)) p++;
    size_t nsLen = 0;
    while (*p && !isspace(*p) && nsLen < NVS_NAMESPACE_MAX_LEN) {
        nsLen++; p++;
    }
    if (nsLen > NVS_NAMESPACE_MAX_LEN) {
        Console::printf("ERROR: Namespace too long (max %d chars)\r\n", NVS_NAMESPACE_MAX_LEN);
        return;
    }
    // ... continue with key length check
}
```

## References

- ESP-NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- CWE-134: Use of externally-controlled format string
- OWASP API Security: API3:2023 Broken Object Property Level Authorization (input validation)
