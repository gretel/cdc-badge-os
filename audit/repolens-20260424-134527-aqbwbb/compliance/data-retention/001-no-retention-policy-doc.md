---
title: "[MEDIUM] Missing Data Retention Policy Documentation"
severity: MEDIUM
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
The codebase lacks any documented retention policies for the various data categories stored by the CDC Badge OS. There is no documentation specifying:
- How long different types of data should be retained
- What the legal or operational basis is for each data type
- When data should be archived or deleted

**Location**: No documentation file exists in `docs/` directory covering retention policies.

## Impact
Without documented retention policies:
1. **Compliance Risk**: Difficult to demonstrate compliance with GDPR "storage limitation" principle (data kept no longer than necessary)
2. **Developer Uncertainty**: Engineers have no reference for implementing retention logic
3. **Audit Gap**: No baseline for auditors to verify data lifecycle management
4. **Inconsistent Treatment**: Different modules may implement ad-hoc retention logic (or none at all)

## Evidence
Data stored by the system includes:
- **FIDO2 credentials** (`components/mod_fido2/src/fido2_storage.cpp`): Stored in TROPIC01 R-Memory slots with no expiration
- **TOTP secrets** (`components/mod_totp/src/TotpStore.cpp`): Stored in TROPIC01 R-Memory slots indefinitely
- **Password entries** (`components/mod_password/src/PasswordStore.cpp`): Stored in TROPIC01 R-Memory slots with no expiry
- **GPG keys** (`components/mod_gpg/src/GpgStorage.cpp`): Stored in TROPIC01 ECC slots and R-Memory
- **Error logs** (`components/cdc_log/src/cdc_log.cpp`): 50-entry ring buffer in PSRAM (volatile)
- **NVS metadata** (`components/cdc_core/src/TropicStorage.cpp`, `ModuleRegistry.cpp`): Persisted in NVS

None of these data stores have retention period configuration or documentation.

## Recommended Fix
Create a new documentation file `docs/DATA_RETENTION.md` that:
1. Lists all data categories stored by the badge
2. Specifies retention period for each (e.g., "FIDO2 credentials: Until user manually deletes")
3. Documents the legal/operational basis for each retention period
4. Identifies which data is "temporary" vs "persistent"
5. References any regulatory requirements (if applicable)

Example structure:
```markdown
# Data Retention Policy

## TOTP Secrets
- **Retention**: Indefinite (until manual deletion)
- **Basis**: User-generated secrets, no regulatory expiry
- **Location**: TROPIC01 R-Memory slots 32-131

## FIDO2 Credentials
- **Retention**: Indefinite (until manual deletion)
- **Basis**: User-authentication data, no automatic expiry
- **Location**: TROPIC01 ECC slots 5-31, R-Memory slots 5-31
...
```

## References
- GDPR Article 5(1)(e) - Storage limitation principle
- NIST SP 800-53 - DM-2 Data Retention and Disposal
