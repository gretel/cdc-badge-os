---
title: "[MEDIUM] ESP_LOG bypasses cdc_log routing in multiple modules"
severity: MEDIUM
domain: logging
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
Multiple modules use `ESP_LOG` directly instead of the project's `cdc_log` library. This bypasses the USB CDC routing and may send logs to different destinations (e.g., RTT, SWO, or default ESP-IDF log). Affected files include:
- `components/mod_gpg/src/openpgp/openpgp.cpp` (225 matches)
- `components/CalEPD/` (various display drivers)

## Impact
- **Inconsistent Log Routing**: ESP_LOG may go to different destinations than cdc_log, potentially exposing logs to unintended channels
- **Harder to Audit**: Logs may not appear in the expected serial output, making it harder to track what's being logged
- **Potential Info Leakage**: Some ESP_LOG destinations (like RTT) may be accessible without PIN authentication
- **Project Violation**: Violates the project's logging convention documented in `docs/README.md:157`

## Evidence
File: `components/mod_gpg/src/openpgp/openpgp.cpp` (examples)

Line 601:
```cpp
ESP_LOGI(TAG, "AID initialized: Manufacturer=0x%02X%02X Serial=%02X%02X%02X%02X", ...);
```

Line 1088:
```cpp
ESP_LOGI(TAG, "PW1 verified successfully");
```

Line 1630:
```cpp
ESP_LOGD(TAG, "APDU: CLA=%02X INS=%02X P1=%02X P2=%02X Lc=%d", ...);
```

File: `components/CalEPD/epdspi.cpp:76`
```cpp
ESP_LOGI("EpdSPI", "init() Debug enabled. SPI master at frequency:%d  MOSI:%d CLK:%d CS:%d DC:%d RST:%d BUSY:%d DMA_CH: %d\n", ...);
```

Project documentation at `docs/README.md:157`:
> "Use `cdc_log` for logging (never `ESP_LOG` directly)"

## Recommended Fix
1. **Replace all ESP_LOG with cdc_log** in mod_gpg module
2. **Add cdc_log dependency** to mod_gpg's CMakeLists.txt
3. **Audit CalEPD modules** - replace ESP_LOG with cdc_log or use local logging if appropriate
4. **Add lint rule** to prevent ESP_LOG usage in future commits

Example fix:
```cpp
// Instead of:
ESP_LOGI(TAG, "PW1 verified successfully");

// Use:
LOG_I(TAG, "PW1 verified successfully");
```

## References
- Project docs: `docs/README.md` line 157
- CDC Badge OS logging guide: `components/cdc_log/`
- OWASP: [Logging Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)

</content>