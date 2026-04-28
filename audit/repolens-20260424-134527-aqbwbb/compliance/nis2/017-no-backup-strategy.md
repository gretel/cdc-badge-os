---
title: "[MEDIUM] No backup strategy for device configuration and user data"
severity: MEDIUM
domain: business-continuity
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The CDC Badge OS lacks a documented backup strategy for user data (FIDO2 credentials, TOTP secrets, passwords, GPG keys). NIS2 Article 20 requires "backup and restoration" capabilities. Without backup, device failure or corruption means permanent loss of all credentials.

## Impact
**NIS2 Art. 20 Business Continuity Gap**:
- No mechanism to backup FIDO2 credentials
- No export of TOTP secrets (can't restore to new device)
- No backup of password vault
- Device failure = complete credential loss
- No RTO/RPO defined

## Evidence

1. **No backup command in serial interface**:
   - File: `docs/SERIAL_COMMANDS.md`
   - Available commands: `HELP`, `VERSION`, `SET_DATE`, `PIN_CHANGE`, etc.
   - Missing: `BACKUP`, `RESTORE`, `EXPORT`, `IMPORT`

2. **FIDO2 credentials stored in ECC slots**:
   - File: `components/mod_fido2/include/mod_fido2/fido2_storage.h`
   - Credentials in TROPIC01 ECC slots 5-31 (non-exportable by design)
   - No mechanism to backup to encrypted file

3. **TOTP secrets stored in R-Memory**:
   - File: `components/mod_totp/src/TotpStore.cpp`
   - Secrets in R-Memory slots 32-131
   - No export/import commands

4. **Password vault in R-Memory**:
   - File: `components/mod_password/src/PasswordStore.cpp`
   - Passwords in R-Memory slots 150-511
   - No backup mechanism

5. **GPG keys in ECC slots**:
   - File: `docs/GPG.md:88-97`
   - SIG/DEC/AUT keys in slots 1-3
   - DEC key encrypted in R-Memory but no backup command

6. **No NVS backup**:
   - File: `components/mod_nvsedit/src/NvsEditModule.cpp`
   - NVS editor can read/write but no backup/restore

## Recommended Fix

1. **Implement encrypted backup command**:
   ```bash
   # Create backup (requires PIN)
   BACKUP CREATE /path/to/backup.bin
   
   # Restore from backup
   BACKUP RESTORE /path/to/backup.bin
   
   # List backups
   BACKUP LIST
   ```

2. **Backup structure**:
   ```cpp
   struct BackupHeader {
       uint32_t magic;           // "CDCB"
       uint32_t version;
       uint32_t timestamp;
       uint8_t salt[16];
       uint8_t iv[12];
       uint32_t size;
   };
   
   struct BackupEntry {
       uint8_t moduleId;
       uint16_t dataSize;
       uint8_t data[];
   };
   ```

3. **Per-module backup handlers**:
   ```cpp
   // Each module implements backup interface
   interface IBackup {
       bool backup(BackupEntry* entry);
       bool restore(const BackupEntry& entry);
   };
   
   // FIDO2 backup
   bool fido2_backup(BackupEntry* entry) {
       // Export ECC slot metadata (not private keys)
       // Export credential IDs, RP IDs, user names
   }
   
   // TOTP backup
   bool totp_backup(BackupEntry* entry) {
       // Export secrets (encrypted)
       // Export account names, time offsets
   }
   ```

4. **Export to QR code for portable backup**:
   ```cpp
   // Generate QR code for TOTP secrets
   bool TOTP_EXPORT QR
   // Displays QR code on E-Paper
   // Scan with new badge to restore
   ```

5. **Add backup verification**:
   ```bash
   BACKUP VERIFY /path/to/backup.bin
   # Checks integrity, shows contents summary
   ```

6. **Document backup procedure**:
   - File: `docs/BACKUP.md`
   - How to create backup
   - How to restore
   - Recommended frequency (weekly)
   - Storage recommendations (encrypted, offline)

## References
- [NIS2 Directive Art. 20 - Backup and restoration](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-34 Rev. 1 - Contingency Planning](https://csrc.nist.gov/publications/detail/sp/800-34/rev-1/final)
- [FIDO2 Credential Backup](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#credential-backup)

</content>