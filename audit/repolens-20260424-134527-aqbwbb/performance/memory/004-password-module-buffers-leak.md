---
title: "[MEDIUM] Password module list buffers leak on repeated menu entry (similar to TOTP)"
severity: MEDIUM
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/mod_password/src/PasswordModule.cpp`, the Password module has the **same memory leak pattern** as the TOTP module (see related issue #001). The module allocates dynamic list buffers in `ensureListBuffers()` but only frees them in `stop()`, which is only called when the module is explicitly stopped, not when the user navigates away from the Password menu.

**Location**: `components/mod_password/src/PasswordModule.cpp:367-400` (buffers), `components/mod_password/src/PasswordModule.cpp:794-797` (stop)

## Impact

- **Memory leak**: Each time the Password menu is opened (if capacity changes), new buffers are allocated in `ensureListBuffers()`, but old buffers are not freed until `stop()` is called.
- **Embedded context**: On ESP32-S3 with limited PSRAM, repeated allocation/deallocation without proper cleanup fragments heap memory.
- **Accumulation**: Buffers allocated at first menu entry remain allocated forever, even when the view is no longer visible.

### Evidence

**components/mod_password/src/PasswordModule.cpp:367-400**:
```cpp
static void freeListBuffers() {
    delete[] s_listItems;
    delete[] s_entries;
    s_listItems = nullptr;
    s_entries = nullptr;
    s_capacity = 0;
    s_entryCount = 0;
}

static bool ensureListBuffers() {
    uint16_t cap = PasswordStore::instance().capacity();
    if (cap == 0) return false;
    if (cap == s_capacity && s_listItems && s_entries) return true;

    delete[] s_listItems;
    delete[] s_entries;
    s_listItems = nullptr;
    s_entries = nullptr;
    s_capacity = 0;

    s_listItems = new (std::nothrow) ui::ListItem[cap + 1];
    s_entries = new (std::nothrow) PasswordStore::EntryIndex[cap];
    if (!s_listItems || !s_entries) {
        delete[] s_listItems;
        delete[] s_entries;
        s_listItems = nullptr;
        s_entries = nullptr;
        s_capacity = 0;
        return false;
    }
    s_capacity = cap;
    return true;
}
```

**components/mod_password/src/PasswordModule.cpp:794-797**:
```cpp
void PasswordModule::stop() {
    freeListBuffers();          // Only freed here
    state_ = core::ServiceState::STOPPED;
}
```

**components/mod_password/src/PasswordModule.cpp:824-840** (menu entry):
```cpp
items[0] = {mstr(STR_PASSWORDS), 55, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_listView.setOnMenu(onListMenu);
        s_viewsInitialized = true;
    }
    if (!PasswordStore::instance().hasSlotRange()) {
        ui::showToastError(mstr(STR_SLOT_ERROR));
        return nullptr;
    }
    rebuildList();              // Calls ensureListBuffers()
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

**Problem**: `ensureListBuffers()` is called from `rebuildList()` every time the Password menu is accessed. If capacity changes, old buffers are freed and new ones allocated, but if capacity stays the same, buffers are reused. However, there's no mechanism to free buffers when the view is popped from the stack.

## Recommended Fix

Same fix pattern as TOTP module (see related issue #001). Two options:

### Option 1: Free buffers when list view is popped (recommended)

Add an `onExit()` callback mechanism to free list buffers when the Password list view is popped:

```cpp
// In PasswordModule.cpp, modify onListSelect to track exit:
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    if (index - 1 >= s_entryCount) return;
    s_activeSlot = s_entries[index - 1].slot;
    showDetails(s_activeSlot);
}

// Add onViewExit callback for ListView:
static void onListViewExit() {
    freeListBuffers();
}

// In getMenuItems, set the callback:
items[0] = {mstr(STR_PASSWORDS), 55, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_listView.setOnMenu(onListMenu);
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
    uint16_t cap = PasswordStore::instance().capacity();
    if (cap == 0) return false;

    // Always free and reallocate (simpler, no leak)
    freeListBuffers();

    s_listItems = new (std::nothrow) ui::ListItem[cap + 1];
    s_entries = new (std::nothrow) PasswordStore::EntryIndex[cap];
    if (!s_listItems || !s_entries) {
        freeListBuffers();
        s_capacity = 0;
        return false;
    }
    s_capacity = cap;
    return true;
}
```

Then call `freeListBuffers()` in `PasswordModule::stop()` (already done) and optionally in a view exit callback.

### Option 3: Use static buffers with max capacity (most efficient for embedded)

Pre-allocate maximum possible buffers once:

```cpp
// Define max password entries based on slot map
static constexpr uint16_t MAX_PASSWORD_ENTRIES = 100;  // Adjust based on slot map
static ui::ListItem s_listItems[MAX_PASSWORD_ENTRIES + 1];
static PasswordStore::EntryIndex s_entries[MAX_PASSWORD_ENTRIES];
```

This eliminates dynamic allocation entirely.

## References

- Related to: Issue #001 (TOTP module memory leak)
- ESP32-S3 memory: 512KB SRAM + optional PSRAM (typically 2MB or 8MB)
- FreeRTOS heap fragmentation: https://www.freertos.org/FreeRTOS-bridge/FreeRTOS-E-Book/FreeRTOS-Ebook.html#Heap-Management

</content>