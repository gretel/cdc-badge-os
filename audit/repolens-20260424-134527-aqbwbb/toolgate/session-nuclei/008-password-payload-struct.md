---
title: "[LOW] Password payload struct may have padding issues"
severity: LOW
domain: code-quality
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

In `components/mod_password/src/PasswordStore.cpp`, the `PasswordPayload` struct uses `#pragma pack(push, 1)` to ensure no padding, but the static_assert only verifies the total size matches, not that individual fields are in the expected order.

```cpp
#pragma pack(push, 1)
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];
    char username[PasswordStore::USERNAME_LEN];
    char password[PasswordStore::PASSWORD_LEN];
    char url[PasswordStore::URL_LEN];
    uint8_t totpSlot;
    char notes[PasswordStore::NOTES_LEN];
};
#pragma pack(pop)

static_assert(sizeof(PasswordPayload) == PasswordStore::PAYLOAD_MAX, "Password payload size mismatch");
```

## Impact

- **Portability**: The pack pragma is compiler-specific (though supported by GCC/Clang).
- **Maintenance**: If field order changes, the payload size might still match but data layout could differ.
- **Interoperability**: If this format needs to be read by other tools, the exact field order matters.

## Evidence

**File**: `components/mod_password/src/PasswordStore.cpp` (lines 15-26)

```cpp
#pragma pack(push, 1)
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];
    char username[PasswordStore::USERNAME_LEN];
    char password[PasswordStore::PASSWORD_LEN];
    char url[PasswordStore::URL_LEN];
    uint8_t totpSlot;
    char notes[PasswordStore::NOTES_LEN];
};
#pragma pack(pop)

static_assert(sizeof(PasswordPayload) == PasswordStore::PAYLOAD_MAX, "Password payload size mismatch");
```

**File**: `components/mod_password/include/mod_password/PasswordStore.h` (need to check field sizes)

## Recommended Fix

**Option 1: Add field-by-field assertions**
```cpp
static_assert(offsetof(PasswordPayload, title) == 0, "title offset mismatch");
static_assert(offsetof(PasswordPayload, username) == PasswordStore::TITLE_LEN, "username offset mismatch");
// etc.
```

**Option 2: Add version byte**
Add a version byte to the payload to detect format changes:

```cpp
#pragma pack(push, 1)
struct PasswordPayload {
    uint8_t version;
    char title[PasswordStore::TITLE_LEN - 1];  // Adjust for version byte
    // ...
};
#pragma pack(pop)
```

**Option 3: Document the format**
Add clear documentation about the binary layout:

```cpp
/**
 * \brief Password entry payload stored in TROPIC01 R-Memory.
 * 
 * Layout (total 145 bytes):
 * - title:   0-29   (30 bytes)
 * - username: 30-59 (30 bytes)
 * - password: 60-119 (60 bytes)
 * - url:     120-134 (15 bytes)
 * - totpSlot: 135   (1 byte)
 * - notes:   136-144 (9 bytes)
 */
```

## References

- C struct packing and alignment
- ESP32 memory layout
- Binary format versioning
