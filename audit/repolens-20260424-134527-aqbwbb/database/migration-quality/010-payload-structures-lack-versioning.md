---
title: "[MEDIUM] TOTP and Password payload structures lack schema versioning"
severity: MEDIUM
domain: database/migration-quality
lens: embedded-storage
labels:
  - "schema-evolution"
  - "totp"
  - "password"
---

## Summary
Both the TOTP and Password payload structures stored in R-Memory lack version fields, making future schema changes difficult:

**TOTP Payload** (`components/mod_totp/src/TotpStore.cpp:14-23`):
```cpp
struct TotpPayload {
    char issuer[TotpStore::ISSUER_LEN];
    uint8_t secret[TotpStore::SECRET_LEN];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};
```

**Password Payload** (`components/mod_password/src/PasswordStore.cpp:13-21`):
```cpp
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];
    char username[PasswordStore::PASSWORD_USERNAME_LEN];
    char password[PasswordStore::PASSWORD_PASSWORD_LEN];
    char url[PasswordStore::PASSWORD_URL_LEN];
    uint8_t totpSlot;
    char notes[PasswordStore::NOTES_LEN];
};
```

Neither structure has a version field, magic number, or checksum. If either structure needs to be extended (e.g., adding a new field for TOTP time-sync or password expiration), all existing data must be re-created.

## Impact
- **TOTP module**: Adding features like time-sync offset, backup codes, or extended algorithms requires recreating all TOTP accounts.
- **Password module**: Adding fields like username case-sensitivity, password categories, or custom fields requires recreating all password entries.
- **No forward compatibility**: New firmware versions cannot gracefully handle old data formats.
- **User disruption**: Any schema change forces users to re-enter all their credentials.

## Evidence
File: `components/mod_totp/src/TotpStore.cpp:14-23`

The TOTP payload is written in `addAccount()` (line 236-291):
```cpp
TotpPayload payload = {};
if (issuer) {
    strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
}
memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
payload.secretLen = static_cast<uint8_t>(secretLen);
payload.digits = digits ? digits : DEFAULT_DIGITS;
payload.period = period ? period : DEFAULT_PERIOD;
payload.algorithm = algorithm;
payload.flags = 0;

auto res = se->rmemWriteWithHeader(slot, moduleId_, name, 0,
                                   reinterpret_cast<const uint8_t*>(&payload),
                                   sizeof(payload));
```

File: `components/mod_password/src/PasswordStore.cpp:13-21`

The Password payload is written in `addEntry()` (line 207-250):
```cpp
PasswordPayload payload = {};
copyText(payload.title, sizeof(payload.title), entry.title);
copyText(payload.username, sizeof(payload.username), entry.username);
copyText(payload.password, sizeof(payload.password), entry.password);
copyText(payload.url, sizeof(payload.url), entry.url);
payload.totpSlot = entry.totpSlot;
copyText(payload.notes, sizeof(payload.notes), entry.notes);

auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0,
                                   reinterpret_cast<const uint8_t*>(&payload),
                                   sizeof(payload));
```

No version field is written or read in either case.

## Recommended Fix
Add versioning to both structures:

1. **TOTP Payload with version**:
   ```cpp
   struct TotpPayload {
       uint8_t version;        // 0x01 = current format
       char issuer[TotpStore::ISSUER_LEN];
       uint8_t secret[TotpStore::SECRET_LEN];
       uint8_t secretLen;
       uint8_t digits;
       uint32_t period;
       uint8_t algorithm;
       uint8_t flags;
   };
   ```

2. **Password Payload with version**:
   ```cpp
   struct PasswordPayload {
       uint8_t version;        // 0x01 = current format
       char title[PasswordStore::TITLE_LEN];
       char username[PasswordStore::USERNAME_LEN];
       char password[PasswordStore::PASSWORD_LEN];
       char url[PasswordStore::URL_LEN];
       uint8_t totpSlot;
       char notes[PasswordStore::NOTES_LEN];
   };
   ```

3. **Update write functions**:
   ```cpp
   payload.version = 0x01;  // Current version
   ```

4. **Update read functions with migration**:
   ```cpp
   bool TotpStore::readAccount(uint16_t slot, TotpAccount* out) {
       // ... read data ...
       if (payload.version == 0x01 || payload.version == 0x00) {
           // Handle both old (no version) and new formats
           // For version 0x00, fields are at same offset
           // For version 0x01, account for version byte
           // ...
       }
   }
   ```

5. **Add version constants**:
   ```cpp
   constexpr uint8_t TOTP_PAYLOAD_V1 = 0x01;
   constexpr uint8_t PASSWORD_PAYLOAD_V1 = 0x01;
   ```

## References
- Schema evolution: https://martinfowler.com/articles/schema-evolution.html
- Embedded data formats: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/storage/nvs.html
