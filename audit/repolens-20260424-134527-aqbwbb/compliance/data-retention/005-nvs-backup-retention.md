---
title: "[LOW] No Backup Retention Policy for NVS Data"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
NVS (Non-Volatile Storage) contains all persistent metadata and configuration data. There is no backup mechanism, no backup retention policy, and no process for restoring data after NVS corruption or wipe.

**Location**: NVS namespaces used throughout: `tr01_meta`, `fido2`, `modules`, `mod_totp`, `mod_password`, `mod_gpg`, etc. (see `components/cdc_core/src/TropicStorage.cpp:12`, `fido2_storage.cpp:22`, `ModuleRegistry.cpp:350`)

## Impact
1. **Single Point of Failure**: NVS corruption = total data loss
2. **No Recovery**: No way to restore FIDO2 credentials, TOTP secrets after NVS wipe
3. **Backup Retention Unknown**: If users backup NVS externally, no guidance on how long to keep backups
4. **Factory Reset Risk**: `NVS_CLEAR` command erases everything with no undo

## Evidence
**NVS namespaces in use**:
- `tr01_meta` (TROPIC01 cache) - `TropicStorage.cpp:12`
- `fido2` (FIDO2 counter) - `fido2_storage.cpp:22`
- `modules` (module list) - `ModuleRegistry.cpp:351`
- `mod_hid` (HID settings) - `BleHidKeyboard.cpp:23`
- `gpg` (GPG metadata) - `mod_gpg/src/gpg.cpp`

**NVS clear command** (`docs/SERIAL_COMMANDS.md`, line 55):
```
NVS_CLEAR YES  # Erase entire NVS [AUTH]
```
No warning about data loss, no backup before clear.

**No backup/restore commands**:
- No `NVS_BACKUP` command
- No `NVS_RESTORE` command
- No versioning of NVS data

## Recommended Fix
Add backup/restore functionality:

**Option 1: Serial commands for backup**
```bash
NVS_BACKUP <filename>  # Export all namespaces to file
NVS_RESTORE <filename> # Import from file [AUTH]
```

**Option 2: Module-level backup**
Each module implements backup of its specific data:
```bash
FIDO2_BACKUP <slot_range>
TOTP_BACKUP
PASSWORD_BACKUP
```

**Option 3: Document backup retention**
Add to `docs/DATA_RETENTION.md`:
```markdown
## NVS Backups
- **Retention**: Same as primary data (indefinite until user deletes)
- **Location**: External storage (user-managed)
- **Process**: Use `NVS_BACKUP` command, store securely
```

## References
- ESP-IDF NVS API (`nvs_flash.h`)
- Backup best practices for embedded systems
