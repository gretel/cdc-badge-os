---
title: "[MEDIUM] Password Store CRUD Operations Lack Unit Test Coverage"
severity: MEDIUM
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `PasswordStore` class (`components/mod_password/src/PasswordStore.cpp`) implements password vault storage with no unit tests. Critical untested functions typically include:

- `init()` - Storage initialization
- `addPassword()` - Add new password entry
- `getPassword()` - Retrieve password by index/name
- `updatePassword()` - Update existing entry
- `deletePassword()` - Remove entry
- `getCount()` - Entry count
- `encryptEntry()` - Entry encryption
- `decryptEntry()` - Entry decryption

## Impact
**Password Vault Risk:** PasswordStore manages sensitive credentials:
1. CRUD operations (add/get/update/delete) are untested
2. Encryption/decryption roundtrip is unproven
3. Search by name/username is untested
4. Storage limits and overflow handling is untested
5. Entry format serialization is unverified

## Evidence
File: `components/mod_password/src/PasswordStore.cpp` (estimated 150-200 lines)

Typical password store operations:
- Store in TROPIC01 R-Memory slots (150-511)
- Entry format: name, username, password, notes
- Encryption with module-specific key

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l -i "password" {} \;
# Returns nothing - no Password store tests exist
```

## Recommended Fix
Create `test/test_password_store/test_password_store.cpp` with test cases:

1. **CRUD tests:**
   - Test `addPassword()` returns new entry index
   - Test `getPassword()` retrieves entry by index
   - Test `updatePassword()` modifies entry
   - Test `deletePassword()` removes entry
   - Test `getCount()` reflects additions/deletions

2. **Search tests:**
   - Test search by name returns matching entries
   - Test search by username returns matching entries

3. **Encryption tests:**
   - Test `encryptEntry()` and `decryptEntry()` roundtrip
   - Test encrypted data differs from plaintext

4. **Edge case tests:**
   - Test `addPassword()` at max capacity
   - Test `getPassword()` with invalid index
   - Test `deletePassword()` on empty store

Example test:
```cpp
void test_addPassword_returns_index() {
    auto& store = PasswordStore::instance();
    store.init();
    
    PasswordEntry entry = {"github", "user", "secret123", "GitHub password"};
    uint16_t index = store.addPassword(entry);
    
    TEST_ASSERT_GREATER_THAN(0, index);
    TEST_ASSERT_EQUAL(1, store.getCount());
}

void test_getPassword_roundtrip() {
    auto& store = PasswordStore::instance();
    store.init();
    
    PasswordEntry original = {"github", "user", "secret123", "GitHub"};
    uint16_t index = store.addPassword(original);
    
    PasswordEntry retrieved = store.getPassword(index);
    TEST_ASSERT_EQUAL_STRING("github", retrieved.name);
    TEST_ASSERT_EQUAL_STRING("user", retrieved.username);
    TEST_ASSERT_EQUAL_STRING("secret123", retrieved.password);
}

void test_updatePassword_modifies_entry() {
    auto& store = PasswordStore::instance();
    store.init();
    
    PasswordEntry entry = {"github", "user", "secret123", "GitHub"};
    uint16_t index = store.addPassword(entry);
    
    PasswordEntry updated = {"github", "user", "newpass456", "GitHub updated"};
    store.updatePassword(index, updated);
    
    PasswordEntry retrieved = store.getPassword(index);
    TEST_ASSERT_EQUAL_STRING("newpass456", retrieved.password);
}

void test_deletePassword_removes_entry() {
    auto& store = PasswordStore::instance();
    store.init();
    
    PasswordEntry entry = {"github", "user", "secret123", "GitHub"};
    uint16_t index = store.addPassword(entry);
    TEST_ASSERT_EQUAL(1, store.getCount());
    
    store.deletePassword(index);
    TEST_ASSERT_EQUAL(0, store.getCount());
}
```

## References
- File: `components/mod_password/include/mod_password/PasswordStore.h` - Store API
- File: `components/mod_password/include/mod_password/PasswordModule.h` - Module API
