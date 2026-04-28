---
title: "[MEDIUM] Partition Table Uses Fixed Offsets Without Flexibility"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The partition table (`default_16MB.csv`) uses fixed offsets for all partitions. This makes it difficult to update partition sizes or add new partitions without breaking existing firmware installations and requiring full device re-flashing.

**Location:** `default_16MB.csv`

## Impact
- **Inflexible updates**: Cannot add new partitions without breaking existing installs
- **Size constraints**: If firmware grows beyond `0x640000` (6.5MB), partition table must change
- **Migration complexity**: Users with existing data must wipe all partitions to update
- **No room for expansion**: SPIFFS and NVS sizes are fixed, no room for growth

## Evidence
Partition table uses absolute offsets:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x640000,
app1,     app,  ota_1,   0x650000,0x640000,
spiffs,   data, spiffs,  0xC90000,0x360000,
coredump, data, coredump,0xFF0000,0x10000,
```

Issues:
- NVS starts at `0x9000` (after bootloader at `0x0`)
- No gap for future bootloader expansion
- SPIFFS ends at `0xFC0000`, leaving only 256KB for coredump
- No room for additional data partitions

## Recommended Fix
1. **Use relative offsets** where possible:
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,     # Increased for growth
otadata,  data, ota,     0xF000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x640000,
app1,     app,  ota_1,    , 0x640000,         # Auto-position after app0
spiffs,   data, spiffs,   , 0x360000,         # Auto-position
coredump, data, coredump,  , 0x10000,         # Auto-position at end
```

2. **Add migration strategy**:
- Create partition migration tool for existing devices
- Document partition changes in release notes
- Provide NVS backup/restore utility

3. **Consider factory partition**:
```csv
factory,  app,  factory,  0x10000, 0x640000,   # For initial flash
```

## References
- [ESP-IDF Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html)
- [Partition Table Editor](https://github.com/espressif/esptool/blob/master/scripts/partition_table_editor.py)
