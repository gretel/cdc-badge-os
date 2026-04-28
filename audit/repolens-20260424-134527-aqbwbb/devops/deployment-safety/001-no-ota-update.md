---
title: "[MEDIUM] No OTA Firmware Update Mechanism Despite Partition Table Support"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

The ESP32-S3 partition table (`default_16MB.csv`) is configured with dual OTA partitions (`app0` and `app1` with `ota_0` and `ota_1` subtypes), but no OTA update mechanism is implemented in the firmware. This means:

1. **No in-field updates**: Devices cannot be updated over-the-air via WiFi/BLE
2. **Full reflash required**: Every update requires physical USB connection and bootloader mode
3. **OTA infrastructure unused**: The `otadata` partition and dual-app layout is wasted

**Evidence:**
- Partition table: `default_16MB.csv` shows OTA layout:
  ```
  otadata,  data, ota,     0xe000,  0x2000,
  app0,     app,  ota_0,   0x10000, 0x640000,
  app1,     app,  ota_1,   0x650000,0x640000,
  ```
- No code references to `esp_ota_*` functions found in codebase
- No OTA update module or service implemented

## Impact

**Operational:**
- Users must physically access each badge to update firmware
- Field deployments require manual reflashing of each device
- Emergency security patches require physical distribution

**Maintenance:**
- OTA partitions occupy ~1.2MB of flash that could be used for other features
- Partition table complexity without benefit

**User Experience:**
- Updates are inconvenient (enter bootloader mode, USB connection)
- No seamless background updates

## Evidence

1. **Partition table** (`default_16MB.csv`):
   ```
   # Name,   Type, SubType, Offset,  Size, Flags
   nvs,      data, nvs,     0x9000,  0x5000,
   otadata,  data, ota,     0xe000,  0x2000,
   app0,     app,  ota_0,   0x10000, 0x640000,
   app1,     app,  ota_1,   0x650000,0x640000,
   ```

2. **No OTA code found:**
   ```bash
   grep -r "esp_ota" components/  # Returns nothing
   find components/ -name "*ota*" # Returns nothing
   ```

3. **Current flash method** uses Python tool (`tools/flash_firmware.py`) and web flasher that require physical USB connection.

## Recommended Fix

Implement a basic OTA update mechanism:

1. **Add OTA service module** (`components/mod_ota/`):
   - Download firmware binary via WiFi (from GitHub releases or custom server)
   - Write to inactive OTA partition using `esp_ota_begin()`, `esp_ota_write()`, `esp_ota_end()`
   - Mark partition as bootable with `esp_ota_set_boot_partition()`
   - Reboot to apply update

2. **Add serial command** for triggering OTA:
   ```
   OTA_CHECK <url>    # Check for update at URL
   OTA_UPDATE         # Download and install update
   OTA_STATUS         # Show current OTA info
   ```

3. **Add UI menu item** for OTA updates (optional, for advanced users)

4. **Verify update** on boot:
   - Check firmware signature/hash
   - Rollback to previous partition if boot fails 3 times

**Estimated effort:** 2-3 hours for basic implementation

## References

- [ESP-IDF OTA Update Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/esp-idf/en/latest/esp32s3/api-reference/system/ota.html)
- [ESP32 OTA Overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/esp-idf/en/latest/esp32s3/api-guides/ota-firmware-update.html)
- Partition table layout: `default_16MB.csv`

</content>