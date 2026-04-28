---
title: "[MEDIUM] mod_totp: Internal TotpStore.h exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_totp` module exposes an internal storage class header in its public include directory:

- **`TotpStore.h`** - TOTP account storage and code generation implementation (internal class)

Only `TotpModule.h` should be public. `TotpStore.h` is a storage layer implementation detail.

## Impact

- **Implementation Leakage**: External modules can depend on internal storage class
- **Tight Coupling**: Changes to storage format break external consumers
- **Poor Encapsulation**: Private members and internal methods exposed
- **Namespace Pollution**: Internal constants and types clutter the public API

## Evidence

**Current public include structure:**
```
components/mod_totp/include/mod_totp/
├── TotpModule.h    # Public API (correct)
└── TotpStore.h     # Storage layer (internal - exposed!)
```

**Internal header contents:**

`TotpStore.h` (line 1-70):
```cpp
// TOTP algorithm enum
enum class TotpAlgorithm : uint8_t {
    SHA1 = 0,
    SHA256 = 1,
    SHA512 = 2
};

// TOTP account structure
struct TotpAccount {
    char name[16 + 1];
    char issuer[32 + 1];
    uint8_t secret[32];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;
};

// TOTP store class (internal implementation)
class TotpStore {
public:
    static constexpr uint8_t NAME_LEN = 16;
    static constexpr uint8_t ISSUER_LEN = 32;
    static constexpr uint8_t SECRET_LEN = 32;
    static constexpr uint8_t DEFAULT_DIGITS = 6;
    static constexpr uint32_t DEFAULT_PERIOD = 30;

    bool readAccount(uint16_t slot, TotpAccount* out);
    bool addAccount(const char* name, const char* issuer, const char* secretBase32,
                    uint8_t digits, uint32_t period, uint8_t algorithm);
    bool updateAccount(uint16_t slot, const char* name, const char* issuer, const char* secretBase32,
                       uint8_t digits, uint32_t period, uint8_t algorithm);
    bool deleteAccount(uint16_t slot);

    int8_t generateCode(uint16_t slot, char* codeOut);

    bool isTimeValid() const;
    uint8_t timeRemaining(uint32_t period) const;

    static TotpStore& instance();

    // Internal slot management
    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    uint16_t capacity() const;
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;

private:
    bool findFreeSlot(uint16_t* slotOut);
    uint32_t generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                      uint32_t period, uint8_t digits, TotpAlgorithm algo) const;
    bool hmacCompute(TotpAlgorithm algo, const uint8_t* key, size_t keyLen,
                     const uint8_t* data, size_t dataLen,
                     uint8_t* output, size_t* outputLen) const;

    bool hasSlotRange_ = false;
    uint16_t rmemStart_ = 0;
    uint16_t rmemEnd_ = 0;
    uint8_t moduleId_ = 0;
};
```

**Usage in module:**
```cpp
// From components/mod_totp/src/TotpModule.cpp:
#include "mod_totp/TotpStore.h"

// From components/mod_totp/src/TotpStore.cpp:
#include "mod_totp/TotpStore.h"
```

**Internal implementation details exposed:**
- `TotpAccount` structure (data layout)
- `TotpStore` class with private members visible
- Internal slot management functions
- HMAC computation function

## Recommended Fix

1. **Move TotpStore.h to src/**:
   ```
   components/mod_totp/
   ├── include/mod_totp/
   │   └── TotpModule.h    # Only public API
   └── src/
       ├── TotpModule.cpp
       └── TotpStore.h     # Move here (internal)
           TotpStore.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/TotpModule.cpp, change:
   #include "mod_totp/TotpStore.h"  # → #include "TotpStore.h"
   ```

3. **Add internal marker**:
   ```cpp
   // In src/TotpStore.h:
   /**
    * @file TotpStore.h
    * @brief Internal TOTP storage - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `TotpModule.h`:
   ```cpp
   /**
    * @defgroup totp-public Public API
    * @brief TOTP module - Time-based One-Time Password
    * 
    * Public classes:
    * - @ref TotpModule - Main module interface
    */
   ```

5. **Consider public interface**:
   - If external modules need TOTP codes, expose via `TotpModule` methods
   - Keep `TotpStore` implementation details internal

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`

(End of file - total 154 lines)
