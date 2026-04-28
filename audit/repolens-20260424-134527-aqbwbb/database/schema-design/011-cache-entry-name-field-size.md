---
title: "[LOW] Cache entry name field truncation may cause data inconsistency"
severity: LOW
domain: database/schema-design
lens: string-handling
labels:
  - "storage"
  - "nvs-cache"
  - "string-truncation"
---

## Summary
The `CacheEntry::name` field in `TropicStorage.h` uses `RMEM_NAME_LEN` (16 bytes) but when writing entries, names longer than 16 chars are silently truncated. This can cause confusion when the displayed name differs from the actual slot name.

**Evidence:**
- `ISecureElement.h:47`: `static constexpr uint8_t RMEM_NAME_LEN = 16;`
- `TropicStorage.h:14`: `char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];`
- `TropicStorage.cpp:173-176`:
  ```cpp
  if (name) {
      strncpy(entry.name, name, sizeof(entry.name) - 1);
      entry.name[sizeof(entry.name) - 1] = '\0';
  }
  ```
  Silent truncation for names > 16 chars!

## Impact
1. **Name mismatch**: Display shows truncated name, actual slot has longer name
2. **User confusion**: "google.com" vs "google_account_2024" (truncated to "google_acc")
3. **Debug difficulty**: Hard to identify which account is which

## Evidence
From `PasswordStore.h:9`:
```cpp
constexpr uint8_t PASSWORD_TITLE_LEN = 24;  // Password titles can be 24 chars
```

From `TotpStore.h:28`:
```cpp
static constexpr uint8_t NAME_LEN = 16;  // TOTP names are 16 chars
```

But `PasswordStore.cpp:220-224`:
```cpp
char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
if (entry.title[0]) {
    copyText(headerName, sizeof(headerName), entry.title);  // 24 char title -> 16 char header!
}
```

## Recommended Fix
1. **Increase `RMEM_NAME_LEN`** to at least 24 to accommodate password titles
2. **Or document the limit** clearly and enforce it at the UI level
3. **Add validation** with warning when truncation occurs:
   ```cpp
   if (strlen(name) > sizeof(entry.name) - 1) {
       LOG_W(TAG, "Name truncated from %zu to %zu chars", strlen(name), sizeof(entry.name) - 1);
   }
   ```

## References
- String handling best practices
- UI/UX for truncated display names
