---
title: "[MEDIUM] TOTP module list buffers leak on repeated menu entry"
severity: MEDIUM
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/mod_totp/src/TotpModule.cpp`, the TOTP module allocates dynamic list buffers in `ensureListBuffers()` (line 666-680) but only frees them in `TotpModule::stop()` (line 968-972). However, `stop()` is only called when the module is explicitly stopped, not when the user navigates away from the TOTP menu. This causes the buffers to persist in memory even when the TOTP list view is no longer visible.

**Location:** `components/mod_totp/src/TotpModule.cpp:666-680`, `components/mod_totp/src/TotpModule.cpp:968-972`

## Impact

- **Memory leak:** Each time the TOTP menu is opened (if capacity changes), new buffers are allocated via `delete[]`/`new` in `ensureListBuffers()`, but the old buffers are not freed until `stop()` is called.
- **Embedded context:** On ESP32-S3 with limited PSRAM, repeated allocation/deallocation without proper cleanup can fragment heap memory.
- **Accumulation:** If the module stays running (typical case), buffers allocated at first menu entry remain allocated forever, even if the slot count changes and reallocation happens.

### Evidence

```cpp
// components/mod_totp/src/TotpModule.cpp:666-680
static bool ensureListBuffers() {
    uint16_t cap = TotpStore::instance().capacity();
    if (cap == 0) return false;
    if (cap == s_capacity && s_listItems && s_listLabels && s_listSlots) return true;

    delete[] s_listItems;        // Free old buffers
    delete[] s_listLabels;
    delete[] s_listSlots;
    s_listItems = nullptr;
    s_listLabels = nullptr;
    s_listSlots = nullptr;
    s_capacity = 0;

    s_listItems = new (std::nothrow) ui::ListItem[cap + 1];  // Allocate new
    s_listLabels = new (std::nothrow) char[cap][24];
    s_listSlots = new (std::nothrow) uint16_t[cap];
    // ...
}
```

```cpp
// components/mod_totp/src/TotpModule.cpp:968-972
void TotpModule::stop() {
    freeListBuffers();          // Only freed here
    state_ = core::ServiceState::STOPPED;
}
```

**Problem:** `ensureListBuffers()` is called from `rebuildList()` which is called from `getMenuItems()` (line 998-1004) every time the TOTP menu is accessed. If the capacity changes, the old buffers are freed and new ones allocated, but if capacity stays the same, buffers are reused. However, there's no mechanism to free buffers when the view is popped from the stack.

## Recommended Fix

Add a `onViewExit()` callback or similar mechanism to free list buffers when the TOTP list view is popped, not just when the module stops. Two options:

### Option 1: Free buffers when list view is popped (recommended)

Add a callback in `ListView` that is called when the view is popped:

```cpp
// In TotpModule.cpp, modify onListSelect to pop with cleanup:
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    if (index - 1 >= s_accountCount) return;

    uint16_t slot = s_listSlots[index - 1];
    const char* name = s_listLabels[index - 1];
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);
}

// Add onViewExit callback for ListView:
static void onListViewExit() {
    freeListBuffers();
}

// In getMenuItems, set the callback:
items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_listView.setOnExit(onListViewExit);  // New callback
        s_viewsInitialized = true;
    }
    rebuildList();
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

Then add `onExit()` callback support to `ListView`/`IView`:

```cpp
// In IView.h
class IView {
    // ...
    virtual void onExit() override { }  // Already exists
    // Add optional view-specific cleanup callback
    using ViewExitCallback = void(*)();
    void setOnExit(ViewExitCallback cb) { onExitCb_ = cb; }
    // ...
private:
    ViewExitCallback onExitCb_ = nullptr;
};
```

### Option 2: Free buffers on every rebuild (simpler)

Modify `ensureListBuffers()` to always free and reallocate:

```cpp
static bool ensureListBuffers() {
    uint16_t cap = TotpStore::instance().capacity();
    if (cap == 0) return false;

    // Always free and reallocate (simpler, no leak)
    freeListBuffers();

    s_listItems = new (std::nothrow) ui::ListItem[cap + 1];
    s_listLabels = new (std::nothrow) char[cap][24];
    s_listSlots = new (std::nothrow) uint16_t[cap];
    if (!s_listItems || !s_listLabels || !s_listSlots) {
        freeListBuffers();
        s_capacity = 0;
        return false;
    }
    s_capacity = cap;
    return true;
}
```

Then call `freeListBuffers()` in `TotpModule::stop()` (already done) and optionally in a view exit callback.

### Option 3: Use static buffers with max capacity (most efficient for embedded)

Pre-allocate maximum possible buffers once:

```cpp
// Define max TOTP accounts based on slot map
static constexpr uint16_t MAX_TOTP_ACCOUNTS = 100;  // Adjust based on slot map
static ui::ListItem s_listItems[MAX_TOTP_ACCOUNTS + 1];
static char s_listLabels[MAX_TOTP_ACCOUNTS][24];
static uint16_t s_listSlots[MAX_TOTP_ACCOUNTS];
```

This eliminates dynamic allocation entirely.

## References

- ESP32-S3 memory: 512KB SRAM + optional PSRAM (typically 2MB or 8MB)
- FreeRTOS heap fragmentation: https://www.freertos.org/FreeRTOS-bridge/FreeRTOS-E-Book/FreeRTOS-Ebook.html#Heap-Management
- C++ `new`/`delete` in embedded: https://www.embedded.com/use-dynamic-memory-allocation-in-an-embedded-system/
