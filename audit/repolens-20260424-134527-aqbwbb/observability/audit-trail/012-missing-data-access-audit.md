---
title: "[MEDIUM] Missing audit trail for sensitive data access"
severity: MEDIUM
domain: secrets-management
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary

Sensitive data access (reading passwords, TOTP secrets) lacks audit logging. When password entries are displayed or TOTP codes are generated, no audit record tracks:
- Which data was accessed (entry name, slot)
- When it was accessed (timestamp)
- How it was accessed (serial, UI, BLE)
- Whether the access was successful

This is distinct from CRUD operations (already covered in existing findings) - this is about tracking **read access** to sensitive data.

**Files affected:**
- `components/mod_password/src/PasswordStore.cpp:116` - `PasswordStore::readEntry()` - Reads password entry
- `components/mod_password/src/PasswordModule.cpp:232` - Display password entry
- `components/mod_totp/src/TotpStore.cpp:480` - `TotpStore::generateCode()` - Generates TOTP code
- `components/mod_totp/src/TotpModule.cpp` - Display TOTP codes

## Impact

1. **Data access tracking**: Cannot determine when sensitive data (passwords, TOTP secrets) was accessed.
2. **Compliance**: Password vaults typically require audit trails for data access (e.g., SOC2, ISO 27001).
3. **Forensics**: If data is compromised, cannot determine which entries were accessed and when.
4. **Accountability**: No record of who accessed which password entry.
5. **Privacy**: TOTP secret access isn't tracked, making it hard to audit code generation.

## Evidence

### Password read lacks audit trail

In `PasswordStore.cpp:116-153`:
```cpp
bool PasswordStore::readEntry(uint16_t slot, PasswordEntry* out) const {
    if (!out) return false;
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    cdc::hal::ISecureElement::RMemHeader header = {};
    PasswordPayload payload = {};
    uint16_t payloadLen = 0;

    auto res = se->rmemReadWithHeader(physSlot, &header,
                                      reinterpret_cast<uint8_t*>(&payload),
                                      sizeof(payload), &payloadLen);
    if (res != cdc::hal::SeResult::OK) {
        return false;  // No audit event for failed read!
    }

    if (header.moduleId != moduleId_) {
        return false;  // No audit event for wrong module!
    }

    memset(out, 0, sizeof(*out));
    if (payload.title[0]) {
        copyText(out->title, sizeof(out->title), payload.title);
    } else {
        copyText(out->title, sizeof(out->title), header.name);
    }
    copyText(out->username, sizeof(out->username), payload.username);
    copyText(out->password, sizeof(out->password), payload.password);  // Secret accessed!
    copyText(out->url, sizeof(out->url), payload.url);
    out->totpSlot = payload.totpSlot;
    copyText(out->notes, sizeof(out->notes), payload.notes);

    return true;  // No audit event for successful read!
}
```

### Password display lacks audit

In `PasswordModule.cpp:232-250`:
```cpp
static void showPassword(uint16_t slot) {
    PasswordEntry entry;
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        ui::showMessage(ui::StringId::ERROR);
        return;
    }

    // Display password (secret accessed!)
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Title: %s\nUser: %s\nPassword: %s",
             entry.title, entry.username, entry.password);  // Password displayed!

    ui::showText(buffer);
    // No audit event for password display!
}
```

### TOTP code generation lacks audit

In `TotpStore.cpp:480-506`:
```cpp
int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    if (!codeOut) return -1;

    TotpAccount account = {};
    if (!readAccount(slot, &account)) {
        return -1;  // No audit event for failed read!
    }

    if (!isTimeValid()) {
        strcpy(codeOut, "------");
        return -1;  // No audit event for invalid time!
    }

    uint32_t code = generate(account.secret, account.secretLen, time(nullptr),
                             account.period, account.digits,
                             static_cast<TotpAlgorithm>(account.algorithm));
    // Secret was accessed to generate code!

    if (account.digits == 8) {
        snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));
    } else if (account.digits == 7) {
        snprintf(codeOut, 8, "%07lu", static_cast<unsigned long>(code));
    } else {
        snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));
    }

    return static_cast<int8_t>(timeRemaining(account.period));
    // No audit event for code generation!
}
```

### TOTP display lacks audit

In `TotpModule.cpp` (code generation display):
```cpp
// When displaying TOTP code to user
int8_t seconds = TotpStore::instance().generateCode(slot, codeBuffer);
// Secret accessed and code generated but no audit event!
```

## Recommended Fix

### Step 1: Create audit helper for password access (15 min)

Create `components/mod_password/include/mod_password/password_audit.h`:

