---
title: "[LOW] SET_NAME, SET_INFO, SET_INFO2 commands allow display manipulation without authentication"
severity: LOW
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `SET_NAME`, `SET_INFO`, and `SET_INFO2` serial commands allow modifying the display text without authentication. When `FEATURE_SECURE_SERIAL` is disabled (default), an attacker with serial access can change the lock screen name and info lines, potentially for social engineering or to hide device identity.

**Location**: `components/serial_cmd/src/SerialCmd.cpp:1462-1464` (registration), `components/serial_cmd/src/SerialCmd.cpp:774-804` (handlers)

## Impact
- **Social Engineering**: Attacker can set custom text to impersonate owners or create misleading information
- **Device Identity**: Lock screen name can be changed to hide original owner's name
- **Low Risk**: Primarily cosmetic, but could be useful for social engineering attacks
- **Serial Access Required**: Physical or USB CDC connection at 115200 baud

## Evidence
```cpp
// Command registration at lines 1462-1464
reg.registerCommand({"SET_NAME", "Set display name", cmdSetName, "display", false});
reg.registerCommand({"SET_INFO", "Set info line 1", cmdSetInfo, "display", false});
reg.registerCommand({"SET_INFO2", "Set info2 line 2", cmdSetInfo2, "display", false});
// The last 'false' means requiresAuth = false

// SET_NAME handler at lines 774-781
static void cmdSetName(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("name", args);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", args);
}

// SET_INFO handler at lines 788-795
static void cmdSetInfo(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info", args);
    }
    Console::printf("OK: Info set to \"%s\"\r\n", args);
}
```

Example usage:
```
SET_NAME John Doe
OK: Name set to "John Doe"
SET_INFO Find me at: john@example.com
OK: Info set to "Find me at: john@example.com"
```

## Recommended Fix
Add authentication requirement to the display commands:

```cpp
// Change registrations from:
reg.registerCommand({"SET_NAME", "Set display name", cmdSetName, "display", false});
reg.registerCommand({"SET_INFO", "Set info line 1", cmdSetInfo, "display", false});
reg.registerCommand({"SET_INFO2", "Set info2 line 2", cmdSetInfo2, "display", false});

// To:
reg.registerCommand({"SET_NAME", "Set display name", cmdSetName, "display", true});
reg.registerCommand({"SET_INFO", "Set info line 1", cmdSetInfo, "display", true});
reg.registerCommand({"SET_INFO2", "Set info2 line 2", cmdSetInfo2, "display", true});
```

## References
- `components/serial_cmd/src/SerialCmd.cpp:1462-1464` - Command registration
- `components/serial_cmd/src/SerialCmd.cpp:774-804` - Command handlers
- CWE-287: Improper Authentication
- CWE-1329: Reproducible predictable results
