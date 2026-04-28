---
title: "[LOW] UI views initialized eagerly instead of lazily"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
UI views (ListView, T9InputView, etc.) are constructed and initialized **at module startup** even though many views may never be accessed during a typical session. This wastes initialization time and memory.

**Location:** `components/mod_totp/src/TotpModule.cpp:544-556`, `components/mod_password/src/PasswordModule.cpp`

## Impact
- **Wasted init time**: Views for rarely-used features initialized on every boot
- **Memory overhead**: Each view ~100-500 bytes static allocation
- **Scalability**: More features = longer boot time

## Evidence
From `components/mod_totp/src/TotpModule.cpp:544-556`:
```cpp
/** \brief Static view instances (no dynamic allocation, no leaks). */
static ui::ListView s_listView;
static ui::T9InputView s_t9Input;
static ui::ListView s_digitsMenu;
static ui::ListView s_algoMenu;
static ui::ListView s_periodMenu;
static TotpCodeView s_codeView;
static bool s_viewsInitialized = false;
```

From `TotpModule::getMenuItems()`:
```cpp
items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_viewsInitialized = true;
    }
    rebuildList();  // Rebuilt on EVERY menu open!
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

Views are initialized once at menu access, but `rebuildList()` runs every time:
```cpp
static void rebuildList() {
    if (!ensureListBuffers()) {
        // ...
    }
    // ... loops through all slots, allocates memory ...
    s_listView.init(mstr(STR_TOTP), s_listItems, s_accountCount + 1);
}
```

## Recommended Fix
**Lazy view construction**: Only create views when first accessed.

**Implementation:**
```cpp
// Change from static instances to pointers
static ui::ListView* s_listView = nullptr;
static ui::T9InputView* s_t9Input = nullptr;

// Lazy getter
static ui::ListView* getListView() {
    if (!s_listView) {
        s_listView = new ui::ListView();
        s_listView->setOnSelect(onListSelect);
    }
    return s_listView;
}

// Memoize list data
static bool s_listBuilt = false;
static void rebuildList() {
    if (s_listBuilt) return;  // Cache result
    
    // ... build list ...
    s_listBuilt = true;
}

// Invalidate on data change
void onAccountAdded() {
    s_listBuilt = false;  // Rebuild next time
}
```

**Alternative**: Use view factory pattern
```cpp
class ViewFactory {
    std::map<std::string, std::shared_ptr<ui::IView>> views_;
    
    ui::IView* getView(const char* name) {
        if (!views_.count(name)) {
            views_[name] = createView(name);  // Lazy
        }
        return views_[name].get();
    }
};
```

## References
- E-Paper display: [Memory constraints](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/ledc.html) - Limited RAM for framebuffers
