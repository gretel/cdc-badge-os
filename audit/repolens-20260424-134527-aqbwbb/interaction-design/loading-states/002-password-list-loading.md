---
title: "[MEDIUM] Missing loading state during password list rebuild"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When the password module list view is rebuilt (e.g., after adding, editing, or deleting an entry), the list is repopulated without any loading indicator. The `rebuildList()` function in `components/mod_password/src/PasswordModule.cpp:405-434` iterates through all TROPIC storage slots synchronously.

**Location**: `components/mod_password/src/PasswordModule.cpp:405-434`

## Impact
- User may perceive the UI as frozen during list rebuild
- No visual feedback that the operation is in progress
- On E-Paper displays with slow refresh, this can be 300-500ms of apparent unresponsiveness
- User might tap multiple times thinking nothing happened

## Evidence
```cpp
// components/mod_password/src/PasswordModule.cpp:405-434
static void rebuildList() {
    if (!PasswordStore::instance().hasSlotRange()) {
        ui::showToastError(mstr(STR_SLOT_ERROR));
        return;
    }
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(PasswordModule::instance().getName(),
                                                               "Password list allocation failed");
        return;
    }
    s_entryCount = 0;
    s_listItems[0] = {mstr(STR_NEW_ENTRY), 0, false, nullptr};

    uint16_t count = 0;
    PasswordStore::instance().listEntriesSorted(s_entries, s_capacity, &count);
    s_entryCount = count;

    for (uint16_t i = 0; i < s_entryCount; i++) {
        uint16_t idx = static_cast<uint16_t>(i + 1);
        s_listItems[idx].label = s_entries[i].title;
        // ... populate list
    }

    s_listView.init(mstr(STR_PASSWORDS), s_listItems, static_cast<uint16_t>(s_entryCount + 1));
    s_listView.setHint(mstr(STR_HINT_LIST));
}
```

Called from `wizardFinish()` at line 522 and `onMenuDeleteConfirm()` at line 677 without any loading indicator:
```cpp
// components/mod_password/src/PasswordModule.cpp:514-530
static void wizardFinish() {
    bool ok = false;
    if (s_wizard.editMode) {
        ok = PasswordStore::instance().updateEntry(s_wizard.editSlot, s_wizard.entry);
    } else {
        ok = PasswordStore::instance().addEntry(s_wizard.entry);
    }

    if (ok) {
        ui::showToastSuccess(mstr(STR_SAVED));
        s_listView.preservePosition();
        rebuildList();  // No loading state shown
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}
```

## Recommended Fix
Add a brief loading indicator before `rebuildList()` is called in both `wizardFinish()` and `onMenuDeleteConfirm()`:

```cpp
static void wizardFinish() {
    // Show loading indicator
    ui::showToastTask("Saving...");

    bool ok = false;
    if (s_wizard.editMode) {
        ok = PasswordStore::instance().updateEntry(s_wizard.editSlot, s_wizard.entry);
    } else {
        ok = PasswordStore::instance().addEntry(s_wizard.entry);
    }

    if (ok) {
        s_listView.preservePosition();
        rebuildList();
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}

static void onMenuDeleteConfirm(void* userData) {
    ui::showToastTask("Deleting...");

    uint16_t slot = *static_cast<uint16_t*>(userData);
    bool ok = PasswordStore::instance().deleteEntry(slot);
    if (ok) {
        s_listView.preservePosition();
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
