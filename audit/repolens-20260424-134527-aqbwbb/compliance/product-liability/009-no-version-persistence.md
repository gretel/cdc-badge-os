---
title: "[LOW] Firmware version not stored in non-volatile memory"
severity: LOW
domain: compliance/product-liability
lens: product-liability
labels:
  - version-tracking
  - firmware-management
---

## Summary
The firmware version is compiled into the binary (`APP_VERSION="0.5.0"` in `platformio.ini`) but not stored in non-volatile memory (NVS). This makes it harder to determine which version is running on a device without serial output.

**Evidence:**
- `platformio.ini`: `-D APP_VERSION=\"0.5.0\"`
- Version is a compile-time macro, not stored in NVS
- No `VERSION` key in NVS schema

## Impact
- Cannot programmatically check version on device (e.g., for update checks)
- Harder to track which users run which versions (for vulnerability notifications)
- Under Product Liability Directive, version tracking is important for patch management
- Makes debugging harder (users may not know which version they have)

## Evidence
```ini
platformio.ini:
build_flags =
    -D APP_VERSION=\"0.5.0\"
```

Version is used in compile-time, but no NVS storage found:
```bash
grep -rn "nv\|version.*store\|store.*version" /input/20260423-132359-oj8ayc/cdc-badge-os/components --include="*.cpp" --include="*.h" | grep -i "version" | head -10
# No relevant results
```

## Recommended Fix
1. **Store version in NVS on first boot**:
   ```cpp
   #include "nvs.h"
   
   void storeVersion() {
       nvs_handle_t nvs = nvs_open("version", NVS_READWRITE, &handle);
       nvs_set_str(nvs, "firmware", APP_VERSION);
       nvs_commit(nvs);
       nvs_close(nvs);
   }
   ```

2. **Add serial command to get version**:
   ```
   VERSION -> returns "0.5.0"
   ```

3. **Include version in settings menu**:
   Show "Firmware: v0.5.0" in Hardware Info screen

4. **Include version in bug reports**:
   Add to boot log: "CDC Badge OS v0.5.0"

## References
- ESP32 NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
