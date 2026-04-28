---
title: "[MEDIUM] Inconsistent field naming across serial command responses"
severity: MEDIUM
domain: api-design
lens: response-consistency
labels:
  - "audit:api-design/response-consistency"
  - "serial-commands"
  - "data-format"
  - "mod_gpg"
  - "mod_password"
  - "mod_totp"
---

## Summary

Field names in serial command responses use inconsistent naming patterns, making it harder to parse responses programmatically and creating a disjointed user experience.

**Locations:**
- `components/mod_gpg/src/GpgModule.cpp` - Lines 139-143, 401-402
- `components/mod_password/src/PasswordModule.cpp` - Lines 236-243
- `components/mod_totp/src/TotpModule.cpp` - Lines 208, 277
- `components/serial_cmd/src/SerialCmd.cpp` - Various lines

**Field naming inconsistencies found:**

| Concept | GPG Module | Password Module | TOTP Module | Main Serial |
|---------|------------|-----------------|-------------|-------------|
| User identifier | `User-ID:` | `Username:` | `name:` (in list) | `User:` |
| Timestamp | `Created:` | N/A | N/A | Various |
| Counter | `Sign Count:` | N/A | N/A | N/A |
| Storage location | N/A | `slot` (lowercase) | `slot` (lowercase) | `Slot` |
| Name field | N/A | `Title:` | `name:` | Varies |
| Curve/type | `Curve:` | N/A | N/A | N/A |
| Identifier | `Fingerprint:` | N/A | N/A | N/A |

## Impact

**Parser complexity:** External tools must handle different field name conventions:
- `User-ID:` vs `Username:` vs `name:` for similar concepts
- `slot` vs `Slot` for storage location
- `Sign Count:` vs `count` (if it existed)

**User experience:** Inconsistent terminology makes the CLI feel less polished:
- GPG uses `User-ID:` (hyphenated, capitalized)
- Password uses `Username:` (camelCase, capitalized)
- TOTP list uses `name:` (lowercase in format string)

**Maintenance burden:** New commands may continue inconsistent patterns.

## Evidence

**GPG module (lines 139-143, 401-402):**
```cpp
// Line 139 - User-ID with hyphen
cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);

// Line 140 - Curve capitalized
cdc::serial::Console::printf("Curve: %s\r\n", ...);

// Line 143 - Sign Count with space
cdc::serial::Console::printf("Sign Count: %lu\r\n", ...);

// Line 401 - Mixed format in multi-line output
snprintf(detail, sizeof(detail),
         "User-ID: %s\nCurve: %s\nFingerprint: %s\nCreated: %lu\nSign Count: %lu",
         status.user_id, curveName, fp_hex, ...);
```

**Password module (lines 236-243):**
```cpp
// Line 236 - Title capitalized
cdc::serial::Console::printf("Title: %s\r\n", entry.title);

// Line 237 - Username (camelCase)
cdc::serial::Console::printf("Username: %s\r\n", entry.username);

// Line 241-243 - TOTP Slot (two words, both capitalized)
cdc::serial::Console::printf("TOTP Slot: none\r\n");
cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);

// Line 210 - slot lowercase in list
cdc::serial::Console::printf("%u: %s (slot %u)\r\n", i, list[i].title, list[i].slot);
```

**TOTP module (lines 208, 277):**
```cpp
// Line 208 - name lowercase, slot lowercase
cdc::serial::Console::printf("%u: %s (slot %u)\r\n", c->idx, entry.name, logical);

// Line 277 - index lowercase
cdc::serial::Console::printf("ERROR: index not found\r\n");
```

**Comparison with main serial commands:**
```cpp
// Various formats across commands:
Console::printf("OK: Time set to %02d:%02d:%02d\r\n", h, m, s);  // Time capitalized
Console::printf("OK: Name set to \"%s\"\r\n", args);              // Name capitalized
Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);    // slotTypeName lowercase
```

**Inconsistency patterns:**

1. **Hyphenation:**
   - `User-ID:` (GPG) vs `Username:` (Password) vs `name:` (TOTP)
   - Should be: `UserID:`, `Username:`, or `Name:` (consistent)

2. **Capitalization:**
   - `slot` (lowercase in list) vs `Slot` (capitalized in detail)
   - `Sign Count:` (space-separated) vs `SignCount:` (camelCase)

3. **Naming style:**
   - `Fingerprint:` (single word)
   - `Sign Count:` (two words)
   - `TOTP Slot:` (two words with prefix)

## Recommended Fix

### Establish field naming conventions

**Recommended standard:**
1. **Use PascalCase for all field names** (first letter of each word capitalized, no hyphens)
2. **Use consistent terminology** across modules
3. **Document the convention** in a shared header

**Updated field names:**

| Current | Proposed | Module |
|---------|----------|--------|
| `User-ID:` | `UserId:` | GPG |
| `Username:` | `Username:` (keep) | Password |
| `name:` | `Name:` | TOTP |
| `slot` | `Slot:` | All |
| `Sign Count:` | `SignCount:` | GPG |
| `TOTP Slot:` | `TotpSlot:` | Password |
| `Fingerprint:` | `Fingerprint:` (keep) | GPG |
| `Created:` | `CreatedAt:` | GPG |
| `Curve:` | `Curve:` (keep) | GPG |

### Apply to GPG module (lines 139-143, 401-402):

```cpp
// Line 139 - Use UserId (no hyphen)
cdc::serial::Console::printf("UserId: %s\r\n", status.user_id);

// Line 140 - Keep Curve
cdc::serial::Console::printf("Curve: %s\r\n", ...);

// Line 143 - Use SignCount (no space)
cdc::serial::Console::printf("SignCount: %lu\r\n", ...);

// Line 143 - Use CreatedAt (more explicit)
cdc::serial::Console::printf("CreatedAt: %lu\r\n", ...);

// Line 401 - Update multi-line output
snprintf(detail, sizeof(detail),
         "UserId: %s\nCurve: %s\nFingerprint: %s\nCreatedAt: %lu\nSignCount: %lu",
         status.user_id, curveName, fp_hex, ...);
```

### Apply to Password module (lines 210, 236-243):

```cpp
// Line 210 - Use Slot: (capitalized with colon)
cdc::serial::Console::printf("%u: %s (Slot: %u)\r\n", i, list[i].title, list[i].slot);

// Line 236-237 - Keep Title, Username
cdc::serial::Console::printf("Title: %s\r\n", entry.title);
cdc::serial::Console::printf("Username: %s\r\n", entry.username);

// Line 241-243 - Use TotpSlot (camelCase)
cdc::serial::Console::printf("TotpSlot: none\r\n");
cdc::serial::Console::printf("TotpSlot: %u\r\n", entry.totpSlot);
```

### Apply to TOTP module (lines 208, 277):

```cpp
// Line 208 - Use Name: (capitalized)
cdc::serial::Console::printf("%u: %s (Slot: %u)\r\n", c->idx, entry.name, logical);

// Line 277 - Use Index (capitalized)
cdc::serial::Console::printf("ERROR: Index not found\r\n");
```

### Create helper macros (optional):

```cpp
// In a shared header
#define FIELD(name, value) cdc::serial::Console::printf("%s: %s\r\n", name, value)
#define FIELD_INT(name, value) cdc::serial::Console::printf("%s: %lu\r\n", name, (unsigned long)value)

// Usage:
FIELD("UserId", status.user_id);
FIELD_INT("SignCount", status.sign_count);
```

## References

- Existing findings: `001-inconsistent-success-prefixes.md`, `004-inconsistent-nvs-output.md`
- JSON naming conventions (camelCase vs PascalCase)
- REST API field naming best practices
