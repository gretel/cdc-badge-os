---
title: "[MEDIUM] Lock screen context menu uses hardcoded callback indices"
severity: MEDIUM
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The lock screen context menu uses hardcoded callback functions (`moduleContextCallback0` through `moduleContextCallback6`) with fixed indices to handle module context items. This limits the menu to a maximum of 7 module items and creates tight coupling between the menu structure and callback definitions.

**Files affected:**
- `components/cdc_os_ui/src/views/LockScreenView.cpp` - Context menu callback definitions (lines 267-302)

## Impact
**Scalability limit:** Maximum of 7 module context items (indices 0-6) before the pattern breaks.
**Maintenance burden:** Adding an 8th module context item requires adding a new callback function and updating the array.
**Fragility:** If module order changes, the wrong callback might be called for a menu item.
**Code duplication:** 7 nearly identical callback functions that could be replaced with a single generic handler.

## Evidence
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp (lines 267-302)
static void moduleContextCallback0() { if (s_moduleContextItems[0].callback) { s_moduleContextItems[0].callback(); } hideContextMenu(); }
static void moduleContextCallback1() { if (s_moduleContextItems[1].callback) { s_moduleContextItems[1].callback(); } hideContextMenu(); }
static void moduleContextCallback2() { if (s_moduleContextItems[2].callback) { s_moduleContextItems[2].callback(); } hideContextMenu(); }
static void moduleContextCallback3() { if (s_moduleContextItems[3].callback) { s_moduleContextItems[3].callback(); } hideContextMenu(); }
static void moduleContextCallback4() { if (s_moduleContextItems[4].callback) { s_moduleContextItems[4].callback(); } hideContextMenu(); }
static void moduleContextCallback5() { if (s_moduleContextItems[5].callback) { s_moduleContextItems[5].callback(); } hideContextMenu(); }
static void moduleContextCallback6() { if (s_moduleContextItems[6].callback) { s_moduleContextItems[6].callback(); } hideContextMenu(); }

static const cdc::ui::ContextMenuItem s_moduleCallbacks[] = {
    moduleContextCallback0, moduleContextCallback1, moduleContextCallback2,
    moduleContextCallback3, moduleContextCallback4, moduleContextCallback5,
    moduleContextCallback6
};
```

The callbacks are then used when building the context menu:
```cpp
// components/cdc_os_ui/src/views/LockScreenView.cpp (line 328)
for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
    const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
    s_contextItems[itemCount++] = {label, s_moduleCallbacks[i]};
}
```

## Recommended Fix
Replace hardcoded callbacks with a single generic handler that uses the selection index:

1. **Create a generic callback** that takes an index parameter:
```cpp
static void moduleContextCallback(uint8_t index) {
    if (index < s_moduleContextCount && s_moduleContextItems[index].callback) {
        s_moduleContextItems[index].callback();
    }
    hideContextMenu();
}
```

2. **Update the ContextMenuItem structure** to store the index:
```cpp
static const cdc::ui::ContextMenuItem s_moduleCallbacks[] = {
    {nullptr, []() { moduleContextCallback(0); }},
    {nullptr, []() { moduleContextCallback(1); }},
    // ... etc, or use a lambda that captures i
};
```

3. **Better approach:** Use the `userData` field in `ContextMenuItem` to store the index:
```cpp
static void moduleContextCallback(uint8_t index) {
    if (index < s_moduleContextCount && s_moduleContextItems[index].callback) {
        s_moduleContextItems[index].callback();
    }
    hideContextMenu();
}

// In menu building loop:
for (uint8_t i = 0; i < s_moduleContextCount && itemCount < MAX_CONTEXT_ITEMS; i++) {
    const char* label = s_moduleContextItems[i].getLabel ? s_moduleContextItems[i].getLabel() : "???";
    s_contextItems[itemCount++] = {label, reinterpret_cast<void*>(i + 1)};  // Store index
}

// In context menu handler:
static void onContextSelect(uint16_t index, void* userData) {
    uint8_t moduleIndex = reinterpret_cast<uintptr_t>(userData) - 1;
    moduleContextCallback(moduleIndex);
}
```

**Scope:** ~1 hour to refactor the callback pattern and test with multiple modules.

## References
- Lock screen context menu: `components/cdc_os_ui/src/views/LockScreenView.cpp` lines 259-330
- Context menu API: `components/cdc_views/include/cdc_views/ContextMenuView.h`
