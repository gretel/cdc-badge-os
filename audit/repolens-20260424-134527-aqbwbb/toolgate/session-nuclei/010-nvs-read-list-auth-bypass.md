---
title: "[MEDIUM] NVS_READ and NVS_LIST commands expose NVS data without authentication"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `NVS_READ` and `NVS_LIST` serial commands allow reading Non-Volatile Storage (NVS) data without authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker with serial access can read all NVS namespaces and keys, potentially exposing module settings, WiFi credentials, and other sensitive data.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1450-1451` (registration), `components/serial_cmd/src/SerialCmd.cpp:574-604` (NVS_READ handler), `components/serial_cmd/src/SerialCmd.cpp:518-568` (NVS_LIST handler)

## Impact
- **Information Disclosure**: NVS can contain:
  - Module configuration settings
  - WiFi credentials (SSID, password)
  - Timezone settings
  - Module-specific data (TOTP, passwords, FIDO2 settings)
  - PIN-related metadata
- **Reconnaissance**: `NVS_LIST` reveals all available namespaces and keys for targeted attacks
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at lines 1450-1451
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});
reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", false});
// The last 'false' means requiresAuth = false

// NVS_READ handler at lines 574-604
static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    if (sscanf(args, "%15s %15s", ns, key) != 2) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
        return;
    }

    nvs_type_t keyType = findNvsKeyType(ns, key);
    if (keyType == NVS_TYPE_ANY) {
        Console::printf("ERROR: Key '%s' not found in namespace '%s'\r\n", key, ns);
        nvs_close(nvs);
        return;
    }

    Console::printf("%s.%s = ", ns, key);
    printNvsValue(nvs, key, keyType);  // Prints the value
    nvs_close(nvs);
}
```

Common NVS namespaces that may contain sensitive data:
- `nvs` - Default namespace with module settings
- `wifi` - WiFi credentials
- `time` - Timezone and time settings
- `tr01_meta` - TROPIC01 cache metadata
- Module-specific namespaces (e.g., `totp`, `password`, `fido2`)

## Recommended Fix
Add authentication requirement to both NVS commands:

```cpp
// Change registration from:
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});
reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", false});

// To:
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", true});
reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", true});
```

Alternatively, add explicit auth checks in the handlers:
```cpp
static void cmdNvsRead(const char* args) {
#if FEATURE_SECURE_SERIAL
    if (!SerialCmd::isAuthenticated()) {
        Console::printf("ERROR: Authentication required. Use AUTH <pin> first.\r\n");
        return;
    }
#endif
    // ... rest of function
}
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1450-1451` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:574-604` - NVS_READ handler
- `components/serial_cmd/src/SerialCmd.cpp:518-568` - NVS_LIST handler
- CWE-200: Exposure of sensitive information to an unauthorized actor
- CWE-497: Exposure of system information to an outside actor
