---
title: "[MEDIUM] No backup and disaster recovery procedure for device data"
severity: MEDIUM
domain: business-continuity
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The device has no backup strategy for user data (passwords, TOTP secrets, SSH keys). If the device is lost, damaged, or the TROPIC01 is corrupted, there is no documented recovery procedure. NIS2 requires "business continuity" planning including "backup management" (Art. 20).

## Impact
**NIS2 Art. 20 Business Continuity Gap**:
- No Recovery Time Objective (RTO) defined
- No Recovery Point Objective (RPO) defined
- Lost device = lost all credentials
- No failover capability
- No data backup verification or restore testing

## Evidence
1. **No backup mechanism**:
   - File: `components/cdc_hal/src/Rtc.cpp`
   ```cpp
   // We do NOT restore from NVS - better no time than stale time
   ```
   - Explicitly avoids restoring from backup for time
   - No similar backup for other data

2. **No backup documentation**:
   - No `BACKUP.md` or `DISASTER_RECOVERY.md` in `/docs/`
   - No procedure for backing up passwords, TOTP secrets
   - No procedure for restoring to new device

3. **Secure element is non-exportable by design**:
   - File: `README.md:95-96`
   ```
   - Keys cannot be extracted or cloned
   - 32 ECC key slots (P-256 and Ed25519)
   ```
   - This is good for security but means no backup of keys
   - R-Memory data also not easily backed up

4. **No failover mechanism**:
   - Single device = single point of failure
   - No secondary device support
   - No "badge pair" for redundancy

5. **NVS data not backed up**:
   - File: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
   - NVS stores module configuration but no backup procedure

## Recommended Fix
1. **Create Backup Procedure Documentation** (`docs/BACKUP.md`):
   - Document what can be backed up (R-Memory slots)
   - Document what cannot be backed up (ECC keys - by design)
   - Provide serial commands for exporting data
   - Include encryption requirements for backup files

2. **Implement Data Export Feature**:
   - Add serial command: `BACKUP PASSWORDS` - export encrypted password vault
   - Add serial command: `BACKUP TOTP` - export TOTP secrets (encrypted)
   - Store backup with timestamp and checksum

3. **Implement Restore Procedure**:
   - Add serial command: `RESTORE PASSWORDS <file>` - import encrypted backup
   - Verify checksum and encryption before restore
   - Document restore procedure in user guide

4. **Define RTO/RPO**:
   - Document expected recovery time
   - Document data loss tolerance
   - Set user expectations

5. **Add Backup Verification**:
   - Verify backup file integrity
   - Test restore procedure periodically
   - Document verification steps

## References
- [NIS2 Directive Art. 20 - Business continuity](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-34 Rev. 1 - Contingency Planning](https://csrc.nist.gov/publications/detail/sp/800-34/rev-1/final)
- [ENISA Business Continuity](https://www.enisa.europa.eu/topics/organizational-resilience/business-continuity)
