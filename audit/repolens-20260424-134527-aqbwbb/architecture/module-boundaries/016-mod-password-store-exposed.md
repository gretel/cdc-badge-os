---
title: "[MEDIUM] mod_password: Internal PasswordStore.h exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_password` module exposes an internal storage class header in its public include directory:

- **`PasswordStore.h`** - Password storage and retrieval implementation (internal class)

Only `PasswordModule.h` should be public. `PasswordStore.h` is a storage layer implementation detail.

## Evidence

**Current public include structure:**
```
components/mod_password/include/mod_password/
├── PasswordModule.h    # Public API (correct)
└── PasswordStore.h     # Storage layer (internal - exposed!)
```

**Internal header contents:**

`PasswordStore.h` (line 1-80):
```cpp
// Password storage constants
constexpr uint8_t PASSWORD_TITLE_LEN = 24;
constexpr uint8_t PASSWORD_USERNAME_LEN = 16;
constexpr uint8_t PASSWORD_PASSWORD_LEN = 64;
constexpr uint8_t PASSWORD_URL_LEN = 64;
constexpr size_t PASSWORD_PAYLOAD_MAX = ...;

// Password entry structure
struct PasswordEntry {
    char title[PASSWORD_TITLE_LEN + 1];
    char username[PASSWORD_USERNAME_LEN + 1];
    char password[PASSWORD_PASSWORD_LEN + 1];
    char url[PASSWORD_URL_LEN + 1];
    uint8_t totpSlot;
    char notes[PASSWORD_NOTES_LEN + 1];
};

// Password store class (internal implementation)
class PasswordStore {
public:
    static constexpr uint8_t TITLE_LEN = PASSWORD_TITLE_LEN;
    static constexpr uint8_t USERNAME_LEN = PASSWORD_USERNAME_LEN;
    // ... many constants

    struct EntryIndex {
        char title[TITLE_LEN + 1];
        uint16_t slot;
    };

    static PasswordStore& instance();

    bool readEntry(uint16_t slot, PasswordEntry* out) const;
    bool addEntry(const PasswordEntry& entry);
    bool updateEntry(uint16_t slot, const PasswordEntry& entry);
    bool deleteEntry(uint16_t slot);
    bool listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const;

    // Internal slot management
    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    uint16_t capacity() const;
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;

private:
    bool findFreeSlot(uint16_t* slotOut) const;
    static int compareTitles(const char* a, const char* b);

    bool hasSlotRange_ = false;
    uint16_t rmemStart_ = 0;
    uint16_t rmemEnd_ = 0;
    uint8_t moduleId_ = 0;
};
```

**Usage in module:**
```cpp
// From components/mod_password/src/PasswordModule.cpp:
#include "mod_password/PasswordStore.h"

// From components/mod_password/src/PasswordStore.cpp:
#include "mod_password/PasswordStore.h"
```

**Internal implementation details exposed:**
- `PasswordEntry` structure (data layout)
- `PasswordStore` class with private members visible
- Internal slot management functions
- R-Memory storage constants

## Impact

- **Implementation Leakage**: External modules can depend on internal storage class
- **Tight Coupling**: Changes to storage format break external consumers
- **Poor Encapsulation**: Private members and internal methods exposed
- **Namespace Pollution**: Internal constants and types clutter the public API

## Recommended Fix

1. **Move PasswordStore.h to src/**:
   ```
   components/mod_password/
   ├── include/mod_password/
   │   └── PasswordModule.h    # Only public API
   └── src/
       ├── PasswordModule.cpp
       └── PasswordStore.h     # Move here (internal)
           PasswordStore.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/PasswordModule.cpp, change:
   #include "mod_password/PasswordStore.h"  # → #include "PasswordStore.h"
   ```

3. **Add internal marker**:
   ```cpp
   // In src/PasswordStore.h:
   /**
    * @file PasswordStore.h
    * @brief Internal password storage - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `PasswordModule.h`:
   ```cpp
   /**
    * @defgroup password-public Public API
    * @brief Password module - Password vault
    * 
    * Public classes:
    * - @ref PasswordModule - Main module interface
    */
   ```

5. **Consider public interface**:
   - If external modules need password access, expose via `PasswordModule` methods
   - Keep `PasswordStore` implementation details internal

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`

(End of file - total 154 lines)
