---
title: "[MEDIUM] FIDO2 Credential Delete Lacks Confirmation Dialog"
severity: MEDIUM
domain: destructive-actions
lens: ui-delete-flows
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The FIDO2 UI delete flow executes immediate deletion of credentials without a confirmation dialog. The `handleDelete()` function in `components/mod_fido2/src/Fido2Ui.cpp:225-239` directly calls `fido2_delete_credential()` when a user selects "Delete" from the context menu, with no intermediate confirmation step.

**Evidence:**
- File: `components/mod_fido2/src/Fido2Ui.cpp`
- Lines: 225-239
- Function: `handleDelete(uint16_t display_index)`

```cpp
static void handleDelete(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    if (fido2_delete_credential(info.slot)) {  // Immediate delete!
        rebuildList();
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
            s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
        }
    }
}
```

The context menu at line 265-275 shows "Delete" as an option that directly calls `handleDelete()`:
```cpp
items[1] = {ui::tr(ui::StringId::DELETE), []() { handleDelete(s_listView ? s_listView->getSelection() : 0); }};
```

Compare to Password module which shows a confirmation dialog before deleting:
```cpp
static void onMenuDelete() {
    static uint16_t slot = 0;
    slot = s_activeSlot;
    ui::showConfirm(mstr(STR_CONFIRM_DELETE), onMenuDeleteConfirm, nullptr,
                    ui::ConfirmView::Icon::WARNING, &slot);
}
```

## Impact
- **Data Loss Risk**: FIDO2 credentials can be accidentally deleted with a single menu selection
- **High Impact**: FIDO2 credentials are used for WebAuthn authentication; losing them may lock users out of accounts
- **No Recovery**: Credentials stored in secure element slots are permanently erased; re-registration requires access to the relying party
- **Inconsistent UX**: Other modules (Password) use confirmation dialogs for delete operations

## Recommended Fix
Add a confirmation dialog before deleting FIDO2 credentials. Modify `handleDelete()` to first show a confirmation dialog with the credential details:

```cpp
static void handleDelete(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    // Build confirmation message with credential details
    static char confirmMsg[200];
    snprintf(confirmMsg, sizeof(confirmMsg),
             "Delete FIDO2 credential?\n\n"
             "Relying Party: %s\n"
             "User: %s\n\n"
             "This action is irreversible.",
             info.rp_id,
             strlen(info.user_name) > 0 ? info.user_name : "(none)");

    // Store slot for callback
    static uint8_t deleteSlot;
    deleteSlot = info.slot;

    // Show confirmation dialog
    ui::showConfirm(confirmMsg,
                    [](void* userData) {
                        uint8_t slot = *static_cast<uint8_t*>(userData);
                        if (fido2_delete_credential(slot)) {
                            rebuildList();
                            if (s_listView) {
                                s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
                                s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
                            }
                        }
                    },
                    nullptr,
                    ui::ConfirmView::Icon::WARNING,
                    &deleteSlot);
}
```

## References
- Password module confirmation pattern: `components/mod_password/src/PasswordModule.cpp:699`
- ConfirmView API: `components/cdc_views/include/cdc_views/ConfirmView.h`
