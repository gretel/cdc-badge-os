---
title: "[LOW] Missing Factory Reset and Re-provisioning Procedure"
severity: LOW
domain: operational-docs
lens: documentation/operational-docs
labels:
  - "audit:documentation/operational-docs"
---

## Summary
There is no documented factory reset procedure with clear steps for wiping all data and re-provisioning the device. While individual reset commands exist (`TR01_WIPE`, `NVS_CLEAR`), there is no complete end-to-end procedure for a full factory reset and device re-setup.

**Where it should be:** Section in `BACKUP_RESTORE.md` or new file `docs/FACTORY_RESET.md`

**Current state:**
- `SERIAL_COMMANDS.md:53-54` - `NVS_CLEAR YES` command documented
- `SERIAL_COMMANDS.md:76` - `TR01_WIPE CONFIRM` command documented
- `README.md:128` - `flash_firmware.py --erase-nvs` option mentioned
- No complete procedure combining all reset steps
- No re-provisioning guide (setting PIN, time, initial config)

## Impact
**Operational Impact:**
- Users may perform incomplete resets (e.g., only NVS, leaving TROPIC01 data)
- Risk of data loss if reset order is wrong
- No clear steps for re-provisioning after reset
- Device sold/traded with residual data if reset is incomplete

**Evidence:**
- `docs/SERIAL_COMMANDS.md:53-54` - `NVS_CLEAR YES` - Erase entire NVS
- `docs/SERIAL_COMMANDS.md:76` - `TR01_WIPE CONFIRM` - Factory reset all TR01 data
- `tools/flash_firmware.py` - Has `--erase-nvs` but no full reset procedure
- No documentation on which commands to run in what order

## Recommended Fix
Create factory reset procedure with:

1. **Pre-Reset Checklist**
   - Backup user data (see `BACKUP_RESTORE.md`)
   - Export public keys for reference
   - List TOTP accounts to re-add later
   - Note any custom settings

2. **Full Factory Reset Steps**
   ```
   Step 1: Clear NVS
   NVVS_CLEAR YES
   
   Step 2: Wipe TROPIC01
   TR01_WIPE CONFIRM
   
   Step 3: Flash fresh firmware (optional)
   python tools/flash_firmware.py --release latest --erase-nvs
   ```

3. **Partial Reset Options**
   - NVS only: `NVS_CLEAR YES` (keeps TROPIC01 keys)
   - TROPIC01 only: `TR01_WIPE CONFIRM` (keeps NVS settings)
   - Module-specific: `TR01_CLEANUP` for module data only

4. **Re-provisioning Steps**
   - Power cycle device
   - Set new PIN (Settings → Change PIN)
   - Set time via WiFi or serial: `SET_DATE <timestamp>`
   - Re-add TOTP accounts
   - Re-import GPG keys if applicable

5. **Verification**
   - Run `STATUS` to confirm clean state
   - Run `TR01_SLOTS` to verify empty slots
   - Run `PIN_STATUS` to confirm default PIN

## References
- `docs/SERIAL_COMMANDS.md` - Reset commands
- `tools/flash_firmware.py` - Firmware flashing
- `docs/BACKUP_RESTORE.md` - Backup before reset

---
**Related Issues:**
- Missing backup/restore procedures (#002)
