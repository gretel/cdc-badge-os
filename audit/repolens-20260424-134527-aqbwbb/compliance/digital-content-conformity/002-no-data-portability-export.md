---
title: "[MEDIUM] No data export/portability for user-generated content (TOTP, passwords)"
severity: MEDIUM
domain: digital-content-conformity
lens: EU-2019-770
labels:
  - "data-portability"
---

## Summary
The CDC Badge OS stores user-generated content (TOTP accounts, password vault entries) in TROPIC01 secure element R-Memory slots, but provides no mechanism to export this data for backup or migration purposes. Under EU Directive 2021/770, consumers have rights to data portability for digital content they have "purchased" or subscribed to.

**Files affected:**
- `components/mod_totp/` - TOTP authenticator module
- `components/mod_password/` - Password vault module
- `components/mod_gpg/src/GpgModule.cpp:124` - GPG export exists, but TOTP/passwords lack equivalent

## Impact
1. **Vendor lock-in risk:** Users cannot easily migrate their TOTP accounts or password data to another device
2. **No backup mechanism:** If device is lost/damaged, user data is irrecoverable (keys stored in secure element cannot be extracted)
3. **Limited interoperability:** No CSV/standard format export for TOTP (like Google Authenticator's `.txt` export)

## Evidence
1. **GPG has export but TOTP/Passwords do not:**
   - `components/mod_gpg/src/GpgModule.cpp:124` - `registry.registerCommand({"GPG_EXPORT", "Export public keys", ...})`
   - `components/mod_gpg/src/gpg.cpp:456` - `gpg_export_pubkey_pem()` function exists
   - No equivalent `totp_export` or `password_export` functions found

2. **Search results confirm no export functionality:**
   - `grep -rn 'export\|import\|backup\|restore' components/mod_totp/` - No matches
   - `grep -rn 'export\|import\|backup\|restore' components/mod_password/` - No matches

3. **Data storage architecture:**
   - TOTP accounts stored in R-Memory slots 32-131 (100 accounts)
   - Password vault in R-Memory slots 150-511 (353 entries)
   - Data encrypted but no export format defined

## Recommended Fix
Add data export functionality for TOTP and password modules:

1. **TOTP Export (1 hour scope):**
   - Create `totp_export_csv()` function in `components/mod_totp/src/`
   - Export format: `account_name,issuer,secret_key,algorithm,digits,period` (standard TOTP format)
   - Add serial command `TOTP_EXPORT` that outputs CSV to serial
   - Add UI menu option to export via serial

2. **Password Export (1 hour scope):**
   - Create `password_export_json()` function in `components/mod_password/src/`
   - Export format: JSON with encrypted payloads
   - Add serial command `PW_EXPORT` with authentication requirement
   - Document that export requires PIN authentication

3. **Documentation:**
   - Add export instructions to `docs/SERIAL_COMMANDS.md`
   - Add backup best practices to README

## References
- EU Directive 2019/770 Article 12 (Data portability)
- RFC 6238 (TOTP standard format)
- Google Authenticator export format (for compatibility)
