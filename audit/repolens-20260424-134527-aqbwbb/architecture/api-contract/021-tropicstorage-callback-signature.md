---
title: "[MEDIUM] TropicStorage slot iteration callbacks lack error reporting"
severity: MEDIUM
domain: architecture/api-contract
lens: callback-signature
labels:
  - "audit:architecture/api-contract"
---

## Summary
`TropicStorage` provides three slot iteration methods (`forEachSlot`) that take a callback with signature `void(*)(uint16_t slot, const CacheEntry& entry, void* ctx)`. The callback has no way to signal an error back to the iterator, forcing callers to rely on side effects or global state to handle iteration failures.

**Evidence location:** `components/cdc_core/include/cdc_core/TropicStorage.h:38-42`

```cpp
using SlotCallback = void(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
using RebuildLogFn = void(*)(uint16_t slot, const char* message, void* ctx);

// Iteration helpers
bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);
bool forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                 SlotCallback cb, void* ctx);
bool getSlot(uint8_t moduleId, uint16_t index, SlotCallback cb, void* ctx);
```

The `SlotCallback` returns `void`, so if the callback encounters an error (e.g., processing a slot fails), it cannot communicate this back to the iterator.

## Impact
1. **Silent failures**: A callback that fails to process a slot has no mechanism to stop iteration or report the error
2. **Caller must track state externally**: Callers need separate variables to track iteration success, breaking encapsulation
3. **No early termination**: Callbacks cannot signal "stop iterating" without using awkward global state or context object mutation

**Example problem:**
```cpp
struct MyContext {
    bool errorOccurred = false;
};

void callback(uint16_t slot, const TropicStorage::CacheEntry& entry, void* ctx) {
    MyContext* c = static_cast<MyContext*>(ctx);
    // Process slot...
    if (processFailed(slot)) {
        c->errorOccurred = true;  // Side effect, not a return value!
        // But iteration continues!
    }
}

// Caller must check side effect after iteration
MyContext ctx;
storage.forEachSlot(MODULE_ID, callback, &ctx);
if (ctx.errorOccurred) {  // Had to use side effect
    // Handle error
}
```

## Evidence
The return type `bool` on `forEachSlot` suggests it can fail, but:
- It only reports errors from the iteration itself (e.g., invalid parameters)
- It cannot report errors from the callback function
- There's no way to stop iteration early from within the callback

## Recommended Fix
Change the callback signature to return a status:

```cpp
// Option A: Return bool for success/failure
using SlotCallback = bool(*)(uint16_t slot, const CacheEntry& entry, void* ctx);

// Option B: Return error code for more detail
using SlotCallback = int(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
// Return 0 for success, negative for error

// Usage:
bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);
// Returns false if callback returned false or iteration failed
```

Implementation change:
```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx) {
    // ...
    for (uint16_t slot = start; slot <= end; slot++) {
        CacheEntry entry;
        if (!getEntry(slot, &entry)) continue;
        
        if (!cb(slot, entry, ctx)) {  // Callback can signal stop
            return false;  // Early termination
        }
    }
    return true;
}
```

## References
- C++ Core Guidelines F.4: A function should perform a single operation
- C++ Core Guidelines F.6: Use `noexcept` to indicate a function won't throw
- C++ Core Guidelines F.54: Avoid passing 'T' by reference-to-const if 'T' is cheaply copyable (for callbacks)
