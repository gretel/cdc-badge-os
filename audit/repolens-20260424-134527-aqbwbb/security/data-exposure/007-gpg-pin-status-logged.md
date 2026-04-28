---
title: "[MEDIUM] GPG PIN verification status logged via ESP_LOG"
severity: MEDIUM
domain: mod_gpg
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The GPG module in `components/mod_gpg/src/openpgp/openpgp.cpp` uses `ESP_LOG` to log PIN verification and change success/failure status at lines 1088, 1096, 1109, 1112, 1157, and 1193. Unlike the rest of the codebase which uses `cdc_log`, this module uses `ESP_LOG` directly, which may route to different log destinations.

## Impact
- **PIN Status Exposure**: Logs reveal when PINs are verified successfully, allowing attackers to confirm valid PINs via log capture
- **Retry Information**: Log messages include retry counts, helping attackers gauge how many attempts remain
- **Block Status**: Logs indicate when PINs are blocked, leaking state information
- **Inconsistent Logging**: Uses `ESP_LOG` instead of `cdc_log`, which may bypass the intended USB CDC logging route

## Evidence
File: `components/mod_gpg/src/openpgp/openpgp.cpp`

Lines 1088, 1096 (PIN verification success):
```cpp
ESP_LOGI(TAG, "PW1 verified successfully");
ESP_LOGI(TAG, "PW3 verified successfully");
```

Lines 1109, 1112 (PIN failure):
```cpp
ESP_LOGW(TAG, "PIN blocked after too many failures");
ESP_LOGW(TAG, "PIN verification failed, %d retries left", retries);
```

Lines 1157, 1193 (PIN change success):
```cpp
ESP_LOGI(TAG, "PW1 changed successfully");
ESP_LOGI(TAG, "PW3 changed successfully");
```

## Recommended Fix
1. **Replace `ESP_LOG` with `cdc_log`** to ensure consistent logging through USB CDC
2. **Remove success messages** for PIN verification entirely (only log errors at DEBUG level)
3. **Mask retry counts** in error messages (e.g., "PIN verification failed" without showing count)

Example fix:
```cpp
// Only log on error, use cdc_log
if (!verified) {
    LOG_W("GPG", "PIN verification failed, %d retries left", retries);
}
```

## References
- OWASP: [Logging Cheat Sheet - Authentication](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)
- Project docs: `docs/README.md` line 157 - "Use `cdc_log` for logging (never `ESP_LOG` directly)"

</content>