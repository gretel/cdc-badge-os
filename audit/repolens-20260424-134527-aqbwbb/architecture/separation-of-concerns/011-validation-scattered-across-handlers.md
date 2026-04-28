---
title: "[LOW] Validation logic scattered across handler classes"
severity: LOW
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
Validation logic (IP address, PIN, text length, etc.) is scattered across multiple handler classes instead of being centralized in a shared validation utility. This leads to code duplication and makes it hard to ensure consistent validation rules across the application.

**Location**: 
- `components/cdc_os_ui/src/WifiHandlers.cpp:38-65` - IP validation methods
- `components/cdc_os_ui/src/SettingsHandlers.cpp` - PIN validation (inferred)
- `components/cdc_os_ui/src/WifiMenuUi.cpp:575-605` - Validation called from UI handlers

## Impact
- **Code duplication**: Similar validation logic may be duplicated in multiple places
- **Inconsistent rules**: Different handlers may validate the same data differently
- **Hard to maintain**: Changing validation rules requires updating multiple files
- **Limited reuse**: Validation logic is tied to specific handler classes

## Evidence

### Example 1: IP validation in WifiHandlers
```cpp
// components/cdc_os_ui/src/WifiHandlers.cpp:38-65
namespace cdc::ui {

bool WifiHandlers::isValidIpOctet(int val) {
    return val >= 0 && val <= 255;
}

bool WifiHandlers::isValidIpAddress(const char* ip) {
    if (!ip || !ip[0]) return false;
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
    return isValidIpOctet(a) && isValidIpOctet(b) && isValidIpOctet(c) && isValidIpOctet(d);
}

uint32_t WifiHandlers::parseIpAddress(const char* ip) const {
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
    if (!isValidIpOctet(a) || !isValidIpOctet(b) || !isValidIpOctet(c) || !isValidIpOctet(d)) return 0;
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

// Used in handlers:
void WifiHandlers::saveConfig() {
    nvs_set_u32(nvs, "ip", parseIpAddress(wizard_.staticIp));  // Validation mixed with persistence
    nvs_set_u32(nvs, "gw", parseIpAddress(wizard_.gateway));
    nvs_set_u32(nvs, "nm", parseIpAddress(wizard_.netmask));
}

} // namespace cdc::ui
```

### Example 2: Validation called from UI handlers
```cpp
// components/cdc_os_ui/src/WifiMenuUi.cpp:575-605
static void onWifiStaticIpEntered(const char* ip) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(ip)) {  // Validation coupled to handler
        showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
        wifiShowIpInputField(tr(StringId::WIFI_IP), wizard.staticIp, 
                             sizeof(wizard.staticIp), onWifiStaticIpEntered);
        return;
    }
    strncpy(wizard.staticIp, ip, sizeof(wizard.staticIp) - 1);
    // ... continue wizard
}

static void onWifiGatewayEntered(const char* gateway) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(gateway)) {  // Same validation, different handler
        showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
        // ...
    }
}

static void onWifiNetmaskEntered(const char* netmask) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(netmask)) {  // Same validation, different handler
        showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
        // ...
    }
}
```

### Example 3: Validation logic not exposed for reuse
```cpp
// In WifiHandlers.h:
class WifiHandlers {
public:
    static bool isValidIpAddress(const char* ip);  // Static, but still tied to WifiHandlers class
    
private:
    static bool isValidIpOctet(int val);  // Private, not reusable
    uint32_t parseIpAddress(const char* ip) const;  // Instance method, not pure utility
};

// To reuse validation, you must:
// 1. Include WifiHandlers.h
// 2. Call WifiHandlers::isValidIpAddress()
// Even if you just need IP validation for something unrelated to WiFi!
```

### Example 4: Similar validation likely duplicated elsewhere
Based on codebase patterns, similar validation logic is likely duplicated in:
- SettingsHandlers for badge text validation
- Module-specific handlers for their configuration
- Serial command handlers for parameter validation

## Recommended Fix

### 1. Create a centralized validation utility

```cpp
// components/cdc_core/include/cdc_core/Validation.h
namespace cdc::core {

class Validation {
public:
    // IP address validation
    static bool isValidIpOctet(int val);
    static bool isValidIpAddress(const char* ip);
    static uint32_t parseIpAddress(const char* ip);
    static bool isValidMacAddress(const char* mac);
    
    // Text validation
    static bool isValidText(const char* text, size_t maxLen);
    static bool isValidPin(const char* pin, size_t minLen, size_t maxLen);
    
    // Number validation
    static bool isInRange(int val, int min, int max);
    
    // Date/time validation
    static bool isValidDate(int day, int month, int year);
    static bool isValidTime(int hour, int minute);
};

} // namespace cdc::core
```

### 2. Refactor handlers to use centralized validation

```cpp
// Before:
if (!WifiHandlers::isValidIpAddress(ip)) {
    // ...
}

// After:
if (!core::Validation::isValidIpAddress(ip)) {
    // ...
}

// Or even better, use a validation result pattern:
auto result = core::Validation::validateIpAddress(ip);
if (!result.isValid()) {
    showToastError(result.getMessage(), TOAST_DURATION_MEDIUM_MS);
}
```

### 3. Create domain-specific validators (optional)

```cpp
// components/cdc_core/include/cdc_core/Validators.h
namespace cdc::core {

class WifiValidators {
public:
    static bool isValidSsid(const char* ssid);
    static bool isValidPassword(const char* password, hal::WifiSecurity security);
};

class BadgeValidators {
public:
    static bool isValidBadgeName(const char* name);
    static bool isValidBadgeInfo(const char* info);
};

} // namespace cdc::core
```

### 4. Benefits of centralized validation
- **Consistency**: Same rules applied everywhere
- **Reusability**: Validation logic can be used by any component
- **Testability**: Validation can be tested independently
- **Maintainability**: Change rules in one place

## References
- [DRY principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [Utility class pattern](https://en.wikipedia.org/wiki/Utility_class)
- Related issue #008 covers validation in WifiHandlers (similar scope)
