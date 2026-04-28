---
title: "[LOW] Switch statements with incomplete default handling"
severity: LOW
domain: maintainability
lens: tech-debt/robustness
labels:
  - robustness
  - switch-statements
---

## Summary
Several switch statements in the codebase have minimal or no handling in their `default` cases, which could silently ignore unexpected values.

## Impact
- **Silent failures**: Unexpected enum values may not be caught
- **Debugging difficulty**: Hard to trace why certain code paths execute
- **Future bugs**: Adding new enum values may break existing logic silently

## Evidence

### 1. WifiController.cpp - Default does nothing:
File: `components/cdc_hal/src/WifiController.cpp`, Lines 305-310
```cpp
switch (mode) {
    case WifiMode::STA:    wifiMode = WIFI_MODE_STA; break;
    case WifiMode::AP:     wifiMode = WIFI_MODE_AP; break;
    case WifiMode::STA_AP: wifiMode = WIFI_MODE_APSTA; break;
    default: break;  // Silent failure!
}
```

### 2. ServiceRegistry.cpp - Returns "unknown":
File: `components/cdc_core/src/ServiceRegistry.cpp`, Lines 15-20
```cpp
switch (type) {
    case ServiceType::KEYBOARD:     return "keyboard";
    case ServiceType::CLIPBOARD:    return "clipboard";
    case ServiceType::NOTIFICATION: return "notification";
    default:                        return "unknown";  // Could log warning
}
```

### 3. TCA9535Keypad.cpp - No default case shown:
File: `components/cdc_hal/src/TCA9535Keypad.cpp`, Lines 45-66
```cpp
switch (raw & 0x0FFF) {
    case 0b111111111110: return Key::KEY_0;
    case 0b111111111101: return Key::KEY_1;
    // ... more cases
    // No default case visible
}
```

## Recommended Fix

### For WifiController.cpp - Add logging:
```cpp
switch (mode) {
    case WifiMode::STA:    wifiMode = WIFI_MODE_STA; break;
    case WifiMode::AP:     wifiMode = WIFI_MODE_AP; break;
    case WifiMode::STA_AP: wifiMode = WIFI_MODE_APSTA; break;
    default:
        LOG_W(TAG, "Invalid WifiMode: %d", static_cast<int>(mode));
        wifiMode = WIFI_MODE_NULL;
        break;
}
```

### For ServiceRegistry.cpp - Add debug logging:
```cpp
switch (type) {
    case ServiceType::KEYBOARD:     return "keyboard";
    case ServiceType::CLIPBOARD:    return "clipboard";
    case ServiceType::NOTIFICATION: return "notification";
    default:
        LOG_D(TAG, "Unknown ServiceType: %d", static_cast<int>(type));
        return "unknown";
}
```

### For TCA9535Keypad.cpp - Add default case:
```cpp
switch (raw & 0x0FFF) {
    case 0b111111111110: return Key::KEY_0;
    // ... existing cases ...
    default:
        LOG_D(TAG, "Unknown key pattern: 0x%04X", raw & 0x0FFF);
        return Key::UNKNOWN;
}
```

### General pattern to follow:
```cpp
switch (value) {
    case A: /* ... */ break;
    case B: /* ... */ break;
    default:
        LOG_W(TAG, "Unexpected value: %d", static_cast<int>(value));
        return defaultValue;
}
```

## References
- C++ switch statement best practices
- ESP32 logging guidelines in CLAUDE.md
