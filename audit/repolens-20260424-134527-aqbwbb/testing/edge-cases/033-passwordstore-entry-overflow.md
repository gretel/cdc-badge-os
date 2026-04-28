---
title: "[MEDIUM] PasswordStore entry size calculation doesn't account for null terminator in notes"
severity: MEDIUM
domain: mod_password/PasswordStore
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `PasswordStore.h` (file: `components/mod_password/include/mod_password/PasswordStore.h:14-18`), the `PASSWORD_NOTES_LEN` calculation assumes the fixed payload fits exactly, but doesn't account for the null terminator of the notes string.

Lines 14-18:
```cpp
constexpr size_t PASSWORD_PAYLOAD_MAX =
    cdc::hal::ISecureElement::RMEM_SLOT_SIZE - sizeof(cdc::hal::ISecureElement::RMemHeader);
constexpr size_t PASSWORD_FIXED_PAYLOAD =
    PASSWORD_TITLE_LEN + PASSWORD_USERNAME_LEN + PASSWORD_PASSWORD_LEN + PASSWORD_URL_LEN + sizeof(uint8_t);
constexpr size_t PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD;
static_assert(PASSWORD_NOTES_LEN > 0, "Password notes length must be positive");
```

The `PasswordEntry` struct at lines 21-28 defines `notes[PASSWORD_NOTES_LEN + 1]` with an extra byte for null terminator, but the calculation of `PASSWORD_NOTES_LEN` doesn't account for this properly.

## Impact
- **Buffer overflow**: If notes field is filled to `PASSWORD_NOTES_LEN`, the null terminator overflows
- **Data corruption**: Adjacent memory could be corrupted when storing notes
- **Security**: Potential for buffer overflow exploitation

## Evidence
File: `components/mod_password/include/mod_password/PasswordStore.h`, lines 9-28

```cpp
constexpr uint8_t PASSWORD_TITLE_LEN = 24;
constexpr uint8_t PASSWORD_USERNAME_LEN = 16;
constexpr uint8_t PASSWORD_PASSWORD_LEN = 64;
constexpr uint8_t PASSWORD_URL_LEN = 64;

constexpr size_t PASSWORD_PAYLOAD_MAX =
    cdc::hal::ISecureElement::RMEM_SLOT_SIZE - sizeof(cdc::hal::ISecureElement::RMemHeader);
constexpr size_t PASSWORD_FIXED_PAYLOAD =
    PASSWORD_TITLE_LEN + PASSWORD_USERNAME_LEN + PASSWORD_PASSWORD_LEN + PASSWORD_URL_LEN + sizeof(uint8_t);
constexpr size_t PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD;
static_assert(PASSWORD_NOTES_LEN > 0, "Password notes length must be positive");

struct PasswordEntry {
    char title[PASSWORD_TITLE_LEN + 1];
    char username[PASSWORD_USERNAME_LEN + 1];
    char password[PASSWORD_PASSWORD_LEN + 1];
    char url[PASSWORD_URL_LEN + 1];
    uint8_t totpSlot;
    char notes[PASSWORD_NOTES_LEN + 1];  // Extra byte for null terminator
};
```

The struct has `notes[PASSWORD_NOTES_LEN + 1]` for null terminator, but the calculation assumes `PASSWORD_NOTES_LEN` is the actual available space. If `PASSWORD_NOTES_LEN` is calculated as 100, the notes array is 101 bytes, but only 100 bytes are available in the payload.

## Recommended Fix
Adjust the calculation to account for the notes null terminator:

```cpp
constexpr size_t PASSWORD_PAYLOAD_MAX =
    cdc::hal::ISecureElement::RMEM_SLOT_SIZE - sizeof(cdc::hal::ISecureElement::RMemHeader);
constexpr size_t PASSWORD_FIXED_PAYLOAD =
    PASSWORD_TITLE_LEN + PASSWORD_USERNAME_LEN + PASSWORD_PASSWORD_LEN + PASSWORD_URL_LEN + sizeof(uint8_t);
// Subtract 1 for notes null terminator
constexpr size_t PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD - 1;
static_assert(PASSWORD_NOTES_LEN > 0, "Password notes length must be positive");
```

Alternatively, adjust the struct to not add +1 for notes:

```cpp
struct PasswordEntry {
    char title[PASSWORD_TITLE_LEN + 1];
    char username[PASSWORD_USERNAME_LEN + 1];
    char password[PASSWORD_PASSWORD_LEN + 1];
    char url[PASSWORD_URL_LEN + 1];
    uint8_t totpSlot;
    char notes[PASSWORD_NOTES_LEN];  // No +1, use calculated value directly
};
```

Also add validation in `addEntry()` and `updateEntry()` to check notes length:

```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // Validate notes length
    size_t notesLen = strlen(entry.notes);
    if (notesLen > NOTES_LEN) {
        // Handle overflow
        return false;
    }
    // ...
}
```

## References
- CWE-120: Buffer overflow
- CWE-131: Incorrect Calculation of Multi-Byte String Length
