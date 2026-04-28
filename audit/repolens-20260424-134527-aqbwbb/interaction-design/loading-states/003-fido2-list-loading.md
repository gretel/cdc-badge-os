---
title: "[MEDIUM] Missing loading state during FIDO2 credential list rebuild"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When the FIDO2 credential list is rebuilt (e.g., after deleting a credential), the list is repopulated without any loading indicator. The `rebuildList()` function in `components/mod_fido2/src/Fido2Ui.cpp:117-167` iterates through all FIDO2 credentials synchronously.

**Location**: `components/mod_fido2/src/Fido2Ui.cpp:117-167`

## Impact
- User may perceive the UI as frozen during list rebuild
- No visual feedback that the operation is in progress
- On E-Paper displays with slow refresh, this can be 300-500ms of apparent unresponsiveness
- During user-presence prompts (approve/deny flow), the UI should clearly indicate the operation is in progress

## Evidence
```cpp
// components/mod_fido2/src/Fido2Ui.cpp:117-167
static void rebuildList() {
    s_listCount = 0;
    uint8_t count = fido2_get_credential_count();
    if (count == 0) {
        // Show "No entries" placeholder
        s_listItems[0].label = mstr(STR_NO_ENTRIES);
        s_listItems[0].userData = nullptr;
        s_listItems[0].icon = 0;
        s_listItems[0].iconDisabled = true;
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, 1);
            s_listView->setHint(ui::tr(ui::StringId::HINT_BACK));
        }
        return;
    }

    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        s_sortMap[i] = i;
        fido2_credential_info_t info = {};
        if (fido2_get_credential_info(i, &info)) {
            // ... populate list
        }
    }
    // ... sort and populate s_listItems
    if (s_listView) {
        s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
        s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
    }
}
```

Called from `handleDelete()` at line 227:
```cpp
static void handleDelete(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    if (fido2_delete_credential(info.slot)) {
        rebuildList();  // No loading state shown
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
            s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
        }
    }
}
```

Also called from `promptComplete()` at line 315 after user-presence approval:
```cpp
if (result == FIDO2_UP_APPROVED &&
    s_promptAction == FIDO2_ACTION_REGISTER &&
    s_promptReturnView == s_listView) {
    rebuildList();  // No loading state shown
}
```

## Recommended Fix
Add a brief toast notification before `rebuildList()` is called:

```cpp
static void handleDelete(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    ui::showToastTask("Deleting...");  // Show loading
    if (fido2_delete_credential(info.slot)) {
        rebuildList();
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
            s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
        }
    }
}

static void promptComplete(fido2_user_presence_result_t result) {
    s_promptActive = false;

    if (result == FIDO2_UP_APPROVED) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED), 2000);
    }

    restoreView();

    if (result == FIDO2_UP_APPROVED &&
        s_promptAction == FIDO2_ACTION_REGISTER &&
        s_promptReturnView == s_listView) {
        ui::showToastTask("Saving...");  // Show loading
        rebuildList();
    }

    // ... rest of function
}
```

## References
- ToastView already supports task indicator: `showToastTask()` in `components/cdc_views/src/ToastView.cpp`
- FIDO2 user-presence flow in `components/mod_fido2/src/Fido2Ui.cpp:450-593`
