---
title: "[MEDIUM] NVS read/list commands expose sensitive data without authentication"
severity: MEDIUM
domain: API Security
lens: session-zap-api
labels:
  - "audit:toolgate/session-zap-api"
---

## Summary
The `NVS_LIST` and `NVS_READ` serial commands allow reading Non-Volatile Storage (NVS) entries without authentication. Located in `components/serial_cmd/src/SerialCmd.cpp:522-600`, these commands can expose sensitive data including WiFi credentials, module settings, and potentially PIN hashes.

## Impact
- **Information Disclosure**: NVS may contain sensitive data like:
  - WiFi credentials (SSID, password)
  - Module configuration (TOTP secrets, password vault entries)
  - PIN hashes and lockout state
  - Timestamps and user preferences
- **Reconnaissance**: Attacker can enumerate all namespaces to understand the device configuration
- **PII Exposure**: User data stored in NVS can be read without authentication

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp`
Lines: 522-568 (NVS_LIST), 574-604 (NVS_READ)

```cpp
// NVS_LIST - lists all NVS entries
static void cmdNvsList(const char* args) {
    const char* nsFilter = (args && *args) ? args : nullptr;
    // ... reads and prints all namespace keys and types
    Console::printf("  %s (%s)\r\n", info.key, getNvsTypeName(info.type));
}

// NVS_READ - reads a specific NVS key value
static void cmdNvsRead(const char* args) {
    // ... opens namespace and prints value
    printNvsValue(nvs, key, keyType);
}
```

Registration (lines 1450-1451):
```cpp
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});
reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", false});
```

Both commands are registered with `requiresAuth = false`.

## Recommended Fix
1. **Add authentication requirement** to both commands:
   ```cpp
   reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", true});
   reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", true});
   ```

2. **Consider namespace filtering**: If some namespaces should be public, add a whitelist check in the command handler.

3. **Add sensitive key masking**: For NVS keys that might contain secrets, consider masking values or requiring elevated privileges.

## References
- OWASP API Security Top 10: **API4:2023 Unrestricted Resource Consumption** (enumeration)
- OWASP API Security Top 10: **API8:2023 Security Misconfiguration**
- CWE-200: Exposure of Sensitive Information to an Unauthorized Actor
- OWASP Cheat Sheet: Information Disclosure
