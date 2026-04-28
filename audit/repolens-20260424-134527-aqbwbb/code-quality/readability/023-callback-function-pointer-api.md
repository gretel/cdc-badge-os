---
title: "[023] [LOW] Callback function pointer API in TotpModule uses opaque void*"
severity: LOW
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/mod_totp/src/TotpModule.cpp`, several callback functions use `void* userData` parameters that are opaque at the call site:

```cpp
static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) {
    // ...
    struct Ctx {
        uint16_t target;
        uint16_t current;
        uint16_t slot;
        bool found;
    } ctx = { index, 0, 0, false };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);  // Cast needed
        // ...
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        store.moduleId(), store.rmemStart(), store.rmemEnd(), cb, &ctx);
}
```

Similar patterns appear in `cmd_totp_list` and `rebuildList`.

## Impact
- **Type safety**: The `void*` allows passing any pointer, but the callback expects a specific type
- **Readability**: Caller must cast `void*` back to the original struct type
- **Error-prone**: Wrong cast type compiles but causes runtime errors

## Evidence
**File**: `components/mod_totp/src/TotpModule.cpp:151-220`

Three locations use this pattern:
1. `findSlotByIndex` (lines 151-189)
2. `cmd_totp_list` (lines 195-220)
3. `rebuildList` (lines 677-707)

## Recommended Fix
Use `std::function` with captured context instead of `void*`:

```cpp
// Before
static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) {
    struct Ctx {
        uint16_t target;
        uint16_t current;
        uint16_t slot;
        bool found;
    } ctx = { index, 0, 0, false };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        // ...
    };
    cdc::core::TropicStorage::instance().forEachSlot(..., cb, &ctx);
}

// After - using std::function with capture
static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) {
    struct Ctx {
        uint16_t target;
        uint16_t current = 0;
        uint16_t slot = 0;
        bool found = false;
    } ctx = { index };

    auto cb = [&](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&) {
        if (ctx.found) return;
        uint16_t logical = 0;
        if (!TotpStore::instance().toLogicalSlot(slot, &logical)) return;
        if (ctx.current == ctx.target) {
            ctx.slot = logical;
            ctx.found = true;
            return;
        }
        ctx.current++;
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        store.moduleId(), store.rmemStart(), store.rmemEnd(), cb, nullptr);
    // ...
}
```

Or better, use a template for type-safe callbacks:
```cpp
template <typename T>
void forEachSlot(uint8_t moduleId, uint16_t start, uint16_t end,
                 std::function<void(uint16_t, const CacheEntry&, T*)> cb, T* context);
```

## References
- [C++ Core Guidelines - R.11: Avoid generic (void*) pointers to objects](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#R11-avoid-generic-void-pointers-to-objects)
- [Effective Modern C++ - Item 39: Use `std::function` judiciously](https://www.oreilly.com/library/view/effective-modern-c/9781491908442/)
