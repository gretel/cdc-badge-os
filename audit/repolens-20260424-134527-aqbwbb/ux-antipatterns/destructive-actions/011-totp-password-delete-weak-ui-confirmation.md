---
title: "[MEDIUM] TOTP and Password Delete Confirmation Dialogs Lack Context"
severity: MEDIUM
domain: destructive-actions
lens: ui-delete-flows
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The Password module's delete confirmation dialog uses a generic message without naming the specific entry being deleted. The TOTP module's UI delete flow (accessible via serial command) has no confirmation at all. Both could benefit from showing the account/entry name in the confirmation dialog.

**Evidence:**

### Password Module
- File: `components/mod_password/src/PasswordModule.cpp`
- Lines: 699-704

```cpp
static void onMenuDelete() {
    static uint16_t slot = 0;
    slot = s_activeSlot;
    ui::showConfirm(mstr(STR_CONFIRM_DELETE), onMenuDeleteConfirm, nullptr,
                    ui::ConfirmView::Icon::WARNING, &slot);
}
```

The confirmation message `STR_CONFIRM_DELETE` is defined at line 112 as:
```cpp
i18n.registerTranslation(s_strIdBase + STR_CONFIRM_DELETE, ui::Language::EN, "Delete entry?");
i18n.registerTranslation(s_strIdBase + STR_CONFIRM_DELETE, ui::Language::DE, "Eintrag loeschen?");
```

This generic message does NOT include:
- The name/title of the password entry
- What data will be lost (username, password, URL, notes)

### TOTP Module
- File: `components/mod_totp/src/TotpModule.cpp`
- Lines: 268-282

The TOTP module has NO confirmation dialog for its serial command delete `cmd_totp_del()`.

## Impact
- **Low-Friction Delete**: Generic "Delete entry?" message provides weak cognitive friction
- **Missing Context**: Users don't see which specific entry they're about to delete
- **Inconsistent with Best Practices**: High-impact deletes should name the resource and show consequences

## Recommended Fix

### For Password Module
Update the confirmation dialog to include the entry title:

```cpp
static void onMenuDelete() {
    PasswordEntry entry = {};
    PasswordStore::instance().readEntry(s_activeSlot, &entry);
    
    // Build context-aware confirmation message
    static char confirmMsg[128];
    snprintf(confirmMsg, sizeof(confirmMsg),
             "Delete password entry?\n\n"
             "Title: %s\n"
             "Username: %s\n\n"
             "This action is irreversible.",
             entry.title,
             entry.username[0] ? entry.username : "(none)");
    
    static uint16_t slot = 0;
    slot = s_activeSlot;
    ui::showConfirm(confirmMsg, onMenuDeleteConfirm, nullptr,
                    ui::ConfirmView::Icon::WARNING, &slot);
}
```

### For TOTP Module
Add a confirmation dialog for the UI flow (if implemented) or ensure serial command uses confirmation:

```cpp
// If adding UI delete (in rebuildList or similar)
static void onListDelete(uint16_t index) {
    uint16_t slot = s_listSlots[index - 1];
    TotpAccount account = {};
    TotpStore::instance().readAccount(slot, &account);
    
    static char confirmMsg[128];
    snprintf(confirmMsg, sizeof(confirmMsg),
             "Delete TOTP account?\n\n"
             "Name: %s\n"
             "Issuer: %s\n\n"
             "This will remove the secret.",
             account.name,
             account.issuer[0] ? account.issuer : "(none)");
    
    ui::showConfirm(confirmMsg,
                    [](void* userData) {
                        uint16_t slot = *static_cast<uint16_t*>(userData);
                        TotpStore::instance().deleteAccount(slot);
                        rebuildList();
                    },
                    nullptr,
                    ui::ConfirmView::Icon::WARNING,
                    &slot);
}
```

## References
- ConfirmView API: `components/cdc_views/include/cdc_views/ConfirmView.h`
- FIDO2 module shows credential details in delete (but lacks confirmation)
