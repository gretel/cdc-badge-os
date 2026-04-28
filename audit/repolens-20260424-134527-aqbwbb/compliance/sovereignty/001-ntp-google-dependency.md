---
title: "[MEDIUM] NTP time synchronization uses US-based Google server as fallback"
severity: MEDIUM
domain: digital-sovereignty
lens: time-dependency
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The Wi-Fi time synchronization in `components/cdc_os_ui/src/WifiHandlers.cpp:230-232` configures two NTP servers:

```cpp
esp_sntp_setservername(0, "pool.ntp.org");
esp_sntp_setservername(1, "time.google.com");
```

While `pool.ntp.org` is a decentralized, global project, the fallback server `time.google.com` is hosted by Google (US-based), creating a minor geopolitical dependency for time synchronization.

## Impact

- **Low-moderate risk**: Time synchronization is a basic utility function
- If `pool.ntp.org` becomes inaccessible from EU regions, the device falls back to a US-controlled server
- For a security-focused hardware key, time accuracy affects TOTP generation and certificate validation
- Minimal CLOUD Act exposure (time data is not user-sensitive)

## Evidence

**File**: `components/cdc_os_ui/src/WifiHandlers.cpp`  
**Lines**: 230-232

```cpp
esp_sntp_setservername(0, "pool.ntp.org");
esp_sntp_setservername(1, "time.google.com");
```

## Recommended Fix

Replace the Google fallback with a European or more neutral NTP server:

```cpp
esp_sntp_setservername(0, "pool.ntp.org");
esp_sntp_setservername(1, "europe.pool.ntp.org");  // EU-focused NTP pool
```

Alternative EU-friendly NTP options:
- `de.pool.ntp.org` (Germany)
- `fr.pool.ntp.org` (France)
- `at.pool.ntp.org` (Austria)
- `ch.pool.ntp.org` (Switzerland)

For maximum sovereignty, use only NTP pool servers with regional targeting.

## References

- [NTP Pool Project - Regional Pools](https://www.pool.ntp.org/en/regions.html)
- [European NTP Pool](https://www.pool.ntp.org/en/regions/europe.html)

---
