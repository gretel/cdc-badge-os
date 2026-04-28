---
title: "[MEDIUM] Hardcoded Lock Screen Context Menu Callbacks Limit Extension"
severity: MEDIUM
domain: architecture/extensibility
lens: extensibility-plugin
labels:
  - "hardcoded-behavior"
  - "open-closed-principle"
  - "lock-screen"
---

## Summary

The lock screen context menu uses 7 hardcoded callback wrapper functions (`moduleContextCallback0` through `moduleContextCallback6`) to invoke module-provided actions. Adding an 8th module context item requires:

1. Adding a new `moduleContextCallback7()` function
2. Adding it to the `s_moduleCallbacks[]` array
3. Modifying `LockScreenView.cpp` core code

This is a classic "growing switch/callback chain" anti-pattern where each new item requires modifying the core.

**Evidence:**

`components/cdc_os_ui/src/views/LockScreenView.cpp:267-302`:
```cpp
static void moduleContextCallback0() { if (s_moduleContextItems[0].callback) { s_moduleContextItems[0].callback(); } hideContextMenu(); }
static void moduleContextCallback1() { if (s_moduleContextItems[1].callback) { s_moduleContextItems[1].callback(); } hideContextMenu(); }
static void moduleContextCallback2() { if (s_moduleContextItems[2].callback) { s_moduleContextItems[2].callback(); } hideContextMenu(); }
static void moduleContextCallback3() { if (s_moduleContextItems[3].callback) { s_moduleContextItems[3].callback(); } hideContextMenu(); }
static void moduleContextCallback4() { if (s_moduleContextItems[4].callback) { s_moduleContextItems[4].callback(); } hideContextMenu(); }
static void moduleContextCallback5() { if (s_moduleContextItems[5].callback) { s_moduleContextItems[5].callback(); } hideContextMenu(); }
static void moduleContextCallback6() { if (s_moduleContextItems[6].callback) { s_moduleContextItems[6].callback(); } hideContextMenu(); }

static void (*const s_moduleCallbacks[])() = {
    moduleContextCallback0, moduleContextCallback1, moduleContextCallback2,
    moduleContextCallback3, moduleContextCallback4, moduleContextCallback5,
    moduleContextCallback6
};
```

Currently limited to 7 module items (`MAX_CONTEXT_ITEMS = 8` with 1 reserved for "Light" toggle).

## Impact

**Scalability:** Adding more than 7 module context items requires core modification.

**Code Duplication:** 7 nearly-identical wrapper functions that could be replaced with a single generic callback.

**Maintenance:** Every new context item slot requires touching core UI code, risking regressions.

## Recommended Fix

Replace the hardcoded callback array with a single generic wrapper that captures the index:

**Option 1 - Lambda capture (C++17):**
```cpp
// In LockScreenView::onKey():
for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
    const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
    // Capture index by value in lambda
    auto* context = new uint8_t(i);  // Alloc once, freed by ContextMenuView
    s_contextItems[itemCount++] = {
        label,
        [context]() {
            uint8_t idx = *context;
            if (s_moduleContextItems[idx].callback) {
                s_moduleContextItems[idx].callback();
            }
            hideContextMenu();
            delete context;  // Clean up
        }
    };
}
```

**Option 2 - Generic wrapper with userData:**
```cpp
static void moduleContextCallback(void* userData) {
    uint8_t idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(userData));
    if (s_moduleContextItems[idx].callback) {
        s_moduleContextItems[idx].callback();
    }
    hideContextMenu();
}

// In LockScreenView::onKey():
for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
    const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
    s_contextItems[itemCount++] = {
        label,
        moduleContextCallback,  // Same function for all
        reinterpret_cast<void*>(static_cast<uintptr_t>(i))  // Pass index as userData
    };
}
```

**Option 3 - Increase MAX_CONTEXT_ITEMS statically:**
If 7 items is typically enough, simply increase `MAX_CONTEXT_ITEMS` to 16 and add more callbacks. This is the quickest fix but doesn't solve the architectural issue.

**Recommended:** Option 2 - Generic wrapper with userData. It's the cleanest, requires ~1 hour to implement, and scales to any number of items.

## References

- Context Menu API: `components/cdc_views/include/cdc_views/ContextMenuView.h`
- Current implementation: `components/cdc_os_ui/src/views/LockScreenView.cpp:267-302`
