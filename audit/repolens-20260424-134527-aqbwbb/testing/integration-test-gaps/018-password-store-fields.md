---
title: "[MEDIUM] Password store text field handling lacks integration tests for boundaries"
severity: MEDIUM
domain: modules
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:mod_password"
  - "area:password"
---

## Summary
The `PasswordStore` component (`components/mod_password/src/PasswordStore.cpp`) stores password entries with various text fields (title, username, password, URL, notes), but **no integration tests** verify correct handling of field boundaries, null termination, and special characters.

## Impact
- **Buffer overflow**: Long text may overflow fixed-size fields
- **Null termination**: Strings may not be null-terminated
- **Special characters**: Unicode, spaces, special chars may corrupt storage
- **Field truncation**: Truncation may not preserve valid strings

## Evidence

**Password payload structure** (`components/mod_password/src/PasswordStore.cpp:14-25`):
```cpp
#pragma pack(push, 1)
struct PasswordPayload {
    char title[TITLE_LEN];        // 64 bytes
    char username[USERNAME_LEN];  // 64 bytes
    char password[PASSWORD_LEN];  // 128 bytes
    char url[URL_LEN];            // 128 bytes
    uint8_t totpSlot;
    char notes[NOTES_LEN];        // 256 bytes
};
#pragma pack(pop)

static_assert(sizeof(PasswordPayload) == PasswordStore::PAYLOAD_MAX, "...");
```

**Text copying helper** (`components/mod_password/src/PasswordStore.cpp:30-42`):
```cpp
static void copyText(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}
```

**Storage operations** (`components/mod_password/src/PasswordStore.cpp:80-200`):
```cpp
bool PasswordStore::addEntry(const char* title, const char* username,
                             const char* password, const char* url,
                             const char* notes, uint8_t totpSlot) {
    PasswordPayload payload;
    copyText(payload.title, TITLE_LEN, title);
    copyText(payload.username, USERNAME_LEN, username);
    copyText(payload.password, PASSWORD_LEN, password);
    copyText(payload.url, URL_LEN, url);
    copyText(payload.notes, NOTES_LEN, notes);
    payload.totpSlot = totpSlot;
    
    se->rmemWrite(slot, &payload, sizeof(PasswordPayload));
}

bool PasswordStore::getEntry(uint16_t index, PasswordEntry* entry) {
    se->rmemRead(slot, &payload, sizeof(PasswordPayload));
    // Copy fields to entry
}
```

**Field sizes** (`components/mod_password/include/mod_password/PasswordStore.h`):
```cpp
static constexpr uint8_t TITLE_LEN = 64;
static constexpr uint8_t USERNAME_LEN = 64;
static constexpr uint8_t PASSWORD_LEN = 128;
static constexpr uint8_t URL_LEN = 128;
static constexpr uint8_t NOTES_LEN = 256;
```

**Usage in PasswordModule** (`components/mod_password/src/PasswordModule.cpp:100-400`):
```cpp
// Module collects text input from T9InputView
T9InputView::setOnSave([](const char* text) {
    PasswordStore::addEntry(title, username, password, url, notes, totpSlot);
});
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_password_store/` that verifies:

1. **Field boundaries**: Text truncated correctly at field limits
2. **Null termination**: All strings null-terminated
3. **Empty strings**: Empty strings handled correctly
4. **Special characters**: Spaces, punctuation stored correctly
5. **Storage round-trip**: Write and read preserve data
6. **TOTP slot**: Slot reference stored and retrieved

**Test structure** (example):
```cpp
// test/test_password_store/test_fields.cpp
#include "mod_password/PasswordStore.h"

void test_field_truncation() {
    // Title is 64 bytes, provide 100
    const char* longTitle = "A very long title that exceeds 64 characters...";
    
    PasswordEntry entry;
    PasswordStore::addEntry(longTitle, "user", "pass", "url", "notes", 0);
    PasswordStore::getEntry(0, &entry);
    
    // Should be truncated to 63 chars + null
    ASSERT_LE(strlen(entry.title), 63);
}

void test_null_termination() {
    const char* title = "Test";
    
    PasswordStore::addEntry(title, "user", "pass", "url", "notes", 0);
    
    PasswordEntry entry;
    PasswordStore::getEntry(0, &entry);
    
    // Verify null termination
    ASSERT_EQ(entry.title[strlen(entry.title)], '\0');
}

void test_special_characters() {
    const char* title = "Test with spaces & symbols!";
    const char* url = "https://example.com/path?foo=bar&baz=qux";
    
    PasswordStore::addEntry(title, "user", "pass", url, "notes", 0);
    
    PasswordEntry entry;
    PasswordStore::getEntry(0, &entry);
    
    ASSERT_STREQ(entry.title, title);
    ASSERT_STREQ(entry.url, url);
}

void test_empty_fields() {
    PasswordStore::addEntry("Title", "", "pass", "", "", 0);
    
    PasswordEntry entry;
    PasswordStore::getEntry(0, &entry);
    
    ASSERT_STREQ(entry.username, "");
    ASSERT_STREQ(entry.url, "");
}
```

## References
- [PasswordStore implementation](components/mod_password/src/PasswordStore.cpp)
- [Password module](components/mod_password/src/PasswordModule.cpp)
- [T9InputView integration](components/cdc_views/src/T9InputView.cpp)

</content>