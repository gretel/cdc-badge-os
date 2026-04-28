---
title: "[MEDIUM] Missing audit trail for TOTP and Password vault operations"
severity: MEDIUM
domain: secrets-management
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary
TOTP account and Password vault CRUD operations lack audit logging. Both `TotpStore` (components/mod_totp/src/TotpStore.cpp) and `PasswordStore` (components/mod_password/src/PasswordStore.cpp) perform state changes without audit records.

**Missing audit events:**
1. TOTP account creation (`TotpStore::addAccount`)
2. TOTP account update (`TotpStore::updateAccount`)
3. TOTP account deletion (`TotpStore::deleteAccount`)
4. Password entry creation (`PasswordStore::addEntry`)
5. Password entry update (`PasswordStore::updateEntry`)
6. Password entry deletion (`PasswordStore::deleteEntry`)
7. TOTP code generation (read access to secrets)

## Impact
- **Data access tracking**: No record of when TOTP secrets or password entries were created/modified/deleted
- **Compliance**: Password vaults typically require audit trails for compliance (e.g., SOC2, ISO 27001)
- **Forensics**: Cannot determine when a specific password entry was last modified
- **Sensitive data reads**: TOTP code generation reads secret keys but isn't logged

## Evidence
```cpp
// TotpStore.cpp:235 - Account creation
bool TotpStore::addAccount(const char* name, const char* issuer, ...) {
    // ... decode secret ...
    auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0, ...);
    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }
    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, name, 0);
    return true;  // No audit event!
}

// PasswordStore.cpp:207 - Entry creation
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // ... prepare payload ...
    auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0, ...);
    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }
    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, headerName, 0);
    return true;  // No audit event!
}

// TotpStore.cpp:480 - TOTP code generation (read access)
int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    TotpAccount account = {};
    if (!readAccount(slot, &account)) return -1;
    // ... generate code from secret ...
    snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));
    return seconds_remaining;  // Secret was accessed but not logged!
}
```

## Recommended Fix
1. Create audit helper in `components/mod_totp/include/mod_totp/totp_audit.h`:
   ```cpp
   void totp_audit_account_added(const char* name, const char* issuer);
   void totp_audit_account_updated(uint16_t slot, const char* name);
   void totp_audit_account_deleted(uint16_t slot);
   void totp_audit_code_generated(uint16_t slot);
   ```

2. Create audit helper in `components/mod_password/include/mod_password/password_audit.h`:
   ```cpp
   void password_audit_entry_added(const char* title);
   void password_audit_entry_updated(uint16_t slot, const char* title);
   void password_audit_entry_deleted(uint16_t slot);
   void password_audit_entry_read(uint16_t slot);  // For password display
   ```

3. Add calls:
   - After successful write in `addAccount()`, `updateAccount()`, `deleteAccount()`
   - After successful write in `addEntry()`, `updateEntry()`, `deleteEntry()`
   - At start of `generateCode()` for TOTP access tracking
   - When password entry is read for display

## References
- OWASP Password Storage Cheat Sheet
- NIST SP 800-63B (Section 5.1.1.2 Memorized Secret Verifiers)
- ISO/IEC 27001:2022 (A.12.4 Logging and Event Correlation)
