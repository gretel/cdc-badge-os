---
title: "[MEDIUM] Duplicated storage layer helper functions across TOTP and Password modules"
severity: MEDIUM
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

The `mod_totp` and `mod_password` modules implement nearly identical storage layer helper functions for managing secure element slot operations. These functions are 90%+ identical with only minor naming differences:

- **`findFreeSlot`** - Lines 195-235 in `TotpStore.cpp`, Lines 160-200 in `PasswordStore.cpp`
- **`toPhysicalSlot`** - Lines 123-130 in `TotpStore.cpp`, Lines 87-94 in `PasswordStore.cpp`
- **`toLogicalSlot`** - Lines 138-146 in `TotpStore.cpp`, Lines 102-108 in `PasswordStore.cpp`
- **`setSlotRange`** - Lines 88-100 in `TotpStore.cpp`, Lines 58-70 in `PasswordStore.cpp`
- **`capacity`** - Lines 108-113 in `TotpStore.cpp`, Lines 76-81 in `PasswordStore.cpp`

### Code Comparison

**`findFreeSlot` - 95% identical (40 lines each):**

```cpp
// TotpStore.cpp (lines 195-235)
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));
    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), 0, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;
        }
    };

    ctx.base = rmemStart_;
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            if (candidate <= rmemEnd_) {
                *slotOut = candidate;
                return true;
            }
            return false;
        }
    }

    return false;
}

// PasswordStore.cpp (lines 160-200) - Nearly identical
bool PasswordStore::findFreeSlot(uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));

    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), rmemStart_, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;
        }
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            if (candidate <= rmemEnd_) {
                *slotOut = candidate;
                return true;
            }
            return false;
        }
    }

    return false;
}
```

**`toPhysicalSlot` - 100% identical (7 lines each):**

```cpp
// TotpStore.cpp (lines 123-130)
bool TotpStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}

// PasswordStore.cpp (lines 87-94) - Byte-for-byte identical
bool PasswordStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}
```

**`setSlotRange` - 100% identical (13 lines each):**

```cpp
// TotpStore.cpp (lines 88-100)
void TotpStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
    if (start > end || start == 0 || end == 0) {
        hasSlotRange_ = false;
        rmemStart_ = 0;
        rmemEnd_ = 0;
        moduleId_ = 0;
        return;
    }
    hasSlotRange_ = true;
    rmemStart_ = start;
    rmemEnd_ = end;
    moduleId_ = moduleId;
}

// PasswordStore.cpp (lines 58-70) - Byte-for-byte identical
void PasswordStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
    if (start > end || start == 0 || end == 0) {
        hasSlotRange_ = false;
        rmemStart_ = 0;
        rmemEnd_ = 0;
        moduleId_ = 0;
        return;
    }
    hasSlotRange_ = true;
    rmemStart_ = start;
    rmemEnd_ = end;
    moduleId_ = moduleId;
}
```

## Impact

- **Maintenance Burden:** Bug fixes or improvements to slot management must be applied in multiple places.
- **Code Bloat:** Approximately 150-200 lines of duplicated storage helper code across the two modules.
- **Inconsistency Risk:** One module might be updated while the other is forgotten, leading to subtle bugs.
- **Scalability:** As more modules are added (FIDO2, GPG, etc.), this duplication pattern will continue to grow.
- **Testing Overhead:** Same logic needs unit tests in multiple places.

## Evidence

**Files Affected:**
- `components/mod_totp/src/TotpStore.cpp`
- `components/mod_password/src/PasswordStore.cpp`

**Duplicated Functions:**
1. **`findFreeSlot`** (40 lines each, 95% similar) - Used to locate available storage slots
2. **`toPhysicalSlot`** (7 lines each, 100% identical) - Converts logical to physical slot index
3. **`toLogicalSlot`** (7 lines each, 100% identical) - Converts physical to logical slot index
4. **`setSlotRange`** (13 lines each, 100% identical) - Configures slot range mapping
5. **`capacity`** (5 lines each, 100% identical) - Returns slot capacity

**Additional Duplicated Patterns:**
- Both modules use identical `struct Ctx` for callback context in `findFreeSlot`
- Both modules use identical lambda callbacks for `TropicStorage::forEachSlot`
- Both modules use identical memory allocation pattern: `std::unique_ptr<bool[]>(new (std::nothrow) bool[cap])`

## Recommended Fix

### Create a Shared Storage Helper Base Class

Create `components/cdc_core/include/cdc_core/SlotManager.h`:

