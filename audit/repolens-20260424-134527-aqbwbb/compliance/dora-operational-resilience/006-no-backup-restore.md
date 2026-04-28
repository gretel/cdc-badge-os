---
title: "[MEDIUM] No Backup and Restore Strategy for Device Data"
severity: MEDIUM
domain: Backup and Recovery
lens: dora-backup-recovery
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The firmware lacks:
- Backup strategy for NVS (Non-Volatile Storage) data
- Restore procedures for device configuration
- Data migration plan for firmware upgrades
- Recovery from corrupted NVS

Key searches performed:
- `grep -rn 'backup.*restore\|migrate\|export.*import' --include='*.cpp' --include='*.h'` - only found GPG key export, no general backup
- `find . -name '*backup*' -o -name '*restore*'` - no relevant results

## Impact
For financial entities:
- No data recovery if device is lost/damaged
- Cannot migrate data to new devices
- Risk of data loss during firmware upgrades
- No testing of backup restoration

## Evidence
Storage architecture (from `README.md` and code analysis):
- NVS stores: PINs, FIDO2 credentials, GPG keys, TOTP accounts, password vault
- TROPIC01 stores: ECC keys (32 slots), R-Memory (512 slots)
- **No backup mechanism** for NVS data
- **No export/import** for most data types

From code analysis:
- `components/mod_gpg/` has `gpg_export_pubkey_pem()` - exports public keys only
- `components/mod_nvsedit/` - NVS editor exists but no backup functionality
- No `backup_all_data()` or `restore_all_data()` functions found

From `components/mod_nvsedit/include/mod_nvsedit/NvsEditModule.h` (inferred):
- NVS editing capability exists
- No backup/restore functions defined

## Recommended Fix

1. **Create `components/data_backup/`** with:
   - `BackupManager.h/cpp` - Class for backup/restore operations
   - `BackupFormat.h` - Struct for backup data format
   - Encrypted backup storage in NVS or external storage

2. **Implement backup functions**:
   - `backup_all_data()` - Full backup of NVS data
   - `restore_all_data()` - Restore from backup
   - `backup_to_external()` - Export encrypted backup to USB/BLE

3. **Create `docs/BACKUP_RECOVERY.md`** with:
   - Backup procedures
   - Restore procedures
   - Backup retention policy
   - Testing procedures for backup restoration

4. **Add backup commands to serial interface**:
   - `BACKUP_ALL` - Create full backup
   - `RESTORE_ALL` - Restore from backup
   - `BACKUP_STATUS` - Show last backup info

## References
- DORA Regulation (EU) 2022/2554, Article 12 - Data backup
- EBA Guidelines on ICT risk management (EBA-GL-2021-07)
- ISO 27001:2013 A.12.3 - Information backup
