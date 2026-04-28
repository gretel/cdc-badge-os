---
title: "[HIGH] NVS data fully exposed via serial commands NVS_LIST/NVS_READ and UI module"
severity: HIGH
domain: serial-cmd, mod_nvsedit
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `NVS_LIST` and `NVS_READ` serial commands in `components/serial_cmd/src/SerialCmd.cpp` and the NVS Editor module in `components/mod_nvsedit/src/NvsEditModule.cpp` expose all NVS (Non-Volatile Storage) data without filtering, including potentially sensitive data like PIN hashes, module configuration, and metadata.

## Impact
- **Sensitive Data Exposure**: NVS storage contains critical data including:
  - Module configuration and state
  - Attestation key public hashes (`attest` namespace)
  - FIDO2 credential metadata (`fido2` namespace)
  - GPG cardholder data (`openpgp` namespace)
  - TOTP secrets (`mod_totp` namespace)
  - Password entries (`mod_password` namespace)
  - System PINs and retry counters
- **No Access Control**: Commands are registered without authentication requirements
- **Complete Data Enumeration**: `NVS_LIST` exposes all namespace/key names revealing internal data structure
- **Full Value Exposure**: `NVS_READ` exposes complete values including strings and blobs (hex dump)

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`

Command registration (lines 1450-1451):
```cpp
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});
reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", false});
```

NVS_LIST implementation (lines 522-568) - lists all namespaces and keys:
```cpp
Console::printf("\r\n[%s]\r\n", info.namespace_name);
Console::printf("  %s (%s)\r\n", info.key, getNvsTypeName(info.type));
```

NVS_READ implementation (lines 574-604) - reads full values:
```cpp
Console::printf("%s.%s = ", ns, key);
printNvsValue(nvs, key, keyType);  // Exposes full string and blob values
```

File: `components/mod_nvsedit/src/NvsEditModule.cpp`

NVS Editor view (lines 495-506) - exposes values in UI:
```cpp
static void showValueView(const char* ns, const char* key, nvs_type_t type) {
    formatValue(ns, key, type, s_valueBuffer, sizeof(s_valueBuffer));
    s_valueView->init(s_valueTitle, s_valueBuffer);
    ViewStack::instance().push(s_valueView);
}
```

## Recommended Fix
1. **Filter sensitive namespaces/keys**: Create a whitelist of safe namespaces to expose
2. **Add authentication requirement**: Require PIN verification before NVS commands
3. **Mask sensitive values**: For strings, show only first/last characters; for blobs, show size only
4. **Add verbose flag**: Require explicit `--verbose` or `VERBOSE` flag to show full values
5. **Create separate commands**: `NVS_LIST_SAFE` for overview, `NVS_LIST_VERBOSE` for full details

Example fix for NVS_READ:
```cpp
// Check if key is in sensitive list
static bool isSensitiveKey(const char* ns, const char* key) {
    const char* sensitive[] = {"pin", "hash", "secret", "key", "blob"};
    for (int i = 0; i < 5; i++) {
        if (strstr(key, sensitive[i]) != nullptr) return true;
    }
    return false;
}

// In printNvsValue, mask sensitive data
if (isSensitiveKey(ns, key)) {
    if (type == NVS_TYPE_STR) {
        Console::printf("\"****\" (use NVS_READ_VERBOSE for full value)\r\n");
    } else if (type == NVS_TYPE_BLOB) {
        Console::printf("(blob, %zu bytes, use NVS_READ_VERBOSE for hex)\r\n", len);
    }
}
```

## References
- OWASP: [Secrets Management](https://owasp.org/www-project-cheat-sheets/cheatsheets/Secrets_Management_Cheat_Sheet.html)
- Related finding: #001 (Password exposed via PASSWORD_GET)
- Related finding: #002 (TOTP secret exposed via TOTP_ADD)
- NVS data structure: `components/cdc_core/src/AttestationKeyService.cpp`, `components/mod_fido2/src/fido2_storage.cpp`

</content>