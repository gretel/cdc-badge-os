---
title: "[LOW] Duplicate Code: findFreeSlot() logic duplicated in TotpStore and PasswordStore"
severity: LOW
domain: modules
lens: code-smells
labels:
  - "duplicate-code"
  - "mod_totp"
  - "mod_password"
---

## Summary
The `findFreeSlot()` method in `components/mod_totp/src/TotpStore.cpp:192-227` and `components/mod_password/src/PasswordStore.cpp:153-199` have nearly identical implementations. Both use the same pattern of allocating a boolean array, iterating through slots, and finding the first free one.

## Impact
**Maintenance**: Changes to slot-finding logic must be applied in multiple places.

**Consistency**: Different implementations may behave slightly differently.

**Testing**: Need to test the same logic in multiple modules.

## Evidence
`components/mod_totp/src/TotpStore.cpp:192-227`:
```cpp
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
```

`components/mod_password/src/PasswordStore.cpp:153-199`:
```cpp
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

Nearly identical (90%+ code similarity).

## Recommended Fix
1. **Create a base class for stores**:
```cpp
class SlotStore {
protected:
    bool findFreeSlotInRange(uint16_t start, uint16_t end, uint8_t moduleId, uint16_t* slotOut);
};

class TotpStore : public SlotStore {
    // Use inherited findFreeSlotInRange
};

class PasswordStore : public SlotStore {
    // Use inherited findFreeSlotInRange
};
```

2. **Or create a utility function**:
```cpp
namespace cdc::core {
    bool findFreeSlot(uint16_t start, uint16_t end, uint8_t moduleId, uint16_t* slotOut);
}
```

**Estimated effort**: ~1 hour to create shared utility and update both stores.

## References
- Refactoring.com: "Duplicate Code" - https://refactoring.com/catalog/extractMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
