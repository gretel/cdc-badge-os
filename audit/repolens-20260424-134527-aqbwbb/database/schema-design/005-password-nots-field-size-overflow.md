---
title: "[MEDIUM] Password notes field calculation may cause struct size mismatch"
severity: MEDIUM
domain: database/schema-design
lens: struct-layout
labels:
  - "storage"
  - "struct-size"
  - "password"
---

## Summary
The `PasswordStore` calculates the notes field size as a remainder: `PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD`. This calculation depends on exact field sizes and could result in a negative value or incorrect size if the fixed payload calculation changes. The `static_assert` helps catch this, but the margin is tight.

**Evidence:**
- `PasswordStore.h:14-18`:
  ```cpp
  constexpr size_t PASSWORD_PAYLOAD_MAX =
      cdc::hal::ISecureElement::RMEM_SLOT_SIZE - sizeof(cdc::hal::ISecureElement::RMemHeader);
  constexpr size_t PASSWORD_FIXED_PAYLOAD =
      PASSWORD_TITLE_LEN + PASSWORD_USERNAME_LEN + PASSWORD_PASSWORD_LEN + PASSWORD_URL_LEN + sizeof(uint8_t);
  constexpr size_t PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD;
  ```
- `ISecureElement.h:46`: `static constexpr uint16_t RMEM_SLOT_SIZE = 476;`
- `PasswordStore.h:9-12`:
  ```cpp
  constexpr uint8_t PASSWORD_TITLE_LEN = 24;
  constexpr uint8_t PASSWORD_USERNAME_LEN = 16;
  constexpr uint8_t PASSWORD_PASSWORD_LEN = 64;
  constexpr uint8_t PASSWORD_URL_LEN = 64;
  ```

**Calculation check**:
- `RMEM_SLOT_SIZE` = 476
- `RMemHeader` size = 1 + 1 + 1 + 1 + 16 + 2 = 22 bytes
- `PASSWORD_PAYLOAD_MAX` = 476 - 22 = 454 bytes
- `PASSWORD_FIXED_PAYLOAD` = 24 + 16 + 64 + 64 + 1 = 169 bytes
- `PASSWORD_NOTES_LEN` = 454 - 169 = 285 bytes

This works, but the `__attribute__((packed))` on the struct and exact alignment could cause issues.

## Impact
1. **Fragile calculation**: Any change to field sizes could break the notes field
2. **Struct packing issues**: `#pragma pack(push, 1)` is used but not always visible at definition site
3. **Hard to debug**: If the static_assert fails, the error message may not be clear

## Evidence
From `PasswordStore.cpp:14-22`:
```cpp
#pragma pack(push, 1)
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];      // 24
    char username[PasswordStore::USERNAME_LEN]; // 16
    char password[PasswordStore::PASSWORD_LEN]; // 64
    char url[PasswordStore::URL_LEN];          // 64
    uint8_t totpSlot;                          // 1
    char notes[PasswordStore::NOTES_LEN];      // 285
};
#pragma pack(pop)

static_assert(sizeof(PasswordPayload) == PasswordStore::PAYLOAD_MAX, "Password payload size mismatch");
```

## Recommended Fix
1. **Add explicit size verification** at compile time with more descriptive error messages
2. **Document the calculation** with comments showing the math
3. **Consider using `sizeof()` directly** instead of constexpr arithmetic for clarity
4. **Add a test** that verifies the struct size matches expected payload size

Example improvement:
```cpp
// Total R-Memory slot: 476 bytes
// Header: 22 bytes (magic 1 + checksum 1 + moduleId 1 + flags 1 + name 16 + payloadLen 2)
// Available for payload: 454 bytes
// Fixed fields: title(24) + username(16) + password(64) + url(64) + totpSlot(1) = 169 bytes
// Remaining for notes: 454 - 169 = 285 bytes
constexpr size_t PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD;
static_assert(PASSWORD_NOTES_LEN >= 100, "Notes field too small for practical use");
```

## References
- C++ struct packing and alignment
- Embedded data structure design patterns
