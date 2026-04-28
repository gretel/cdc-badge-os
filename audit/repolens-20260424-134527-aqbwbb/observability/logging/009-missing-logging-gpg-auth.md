---
title: "[MEDIUM] Missing logging for GPG PIN blocked decisions"
severity: MEDIUM
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The GPG module's OpenPGP application (`components/mod_gpg/src/openpgp/openpgp.cpp`) has missing logging for authentication decision points, specifically when PW1 or PW3 PINs are blocked. The `cmd_get_status()` function returns `SW_AUTH_METHOD_BLOCKED` silently without logging.

**Key locations:**

1. **PW1 blocked check** (lines 1052-1055):
```cpp
if (pw_ref == 0x81 || pw_ref == 0x82) {
    if (pin_storage_openpgp_pw1_blocked()) {
        return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);  // No LOG_W!
    }
    retries = pin_storage_openpgp_pw1_retries();
}
```

2. **PW3 blocked check** (lines 1057-1061):
```cpp
} else if (pw_ref == 0x83) {
    if (pin_storage_openpgp_pw3_blocked()) {
        return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);  // No LOG_W!
    }
    retries = pin_storage_openpgp_pw3_retries();
}
```

3. **GET STATUS without PIN data** (lines 1050-1066): No logging when returning retry counts

## Impact
- **Security observability**: Blocked PIN state changes don't leave a log trail for audit
- **Debug difficulty**: When a PIN gets blocked, there's no log entry showing when/why
- **Inconsistent with existing logging**: The same function logs successful verification (lines 1088, 1096) but not blocked state
- **Troubleshooting**: Users can't see from logs why their PIN is blocked

## Evidence
Compare the existing logging for successful verification:

```cpp
// Line 1088: PW1 verified - logged
if (verified) {
    pw1_verified = true;
    strncpy(s_session_pin, pin_str, OPENPGP_PIN_MAX_LEN);
    ESP_LOGI(TAG, "PW1 verified successfully");
}

// Line 1096: PW3 verified - logged
if (verified) {
    pw3_verified = true;
    ESP_LOGI(TAG, "PW3 verified successfully");
}

// Line 1109: PIN blocked after failures - logged
if (retries == 0) {
    ESP_LOGW(TAG, "PIN blocked after too many failures");
    return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
}
```

But the **initial blocked check** (lines 1054, 1059) has no logging:
```cpp
// Line 1054: PW1 already blocked - NOT logged
if (pin_storage_openpgp_pw1_blocked()) {
    return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
}

// Line 1059: PW3 already blocked - NOT logged
if (pin_storage_openpgp_pw3_blocked()) {
    return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
}
```

Note: The code already uses `ESP_LOG` instead of `LOG_I`/`LOG_W` (see finding #1), so this fix can be combined with that migration.

## Recommended Fix
Add logging to blocked PIN checks in `cmd_get_status()`:

1. **In PW1 blocked check** (around line 1054):
```cpp
if (pin_storage_openpgp_pw1_blocked()) {
    LOG_W(TAG, "PW1 blocked, need PW3 to reset");
    return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
}
```

2. **In PW3 blocked check** (around line 1059):
```cpp
if (pin_storage_openpgp_pw3_blocked()) {
    LOG_W(TAG, "PW3 blocked, all admin functions locked");
    return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
}
```

3. **GET STATUS without PIN data** (around line 1050):
```cpp
if (apdu->lc == 0) {
    uint8_t retries;
    if (pw_ref == 0x81 || pw_ref == 0x82) {
        if (pin_storage_openpgp_pw1_blocked()) {
            LOG_W(TAG, "GET STATUS: PW1 blocked");
            return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
        }
        retries = pin_storage_openpgp_pw1_retries();
        LOG_D(TAG, "GET STATUS: PW1 retries=%d", retries);
    }
    ...
}
```

## References
- `components/mod_gpg/src/openpgp/openpgp.cpp` - OpenPGP application implementation
- `components/mod_gpg/src/pin_storage.cpp` - PIN storage and verification
- Finding #1: ESP_LOG migration needed
