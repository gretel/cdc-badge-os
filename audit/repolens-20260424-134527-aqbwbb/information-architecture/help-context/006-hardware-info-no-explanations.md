---
title: "[LOW] Hardware info screen shows technical terms without explanations"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The hardware info screen (`components/cdc_os_ui/src/HardwareInfo.cpp`) displays technical information (I2C Bus, TROPIC01, PSRAM, NVS, etc.) but provides no explanations of what these components do or why they matter to the user.

**Evidence** (`components/cdc_os_ui/src/HardwareInfo.cpp`):
```cpp
REG(HW_I2C_BUS,         "I2C Bus",              "I2C Bus");
REG(HW_TROPIC01,        "TROPIC01",             "TROPIC01");
REG(HW_TR01_SESSION,    "TR01 Session",         "TR01 Sitzung");
REG(HW_PSRAM,           "PSRAM",                "PSRAM");
REG(HW_NVS,             "NVS",                  "NVS");
```

Users see technical specifications like:
- "I2C Bus: 10kHz"
- "TROPIC01: Session 5"
- "PSRAM: 2MB free"
- "NVS: 45 entries"

But no explanation of:
- What each component does
- Why the user should care
- What values are "normal" vs "concerning"

## Impact
Users viewing hardware info (likely for debugging) cannot:
1. Understand what each metric means
2. Know if values indicate a problem
3. Learn about the badge's architecture

## Evidence
- File: `components/cdc_os_ui/src/HardwareInfo.cpp`
- Lines: 153 (showInfo call)
- Technical terms displayed without definitions
- No expandable sections or "Learn more" links

## Recommended Fix
Add explanatory context:

1. **Grouped sections with descriptions**:
   ```
   MEMORY
   PSRAM: 2MB free (main RAM for applications)
   NVS: 45 entries (non-volatile storage)

   HARDWARE
   TROPIC01: Session 5 (secure element)
   I2C Bus: 10kHz (communication bus)
   ```

2. **Info key action** (key '3'):
   Tap any section to see a brief explanation

3. **Reference ranges**:
   ```
   PSRAM: 2MB free (normal: >500KB)
   ```

## References
- ESP32-S3 technical reference: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
- TROPIC01 datasheet: https://tropicshq.com/docs/