```cpp
#ifndef MOD_PASSWORD_PASSWORD_AUDIT_H
#define MOD_PASSWORD_PASSWORD_AUDIT_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Audit record for password entry read.
 * \param slot Logical slot index.
 * \param name Entry name/title.
 * \param success Whether read succeeded.
 * \param context Access context ("SERIAL", "UI", "BLE").
 */
void password_audit_entry_read(uint16_t slot, const char* name, bool success, const char* context);

/**
 * \brief Audit record for password display.
 * \param slot Logical slot index.
 * \param name Entry name/title.
 */
void password_audit_entry_display(uint16_t slot, const char* name);

#ifdef __cplusplus
}
#endif

#endif // MOD_PASSWORD_PASSWORD_AUDIT_H
```

### Step 2: Create audit helper for TOTP access (15 min)

Create `components/mod_totp/include/mod_totp/totp_audit.h`:

```cpp
#ifndef MOD_TOTP_TOTP_AUDIT_H
#define MOD_TOTP_TOTP_AUDIT_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Audit record for TOTP code generation.
 * \param slot Logical slot index.
 * \param name Account name.
 * \param success Whether generation succeeded.
 * \param context Access context ("SERIAL", "UI", "BLE").
 */
void totp_audit_code_generated(uint16_t slot, const char* name, bool success, const char* context);

#ifdef __cplusplus
}
#endif

#endif // MOD_TOTP_TOTP_AUDIT_H
```

### Step 3: Implement password audit functions (10 min)

Add to `components/mod_password/src/PasswordStore.cpp`:

```cpp
#include "mod_password/password_audit.h"

static void password_audit_entry_read(uint16_t slot, const char* name, bool success, const char* context) {
    if (success) {
        LOG_I(TAG, "AUDIT: PASSWORD READ | slot=%d | name=%s | context=%s",
              slot, name ? name : "unknown", context ? context : "unknown");
    } else {
        LOG_I(TAG, "AUDIT: PASSWORD READ FAIL | slot=%d | name=%s | context=%s",
              slot, name ? name : "unknown", context ? context : "unknown");
    }
}
```

### Step 4: Implement TOTP audit functions (10 min)

Add to `components/mod_totp/src/TotpStore.cpp`:

```cpp
#include "mod_totp/totp_audit.h"

static void totp_audit_code_generated(uint16_t slot, const char* name, bool success, const char* context) {
    if (success) {
        LOG_I(TAG, "AUDIT: TOTP GENERATED | slot=%d | name=%s | context=%s",
              slot, name ? name : "unknown", context ? context : "unknown");
    } else {
        LOG_I(TAG, "AUDIT: TOTP GENERATE FAIL | slot=%d | name=%s | context=%s",
              slot, name ? name : "unknown", context ? context : "unknown");
    }
}
```

### Step 5: Integrate audit into password read (10 min)

Update `PasswordStore::readEntry()`:

```cpp
bool PasswordStore::readEntry(uint16_t slot, PasswordEntry* out) const {
    if (!out) {
        password_audit_entry_read(slot, NULL, false, "unknown");
        return false;
    }
    if (!hasSlotRange_) {
        password_audit_entry_read(slot, NULL, false, "unknown");
        return false;
    }
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) {
        // Get entry name for audit
        cdc::core::TropicStorage::instance().getName(moduleId_, physSlot, &entry_name);
        password_audit_entry_read(slot, entry_name, false, "unknown");
        return false;
    }

    // ... existing read logic ...

    if (res != cdc::hal::SeResult::OK) {
        password_audit_entry_read(slot, header.name, false, "unknown");
        return false;
    }

    // ... rest of function ...

    password_audit_entry_read(slot, out->title, true, "unknown");
    return true;
}
```

### Step 6: Integrate audit into TOTP code generation (10 min)

Update `TotpStore::generateCode()`:

```cpp
int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    if (!codeOut) {
        totp_audit_code_generated(slot, NULL, false, "unknown");
        return -1;
    }

    TotpAccount account = {};
    if (!readAccount(slot, &account)) {
        // Try to get account name for audit
        totp_audit_code_generated(slot, NULL, false, "unknown");
        return -1;
    }

    if (!isTimeValid()) {
        strcpy(codeOut, "------");
        totp_audit_code_generated(slot, account.name, false, "unknown");
        return -1;
    }

    uint32_t code = generate(account.secret, account.secretLen, time(nullptr),
                             account.period, account.digits,
                             static_cast<TotpAlgorithm>(account.algorithm));

    if (account.digits == 8) {
        snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));
    } else if (account.digits == 7) {
        snprintf(codeOut, 8, "%07lu", static_cast<unsigned long>(code));
    } else {
        snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));
    }

    totp_audit_code_generated(slot, account.name, true, "unknown");
    return static_cast<int8_t>(timeRemaining(account.period));
}
```

## References

- OWASP Password Storage Cheat Sheet - recommends audit trails for password access
- NIST SP 800-63B (Section 5.1.1.2 Memorized Secret Verifiers)
- ISO/IEC 27001:2022 (A.12.4 Logging and Event Correlation)
- Related finding: `003-missing-totp-password-audit.md` covers CRUD operations, this covers read access

</content>