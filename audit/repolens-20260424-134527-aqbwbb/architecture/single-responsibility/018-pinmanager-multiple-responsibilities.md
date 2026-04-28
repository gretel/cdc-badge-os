---
title: "[MEDIUM] PinManager combines PIN validation, KDF computation, storage loading, and salt management"
severity: MEDIUM
domain: cdc_core
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
  - "component:cdc_core"
---

## Summary
`components/cdc_core/src/PinManager.cpp` (663 lines) combines four distinct responsibilities:
1. **PIN validation** - `verifyBadgePin()`, `verifyPw1()`, `verifyPw3()`
2. **KDF computation** - `computeKdfHash()`, salt generation, hash computation
3. **Storage loading** - `loadFromStorage()`, `saveToStorage()` from TROPIC01 R-Memory
4. **Salt management** - `generateSalt()`, KDF salt storage

Key code evidence:
- Lines 24-45: Singleton init with PIN state loading
- Lines 48-70: `loadDefaults()` with KDF computation
- Lines 72-85: `generateSalt()` with RNG fallback
- Lines 96-150: `loadFromStorage()` with TROPIC01 R-Memory access
- Lines 200-300: `verifyBadgePin()` with hash comparison
- Lines 300-400: `saveToStorage()` with TROPIC01 write

## Impact
**Testability**: Testing PIN validation requires TROPIC01 hardware or complex mocking.

**Performance**: KDF computation blocks UI thread because it's embedded in validation.

**Flexibility**: Changing PIN algorithm requires modifying validation, storage, and KDF code together.

**Memory**: PIN hashes stored in class members (128 bytes) cannot be tuned independently.

## Evidence
File: `components/cdc_core/src/PinManager.cpp`

Lines 24-45 (Initialization):
```cpp
PinManager& PinManager::instance() {
    static PinManager instance;
    return instance;
}

bool PinManager::init() {
    if (pinLoaded_) return true;
    if (!loadFromStorage()) {
        loadDefaults();
    }
    pinLoaded_ = true;
    return true;
}
```

Lines 48-70 (KDF computation):
```cpp
void PinManager::loadDefaults() {
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    generateSalt(pw1Salt_);
    generateSalt(pw3Salt_);
    computeKdfHash(DEFAULT_PW1, pw1Salt_, pw1Hash_);
    computeKdfHash(DEFAULT_PW3, pw3Salt_, pw3Hash_);
}
```

Lines 72-85 (Salt generation):
```cpp
void PinManager::generateSalt(uint8_t* salt) {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (se && se->isSessionActive() && se->getRandom(salt, SALT_SIZE)) {
        return;
    }
    esp_fill_random(salt, SALT_SIZE);
}
```

Lines 96-150 (Storage loading):
```cpp
bool PinManager::loadFromStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) return false;
    // Read from TROPIC01 R-Memory Slot 0
    se->readRMemory(0, buffer, sizeof(PinStorageFormat));
    // Parse and validate
}
```

Lines 200-280 (PIN validation):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    uint8_t hash[32];
    computeBadgeHash(pin, hash);
    return memcmp(hash, badgeHash_, 32) == 0;
}
```

## Recommended Fix
**Split into focused components** (each 1 hour task):

1. **Create `PinHasher` class**: Pure KDF computation:
   - `computeBadgeHash()`, `computeKdfHash()`
   - `generateSalt()`
   - No storage dependencies

2. **Create `PinStorage` class**: TROPIC01 persistence:
   - `loadFromStorage()`, `saveToStorage()`
   - `loadDefaults()`
   - Uses `PinHasher` for KDF

3. **Create `PinValidator` class**: Validation logic:
   - `verifyBadgePin()`, `verifyPw1()`, `verifyPw3()`
   - `getRetries()`, `resetRetries()`
   - Uses `PinHasher` for comparison

4. **Refactor `PinManager`**: Composes three components:
   - `init()` delegates to storage
   - `verifyBadgePin()` delegates to validator
   - `savePin()` delegates to storage

**Files to create**:
- `components/cdc_core/include/cdc_core/PinHasher.h`
- `components/cdc_core/include/cdc_core/PinStorage.h`
- `components/cdc_core/include/cdc_core/PinValidator.h`

**Migration steps**:
1. Create `PinHasher`, move KDF and salt functions
2. Create `PinStorage`, move load/save functions
3. Create `PinValidator`, move verify functions
4. Update `PinManager` to compose these three

## References
- SRP: https://en.wikipedia.org/wiki/Single-responsibility_principle
- KDF best practices: https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
- TROPIC01 R-Memory: https://www.tropic.works/products/tropic01/
