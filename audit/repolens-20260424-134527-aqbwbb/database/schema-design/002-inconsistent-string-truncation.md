---
title: "[MEDIUM] Inconsistent string truncation in TOTP vs Password payload structures"
severity: MEDIUM
domain: database/schema-design
lens: data-consistency
labels:
  - "storage"
  - "string-handling"
  - "payload-format"
---

## Summary
The `TotpPayload` struct in `TotpStore.cpp:17` and `PasswordPayload` struct in `PasswordStore.cpp:14` handle string fields differently. TOTP uses fixed-size arrays with explicit length tracking, while Password uses fixed-size arrays without explicit length fields, leading to potential inconsistencies in data handling.

**Evidence:**
- `TotpStore.cpp:17-23`:
  ```cpp
  struct TotpPayload {
      char issuer[TotpStore::ISSUER_LEN];      // 32 bytes
      uint8_t secret[TotpStore::SECRET_LEN];   // 32 bytes
      uint8_t secretLen;                        // Explicit length!
      uint8_t digits;
      uint32_t period;
      uint8_t algorithm;
      uint8_t flags;
  };
  ```
- `PasswordStore.cpp:14-21`:
  ```cpp
  struct PasswordPayload {
      char title[PasswordStore::TITLE_LEN];     // 24 bytes, no length field
      char username[PasswordStore::USERNAME_LEN]; // 16 bytes, no length field
      char password[PasswordStore::PASSWORD_LEN]; // 64 bytes, no length field
      char url[PasswordStore::URL_LEN];         // 64 bytes, no length field
      uint8_t totpSlot;
      char notes[PasswordStore::NOTES_LEN];     // Variable, no length field
  };
  ```

## Impact
1. **Data inconsistency**: TOTP tracks variable-length secrets; Password assumes fixed-length fields
2. **Storage waste**: Password entries always use full field capacity even for short values
3. **Potential overflow risk**: String operations rely on null-termination which may not be guaranteed if exact field size is written
4. **Migration difficulty**: Schema changes would be harder with Password structure due to lack of length metadata

## Evidence
From `TotpStore.cpp:265-270`:
```cpp
memcpy(payload.secret, secret, static_cast<size_t>(secretLen));
payload.secretLen = static_cast<uint8_t>(secretLen);  // Explicit length tracking
```

From `PasswordStore.cpp:215-220`:
```cpp
copyText(payload.title, sizeof(payload.title), entry.title);  // Truncates but no length stored
copyText(payload.username, sizeof(payload.username), entry.username);
// ... all fields use copyText which null-terminates
```

## Recommended Fix
1. Add explicit length fields to `PasswordPayload` for variable-length fields or document that null-termination is always guaranteed
2. Consider using a common payload structure pattern across modules for consistency
3. Add validation that strings are always null-terminated on read
4. Document the maximum effective lengths (accounting for null terminator)

## References
- C++ struct packing best practices
- Embedded data serialization patterns
