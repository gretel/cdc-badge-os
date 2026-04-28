---
title: "[LOW] 5-minute authentication timeout may be too long for high-security use"
severity: LOW
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The serial authentication timeout is set to 5 minutes (`AUTH_TIMEOUT_MS = 5 * 60 * 1000`) in `components/serial_cmd/include/serial_cmd/SerialCmd.h:26`. This means once authenticated, the session remains valid for 5 minutes without activity.

**Location**: `components/serial_cmd/include/serial_cmd/SerialCmd.h:26`

## Impact
- **Extended Access Window**: After PIN entry, attacker has 5 minutes to execute commands
- **Idle Sessions**: Command session remains authenticated if user walks away
- **Physical Security**: Depends on physical access, but timeout could be shorter

## Evidence
```cpp
// components/serial_cmd/include/serial_cmd/SerialCmd.h:26
static constexpr uint32_t AUTH_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
```

Timeout check in `SerialCmd.cpp:1352-1358`:
```cpp
bool SerialCmd::isAuthenticated() {
#if FEATURE_SECURE_SERIAL
    if (!s_authenticated) return false;

    uint64_t now = esp_timer_get_time();
    if ((now - s_authTimestamp) > (AUTH_TIMEOUT_MS * 1000ULL)) {
        s_authenticated = false;
        LOG_I(TAG, "Session timed out");
        return false;
    }
    return true;
#else
    return true;
#endif
}
```

## Recommended Fix
Reduce timeout to 1-2 minutes for better security:

```cpp
// components/serial_cmd/include/serial_cmd/SerialCmd.h:26
static constexpr uint32_t AUTH_TIMEOUT_MS = 2 * 60 * 1000;  // 2 minutes
```

Consider adding:
1. **Configurable timeout**: Allow build-time configuration
2. **Auto-logout on display sleep**: Invalidate serial session when device sleeps
3. **Manual logout command**: Already exists (`LOGOUT`), but could be more prominent

## References
- `components/serial_cmd/include/serial_cmd/SerialCmd.h:26` - Timeout definition
- `components/serial_cmd/src/SerialCmd.cpp:1352-1358` - Timeout check
- Security best practices for session timeouts
