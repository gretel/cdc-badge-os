---
title: "[MEDIUM] Missing Backup and Restore Procedures for User Data"
severity: MEDIUM
domain: operational-docs
lens: documentation/operational-docs
labels:
  - "audit:documentation/operational-docs"
---

## Summary
The CDC Badge OS has no documented backup and restore procedures for user data. While the device has persistent storage (NVS for metadata, TROPIC01 R-Memory for encrypted secrets), there is no guidance on how to backup these data structures or restore them after a factory reset or device replacement.

**Where it should be:** New file `docs/BACKUP_RESTORE.md`

**Current state:**
- NVS commands exist (`NVS_LIST`, `NVS_READ`, `NVS_DEL`) in `SERIAL_COMMANDS.md`
- TROPIC01 commands exist for slot management
- No documented procedure for backing up all user data
- No restore procedure documented
- `tools/flash_firmware.py` has `--erase-nvs` option but no corresponding backup/restore tool

## Impact
**Data Loss Risk:**
- Users cannot backup TOTP secrets, password vault, or GPG keys before factory reset
- Device replacement requires re-adding all accounts manually
- No point-in-time recovery capability
- NVS corruption (common in ESP32) results in complete data loss

**Evidence:**
- `docs/SERIAL_COMMANDS.md:48-54` - NVS commands exist but no backup procedure
- `docs/SERIAL_COMMANDS.md:76` - `TR01_WIPE CONFIRM` is destructive with no backup steps
- `tools/flash_firmware.py` - Only flashes firmware, no data backup/restore
- `main/tropic_slot_map.h` - Memory map exists but no export/import tooling

## Recommended Fix
Create `docs/BACKUP_RESTORE.md` with:

1. **What Can Be Backed Up**
   - NVS namespaces (PIN config, settings, metadata)
   - TROPIC01 R-Memory slots (TOTP, passwords, FIDO credentials)
   - GPG public keys (for reference)

2. **Backup Procedure**
   - Serial-based backup using `NVS_LIST` and `NVS_READ`
   - TROPIC01 R-Memory export (if tooling exists)
   - Example script using serial console

3. **Restore Procedure**
   - Step-by-step restore after factory reset
   - Order of operations (NVS first, then TROPIC01)
   - Verification steps

4. **Backup Tools (to be created)**
   - Python script `tools/backup_data.py` for automated backup
   - Python script `tools/restore_data.py` for automated restore
   - JSON/YAML backup format specification

5. **Backup Best Practices**
   - When to backup (before major changes, regularly)
   - Secure storage of backup files (encrypted)
   - Testing restore procedure

6. **Limitations**
   - Private keys (TROPIC01 ECC slots) cannot be exported
   - Backup is device-specific (encrypted with device key)
   - Cross-device restore limitations

## References
- `docs/SERIAL_COMMANDS.md` - NVS and TROPIC01 commands
- `main/tropic_slot_map.h` - Storage allocation map
- `tools/flash_firmware.py` - Existing flash tool for reference
- ESP32 NVS documentation (corruption recovery)

---
**Related Issues:**
- Missing troubleshooting guide (#001)
- Missing factory reset procedure documentation
