---
title: "[MEDIUM] FIDO2 credential delete lacks confirmation dialog"
severity: MEDIUM
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary

The FIDO2 module's credential deletion flow executes immediately when "Delete" is selected from the context menu, without showing a confirmation dialog. This could lead to accidental deletion of important WebAuthn credentials.

**Evidence:**

File: `components/mod_fido2/src/Fido2Ui.cpp`, lines 230-247

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
        rebuildList();
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
            s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
        }
    }
}
```

File: `components/mod_fido2/src/Fido2Ui.cpp`, lines 264-283 (context menu)

```cpp
items[1] = {ui::tr(ui::StringId::DELETE), []() { handleDelete(s_listView ? s_listView->getSelection() : 0); }};
items[2] = {ui::tr(ui::StringId::CANCEL), []() {}};
```

**The flow:**
1. User opens context menu on a credential
2. User selects "Delete"
3. `handleDelete()` is called and immediately deletes the credential
4. No confirmation step

**Comparison with other modules:**
- Password module (line 702-703): Uses `showConfirm()` before delete
- GPG module (line 530-531): Uses `showConfirm()` for reset
- NVS Editor (line 528): Uses `showConfirm()` before entering editor

## Impact

1. **Accidental deletion**: A single mispress can permanently delete a FIDO2 credential
2. **Irreversible loss**: FIDO2 credentials are typically tied to specific services; losing them may require re-registration
3. **Inconsistent UX**: Other destructive actions in the app use confirmation dialogs
4. **Higher stakes**: FIDO2 credentials are more critical than general key-value data (NVS) or password entries

## Recommended Fix

Add a confirmation dialog before deleting the credential:

```cpp
static void handleDelete(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    // Store slot for callback
    static uint8_t slotToBeDeleted;
    slotToBeDeleted = info.slot;

    // Show confirmation dialog
    ui::showConfirm("Delete credential?",
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
        &slotToBeDeleted);
}
```

Or show the credential's RP ID in the confirmation:

```cpp
char msg[96];
snprintf(msg, sizeof(msg), "Delete \"%s\"?\n", info.rp_id);
ui::showConfirm(msg, ...);
```

## References

- Nielsen Norman Group: [Action Confirmation](https://www.nngroup.com/articles/confirmation/) - when to ask before action
- Material Design: [Dialogs](https://material.io/components/dialogs/alert) - confirm destructive actions
- WebAuthn Spec: [Credential Management](https://www.w3.org/TR/webauthn-2/#credential-management) - importance of credential preservation

</content>