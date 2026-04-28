---
title: "[MEDIUM] WifiHandlers.cpp combines configuration persistence, connection management, and NTP sync"
severity: MEDIUM
domain: cdc_os_ui
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/cdc_os_ui/src/WifiHandlers.cpp` (273 lines) handles multiple distinct responsibilities:
1. **Configuration persistence** - `loadConfig()`, `saveConfig()` (NVS read/write)
2. **Connection management** - `connect()`, `disconnect()`, `isConnected()`
3. **NTP synchronization** - `syncNtp()` with SNTP setup and wait logic
4. **IP parsing** - `parseIpAddress()`, `isValidIpOctet()`, `isValidIpAddress()`
5. **Wizard state** - `WifiWizard` struct manipulation

## Impact
- **Coupling**: NVS changes, connection logic, and NTP sync all in same file
- **Testing difficulty**: Cannot test IP parsing without full WiFi setup
- **Code reuse**: IP parsing logic could be shared with other network modules

## Evidence
File: `components/cdc_os_ui/src/WifiHandlers.cpp`
- Lines 18-19: WiFi wizard reset
- Lines 28-33: Singleton instance
- Lines 35-66: IP validation and parsing
- Lines 68-99: Config persistence (NVS)
- Lines 101-120: Config save
- Lines 122-131: Connection check
- Lines 133-173: WiFi connect logic
- Lines 175-194: WiFi disconnect
- Lines 196-273: NTP sync with connection management

Key function showing mixed concerns:
```cpp
bool WifiHandlers::syncNtp() {
    auto* wifi = hal::getWifiControllerInstance();
    
    // Connection management
    if (!wifi->isConnected()) {
        // Connect using saved config
        if (!config_.valid) { ... }
        wifi->enable(hal::WifiMode::STA);
        wifi->connect(...);
        weConnected = true;
    }
    
    // NTP sync
    static bool sntpInited = false;
    if (!sntpInited) {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
    } else {
        esp_sntp_restart();
    }
    
    // Wait for sync
    while (...) { ... }
    
    // Disconnect if we connected
    if (weConnected) {
        wifi->disconnect();
        wifi->disable();
    }
}
```

## Recommended Fix
Split into focused modules:
1. **WifiConfig** - Configuration persistence in `components/cdc_os_ui/src/WifiConfig.cpp`
2. **WifiConnection** - Connection management in `components/cdc_os_ui/src/WifiConnection.cpp`
3. **WifiNtp** - NTP synchronization in `components/cdc_os_ui/src/WifiNtp.cpp`
4. **IpHelpers** - IP parsing utilities in `components/cdc_hal/src/IpHelpers.cpp`

Each module should:
- Have its own header file
- Accept dependencies via constructor
- Be testable in isolation

Example split structure:
```
components/cdc_os_ui/src/
  WifiHandlers.cpp  // Main orchestration, delegates to sub-modules
  WifiConfig.cpp    // NVS persistence
  WifiConnection.cpp // Connect/disconnect
  WifiNtp.cpp       // NTP sync
components/cdc_hal/src/
  IpHelpers.cpp     // IP parsing utilities
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Separation of Concerns: https://en.wikipedia.org/wiki/Separation_of_concerns
