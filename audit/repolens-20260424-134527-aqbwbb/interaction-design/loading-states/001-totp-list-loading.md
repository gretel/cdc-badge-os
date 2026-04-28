---
title: "[MEDIUM] Missing loading state during TOTP account list rebuild"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When the TOTP module list view is rebuilt (e.g., after adding or editing an account), the list is repopulated without any loading indicator. The `rebuildList()` function in `components/mod_totp/src/TotpModule.cpp:684-717` iterates through all TROPIC storage slots synchronously, which can take noticeable time on an E-Paper display.

**Location**: `components/mod_totp/src/TotpModule.cpp:684-717`

## Impact
- User may perceive the UI as frozen during list rebuild
- No visual feedback that the operation is in progress
- On E-Paper displays with slow refresh, this can be 300-500ms of apparent unresponsiveness
- User might tap multiple times thinking nothing happened, causing duplicate operations

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:684-717
static void rebuildList() {
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(TotpModule::instance().getName(),
                                                               "TOTP list allocation failed");
        return;
    }
    s_accountCount = 0;
    s_listItems[0] = {mstr(STR_ADD_ACCOUNT), 0, false, nullptr};

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
        (void)user;
        if (s_accountCount >= s_capacity) return;
        uint16_t logical = 0;
        if (!TotpStore::instance().toLogicalSlot(slot, &logical)) return;
        // ... iteration logic
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        TotpStore::instance().moduleId(),
        TotpStore::instance().rmemStart(),
        TotpStore::instance().rmemEnd(),
        cb, nullptr);

    s_listView.init(mstr(STR_TOTP), s_listItems, static_cast<uint16_t>(s_accountCount + 1));
}
```

Called from `wizardFinish()` at line 903 without any loading indicator:
```cpp
if (ok) {
    ui::showToastSuccess(ui::tr(ui::StringId::OK));
    rebuildList();  // No loading state shown
    while (ui::ViewStack::instance().current() != &s_listView &&
           ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}
```

## Recommended Fix
Add a brief toast notification or loading indicator before `rebuildList()` is called:

```cpp
if (ok) {
    ui::showToastInfo(ui::tr(ui::StringId::LOADING), 500);  // Show loading
    rebuildList();
    while (ui::ViewStack::instance().current() != &s_listView &&
           ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}
```

Alternatively, show a task toast at the start of the wizard finish and dismiss it after rebuild:
```cpp
static void wizardFinish() {
    if (strlen(s_wizard.name) == 0 || strlen(s_wizard.secret) == 0) {
        // ... validation
        return;
    }

    // Show loading indicator
    ui::showToastTask("Saving...");

    bool ok = false;
    if (s_wizard.editMode) {
        ok = TotpStore::instance().updateAccount(...);
    } else {
        ok = TotpStore::instance().addAccount(...);
    }

    if (ok) {
        rebuildList();
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}
```

## References
- ToastView already supports task indicator: `showToastTask()` in `components/cdc_views/src/ToastView.cpp`
- E-Paper display refresh times: ~350ms for partial refresh based on code constants in `SleepManager.cpp`