```cpp
#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include "cdc_core/TropicStorage.h"

namespace cdc::core {

/**
 * \brief Manages logical-to-physical slot mapping for secure element storage.
 */
class SlotManager {
public:
    /**
     * \brief Configures logical-to-physical slot mapping.
     * \param start First RMEM slot.
     * \param end Last RMEM slot.
     * \param moduleId Owning module identifier.
     */
    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
        if (start > end || start == 0 || end == 0) {
            hasSlotRange_ = false;
            rmemStart_ = 0;
            rmemEnd_ = 0;
            moduleId_ = 0;
            return;
        }
        hasSlotRange_ = true;
        rmemStart_ = start;
        rmemEnd_ = end;
        moduleId_ = moduleId;
    }

    /**
     * \brief Returns available entry capacity from configured slot range.
     * \return Number of addressable logical entries.
     */
    uint16_t capacity() const {
        if (!hasSlotRange_) return 0;
        return static_cast<uint16_t>(rmemEnd_ - rmemStart_ + 1);
    }

    /**
     * \brief Converts logical index to physical RMEM slot.
     * \param logicalIndex Logical index.
     * \param slotOut Output physical slot.
     * \return `true` on valid mapping.
     */
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
        if (!slotOut) return false;
        if (!hasSlotRange_) return false;
        uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
        if (slot > rmemEnd_) return false;
        *slotOut = static_cast<uint16_t>(slot);
        return true;
    }

    /**
     * \brief Converts physical RMEM slot to logical index.
     * \param slot Physical slot.
     * \param logicalIndexOut Output logical index.
     * \return `true` on valid mapping.
     */
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const {
        if (!logicalIndexOut) return false;
        if (!hasSlotRange_) return false;
        if (slot < rmemStart_ || slot > rmemEnd_) return false;
        *logicalIndexOut = static_cast<uint16_t>(slot - rmemStart_);
        return true;
    }

    /**
     * \brief Finds first free slot in configured range.
     * \param slotOut Output physical slot.
     * \return `true` if free slot found.
     */
    bool findFreeSlot(uint16_t* slotOut) {
        if (!slotOut) return false;
        if (!hasSlotRange_) return false;

        uint16_t cap = capacity();
        if (cap == 0) return false;
        auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
        if (!used) return false;
        memset(used.get(), 0, cap * sizeof(bool));

        struct Ctx {
            bool* used;
            uint16_t base;
            uint16_t cap;
        } ctx = { used.get(), rmemStart_, cap };

        auto cb = [](uint16_t slot, const TropicStorage::CacheEntry&, void* user) {
            auto* c = static_cast<Ctx*>(user);
            if (slot < c->base) return;
            uint16_t idx = slot - c->base;
            if (idx < c->cap) {
                c->used[idx] = true;
            }
        };

        TropicStorage::instance().forEachSlot(moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

        for (uint16_t i = 0; i < cap; i++) {
            if (!used[i]) {
                uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
                if (candidate <= rmemEnd_) {
                    *slotOut = candidate;
                    return true;
                }
                return false;
            }
        }

        return false;
    }

    /**
     * \brief Returns whether slot range is configured.
     * \return `true` if range is valid.
     */
    bool hasSlotRange() const { return hasSlotRange_; }

    /**
     * \brief Returns configured RMEM start slot.
     * \return RMEM start slot.
     */
    uint16_t rmemStart() const { return rmemStart_; }

    /**
     * \brief Returns configured RMEM end slot.
     * \return RMEM end slot.
     */
    uint16_t rmemEnd() const { return rmemEnd_; }

    /**
     * \brief Returns configured module ID.
     * \return Module ID.
     */
    uint8_t moduleId() const { return moduleId_; }

protected:
    bool hasSlotRange_ = false;
    uint16_t rmemStart_ = 0;
    uint16_t rmemEnd_ = 0;
    uint8_t moduleId_ = 0;
};

} // namespace cdc::core
```

### Implementation Steps

1. **Create `components/cdc_core/include/cdc_core/SlotManager.h`** with the base class above.

2. **Update `TotpStore.cpp`:**
   - Add `#include "cdc_core/SlotManager.h"`
   - Make `TotpStore` inherit from `SlotManager` (or compose with it)
   - Remove local `setSlotRange`, `capacity`, `toPhysicalSlot`, `toLogicalSlot`, `findFreeSlot` methods
   - Call inherited methods directly or via `SlotManager::` prefix

3. **Update `PasswordStore.cpp`:**
   - Same changes as TOTP module

4. **Future modules** (FIDO2, GPG, etc.) can also use `SlotManager` for consistent slot management.

### Alternative: Simpler Helper Functions

If inheritance is too complex, create standalone helper functions:

```cpp
// components/cdc_core/include/cdc_core/SlotHelpers.h
namespace cdc::core {

inline bool toPhysicalSlot(uint16_t rmemStart, uint16_t rmemEnd,
                           uint16_t logicalIndex, uint16_t* slotOut) {
    if (!slotOut) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart) + logicalIndex;
    if (slot > rmemEnd) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}

} // namespace cdc::core
```

## References

- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [Base Class Pattern](https://en.cppreference.com/w/cpp/language/derived_class)
- Related findings: #1 (token parsing), #2 (UI helpers), #3 (wizard state), #4 (wizard completion), #5 (lifecycle boilerplate)
