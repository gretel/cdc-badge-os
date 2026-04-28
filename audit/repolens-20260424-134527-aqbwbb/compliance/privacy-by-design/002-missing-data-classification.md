---
title: "[MEDIUM] No data classification scheme for sensitive fields (passwords, TOTP secrets, vCards)"
severity: MEDIUM
domain: Privacy by Design
lens: Missing-Data-Classification
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The codebase lacks a formal data classification scheme to distinguish between different sensitivity levels of stored data. All data is treated uniformly without explicit markers or annotations indicating which fields contain sensitive personal data (passwords, TOTP secrets, vCard names, email addresses, etc.).

**Affected Components:**
- `components/mod_password/` - Password entries with username, password, URL fields
- `components/mod_totp/` - TOTP accounts with issuer and secret fields
- `components/mod_vcard/` - vCards with names, emails, phone numbers
- `components/mod_gpg/` - GPG key metadata

## Impact
- **No Automated Protection:** Without data classification, automated tools cannot identify sensitive fields for encryption, masking, or access control.
- **Inconsistent Logging:** Developers may inadvertently log sensitive data because there's no clear indicator of what constitutes PII.
- **Harder Compliance Audits:** GDPR, CCPA, and other privacy regulations require clear identification of personal data for proper handling.
- **Risk of Data Leakage:** Future code changes may expose sensitive data without triggering appropriate safeguards.

## Evidence
**File:** `components/mod_password/include/mod_password/PasswordStore.h`
```cpp
struct PasswordEntry {
    char title[32];
    char username[32];
    char password[32];
    char url[48];
    uint8_t totpSlot;
    char notes[64];
};
```
The `password` field contains highly sensitive data but has no classification marker.

**File:** `components/mod_totp/include/mod_totp/TotpStore.h`
```cpp
struct TotpAccount {
    char name[32];
    char issuer[16];
    uint8_t secret[SECRET_LEN];  // 20 bytes
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};
```
The `secret` field is critical PII (used for 2FA) but has no classification marker.

**File:** `components/mod_vcard/include/mod_vcard/vcard_store.h`
```cpp
#define VCARD_MAX_LEN   768
#define VCARD_MAX_CARDS 100
```
vCards contain names, emails, phone numbers, social profiles - all PII - but no classification exists.

## Recommended Fix
1. **Create a data classification enum** in `components/cdc_core/include/cdc_core/`:
   ```cpp
   enum class DataClassification {
       PUBLIC = 0,       // No restrictions
       INTERNAL,         // Internal use only
       CONFIDENTIAL,     // Sensitive business data
       PII,              // Personal identifiable information
       SECRET            // Cryptographic secrets, passwords
   };
   ```

2. **Add classification to data structures:**
   ```cpp
   struct PasswordEntry {
       char title[32];
       char username[32];
       char password[32];
       char url[48];
       uint8_t totpSlot;
       char notes[64];
       DataClassification classification;  // e.g., SECRET for password
   };
   ```

3. **Create classification helper functions:**
   ```cpp
   bool isSensitiveData(DataClassification cls);
   bool shouldLogData(DataClassification cls);
   ```

4. **Update logging macros** to check classification before printing:
   ```cpp
   #define LOG_PII(tag, fmt, ...) \
       do { if (shouldLogData(DataClassification::PII)) LOG_I(tag, fmt, ##__VA_ARGS__); } while(0)
   ```

## References
- GDPR Article 5 - Principles relating to processing of personal data
- ISO 27001 - Information security management (data classification)
- NIST Privacy Framework: A01 (Identify-Data Classification)
