---
title: "[LOW] Module list rebuild runs on every menu access"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The TOTP account list (and similar lists in other modules) is **rebuilt from scratch** every time the menu is opened, even if data hasn't changed. This loops through all storage slots and reallocates memory on each access.

**Location:** `components/mod_totp/src/TotpModule.cpp:686-722`, similar patterns in PasswordModule

## Impact
- **Repeated work**: Same data reconstructed multiple times per session
- **Memory churn**: Repeated `new[]`/`delete[]` allocations
- **UI lag**: Menu opening feels slow with many entries

## Evidence
From `components/mod_totp/src/TotpModule.cpp:686-722`:
```cpp
static void rebuildList() {
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(TotpModule::instance().getName(),
                                                               "TOTP list allocation failed");
        return;
    }
    s_accountCount = 0;
    s_listItems[0] = {mstr(STR_ADD_ACCOUNT), 0, false, nullptr};

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
        // ... loops through all slots ...
        s_listItems[idx].label = s_listLabels[s_accountCount];
        // ...
        s_accountCount++;
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        TotpStore::instance().moduleId(),
        TotpStore::instance().rmemStart(),
        TotpStore::instance().rmemEnd(),
        cb, nullptr);

    s_listView.init(mstr(STR_TOTP), s_listItems, s_accountCount + 1);
}
```

Called from `getMenuItems()` (every menu open):
```cpp
items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_viewsInitialized = true;
    }
    rebuildList();  // Runs EVERY TIME menu opens!
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

## Recommended Fix
**Memoize list with dirty tracking**:

```cpp
static uint32_t s_lastDataVersion = 0;
static uint32_t getDataVersion() {
    // Return version based on slot count + last modified timestamp
    return TropicStorage::instance().getVersion(TotpStore::instance().moduleId());
}

static void rebuildList() {
    uint32_t currentVersion = getDataVersion();
    if (currentVersion == s_lastDataVersion) {
        return;  // No change, skip rebuild
    }
    
    // ... existing rebuild logic ...
    
    s_lastDataVersion = currentVersion;
}

// Invalidate on data change
void TotpStore::addAccount(...) {
    // ... existing logic ...
    // Mark list dirty
    s_lastDataVersion = 0;
}
```

**Alternative**: Use event-driven invalidation
```cpp
// Subscribe to storage events
auto& storage = TropicStorage::instance();
storage.subscribe(TotpStore::instance().moduleId(), [](uint16_t slot) {
    // Slot changed, invalidate cache
    s_lastDataVersion = 0;
});
```

## References
- TROPIC Storage: [Slot iteration](components/cdc_core/TropicStorage.h)
