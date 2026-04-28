---
title: "[MEDIUM] NVS_EDIT module exposes raw NVS values without filtering"
severity: MEDIUM
domain: mod_nvsedit
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The NVS_EDIT module provides a UI for browsing and viewing NVS (Non-Volatile Storage) data. While useful for debugging, it exposes all NVS values including potentially sensitive module settings without any filtering or redaction.

The module displays:
- All namespace keys and values
- Binary data in hex format
- String values without masking

**Location:** `components/mod_nvsedit/src/NvsEditModule.cpp`

## Impact
- **Full NVS exposure**: All stored settings visible including module-specific data
- **No filtering**: Sensitive data (PINs, keys, settings) shown in raw format
- **UI accessibility**: If module is accessible, all data is viewable
- **Binary data exposure**: Blob data displayed in hex format

## Evidence
File: `components/mod_nvsedit/src/NvsEditModule.cpp` (from existing finding #18)
```cpp
// The module displays all NVS entries without filtering
// Shows namespace keys, values, types, and raw binary data
```

Combined with serial command `NVS_LIST` and `NVS_READ` (issue #18), this provides full access to:
- Module configuration
- PIN data
- Timestamps
- Any other NVS-stored information

## Recommended Fix
1. Add namespace filtering to show only non-sensitive namespaces
2. Implement value masking for known sensitive keys (e.g., "pin", "secret", "key")
3. Require authentication before allowing NVS editing/viewing
4. Add build flag to disable NVS_EDIT module in production

Example fix:
```cpp
// Add sensitive key patterns to filter
static bool isSensitiveKey(const char* key) {
    const char* sensitive[] = {"pin", "secret", "key", "token", "password", nullptr};
    for (int i = 0; sensitive[i]; i++) {
        if (strstr(key, sensitive[i])) return true;
    }
    return false;
}

// In display function
if (isSensitiveKey(key)) {
    Console::printf("  %s (%s) [masked]\r\n", key, getNvsTypeName(type));
} else {
    printNvsValue(nvs, key, type);
}
```

## References
- NVS storage: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html
- Related to issue #18 (NVS data exposure)
- Module structure: `components/mod_nvsedit/`
