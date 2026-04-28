---
title: "[MEDIUM] No key lifecycle management for TROPIC01 ECC slots (GPG, FIDO2, CA)"
severity: MEDIUM
domain: encryption
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The TROPIC01 ECC key slots (0-31) lack a comprehensive key lifecycle management system. While finding #10 covers FIDO2 attestation specifically, this finding addresses the broader gap: no systematic key rotation, expiration tracking, or retirement procedure for **all** ECC keys stored in the secure element (GPG signature/decryption/authentication keys, CA keys, FIDO2 credential keys). NIS2 requires "state-of-the-art" encryption with proper key management (Art. 20).

## Impact
**NIS2 Art. 20 Key Management Gap**:
- Keys can remain valid indefinitely without review
- No expiration date for GPG, CA, or FIDO2 keys
- No rotation schedule for credential keys
- No process for key retirement when roles change
- Compromised keys remain valid until manually deleted

## Evidence

1. **No key metadata storage for ECC slots**:
   - File: `components/cdc_hal/include/cdc_hal/ISecureElement.h:27-35`
   ```cpp
   enum class SeResult : uint8_t {
       OK,
       ERROR,
       SESSION_REQUIRED,
       SLOT_EMPTY,
       SLOT_OCCUPIED,
       INVALID_PARAM,
       ALARM_MODE,
       NOT_SUPPORTED
   };
   ```
   - No `SeKeyInfo` struct to store creation date, expiration, purpose, owner

2. **GPG keys have no expiration tracking**:
   - File: `docs/GPG.md:88-104`
   - Keys stored in slots 1-3
   - Metadata stored in NVS but no expiration field
   - `GPG_STATUS` shows generation time but no validity period

3. **No key rotation commands**:
   - File: `docs/SERIAL_COMMANDS.md`
   - Available: `GPG_GENERATE`, `GPG_RESET`, `GPG_STATUS`
   - Missing: `KEY_ROTATE`, `KEY_EXPIRE`, `KEY_LIST`, `KEY_REVOKE`

4. **No centralized key registry**:
   - File: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
   - ModuleRegistry tracks modules but not individual keys
   - No `KeyRegistry.h` for tracking all ECC keys across modules

5. **FIDO2 credential keys lack lifecycle**:
   - File: `components/mod_fido2/src/ctap2.cpp`
   - Credential IDs stored but no creation/expiration tracking
   - `GetAssertion` doesn't check key validity period

6. **CA keys (slot 4) have no lifecycle**:
   - If CA module is added, keys will have same issue
   - No certificate validity period enforcement

## Recommended Fix

1. **Create Key Information Structure**:
   ```cpp
   // components/cdc_hal/include/cdc_hal/SeKeyInfo.h
   struct SeKeyInfo {
       uint8_t slot;
       uint8_t moduleId;           // Which module owns this key
       uint32_t createdAt;         // Unix timestamp
       uint32_t expiresAt;         // 0 = no expiration
       uint8_t purpose;            // SIG, DEC, AUT, CA, FIDO2
       char owner[32];             // Human-readable owner
       uint8_t status;             // ACTIVE, EXPIRED, REVOKED
   };
   ```

2. **Implement Key Registry**:
   - Create `SeKeyRegistry` class in `cdc_hal`
   - Store key info in TROPIC01 R-Memory slot 0 (extend PIN storage)
   - Provide API: `registerKey()`, `getKeyInfo()`, `listKeys()`, `expireKey()`

3. **Add Key Lifecycle Commands**:
   ```bash
   # List all keys with metadata
   KEY_LIST
   
   # Show key details
   KEY_INFO <slot>
   
   # Set expiration (seconds from now, 0 = no expiry)
   KEY_EXPIRE <slot> <seconds>
   
   # Rotate key (create new, mark old as superseded)
   KEY_ROTATE <slot>
   
   # Revoke key (immediate invalidation)
   KEY_REVOKE <slot>
   ```

4. **Update GPG Module**:
   - Add expiration tracking to GPG keys
   - `GPG_STATUS` shows validity period
   - Warn when key is near expiration

5. **Update FIDO2 Module**:
   - Track credential creation time
   - Support credential expiration (FIDO2 spec allows this)
   - Auto-clean expired credentials

6. **Add Key Review Process**:
   - Periodic key review (e.g., every 90 days)
   - Command to list keys needing review: `KEY_REVIEW`
   - Document key rotation procedure

## References
- [NIS2 Directive Art. 20 - Encryption and key management](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-57 Part 1 Rev. 5 - Key Management](https://csrc.nist.gov/publications/detail/sp/800-57/part-1/final)
- [FIDO2 Credential Management](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#credential-management)
- [OpenPGP SmartCard Specification v3.4](https://g10code.com/docs/openpgp-card.html)

</content>