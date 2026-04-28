---
title: "[MEDIUM] Missing IStorage Interface for PasswordStore and TotpStore"
severity: MEDIUM
domain: architecture/api-contract
lens: storage-interface-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
Both `PasswordStore` and `TotpStore` implement similar storage patterns (read, write, delete, list) but have no common interface. This creates implicit contracts where consumers must know the specific class and its methods, rather than depending on a shared abstraction.

## Impact
- **Code duplication**: Similar storage logic not extracted to common interface.
- **Tight coupling**: Consumers import concrete classes instead of interfaces.
- **Hard to swap implementations**: No way to replace storage backend without changing all consumers.
- **Testing difficulty**: Mocking requires knowing concrete types.

## Evidence
**TotpStore usage - `components/mod_totp/src/TotpModule.cpp:498-524`:**
```cpp
void updateCode() {
    TotpStore& store = TotpStore::instance();
    timeValid_ = store.isTimeValid();
    
    TotpAccount account = {};
    if (!store.readAccount(slot_, &account)) {
        // ...
    }
    // ...
}
```

**TotpStore methods (implicit contract):**
```cpp
// From components/mod_totp/include/mod_totp/TotpStore.h
class TotpStore {
public:
    bool addAccount(const char* name, const char* issuer, const char* secret,
                    uint8_t digits, uint32_t period, uint8_t algorithm);
    bool updateAccount(uint16_t slot, const char* name, const char* issuer,
                       const char* secret, uint8_t digits, uint32_t period, uint8_t algorithm);
    bool deleteAccount(uint16_t slot);
    bool readAccount(uint16_t slot, TotpAccount* account);
    int8_t generateCode(uint16_t slot, char* code);
    // ...
};
```

**PasswordStore methods (similar but different signature):**
```cpp
// From components/mod_password/include/mod_password/PasswordStore.h
class PasswordStore {
public:
    bool addEntry(const char* title, const char* username, const char* password,
                  const char* url, const char* notes, uint16_t totpSlot);
    bool updateEntry(uint16_t slot, const char* title, const char* username,
                     const char* password, const char* url, const char* notes, uint16_t totpSlot);
    bool deleteEntry(uint16_t slot);
    bool readEntry(uint16_t slot, PasswordEntry* entry);
    // ...
};
```

**Consumers depend on concrete types:**
```cpp
// mod_totp includes concrete class
#include "mod_totp/TotpStore.h"

// mod_password includes concrete class
#include "mod_password/PasswordStore.h"
```

## Recommended Fix
**Create a generic storage interface:**

1. **Define IStorage interface in `cdc_core`:**
```cpp
namespace cdc::core {

struct StorageEntry {
    uint16_t slot;
    char name[32];
    uint16_t dataLen;
};

using StorageReadCallback = bool(*)(uint16_t slot, void* data, uint16_t maxLen);
using StorageWriteCallback = bool(*)(uint16_t slot, const void* data, uint16_t len);
using StorageDeleteCallback = bool(*)(uint16_t slot);

class IStorage {
public:
    virtual ~IStorage() = default;
    virtual const char* getName() const = 0;
    virtual uint16_t getCapacity() const = 0;
    virtual uint16_t getCount() const = 0;
    virtual bool add(const char* name, const uint8_t* data, uint16_t len) = 0;
    virtual bool update(uint16_t slot, const uint8_t* data, uint16_t len) = 0;
    virtual bool remove(uint16_t slot) = 0;
    virtual bool get(uint16_t slot, uint8_t* data, uint16_t maxLen) = 0;
    virtual void forEach(StorageReadCallback callback, void* context) = 0;
};

} // namespace cdc::core
```

2. **Update TotpStore and PasswordStore to implement interface.**

3. **Register storage in ServiceRegistry.**

## References
- TotpStore: `components/mod_totp/include/mod_totp/TotpStore.h`
- PasswordStore: `components/mod_password/include/mod_password/PasswordStore.h`
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
