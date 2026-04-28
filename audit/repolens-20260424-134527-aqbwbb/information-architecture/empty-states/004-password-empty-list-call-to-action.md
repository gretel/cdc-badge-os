---
title: "[MEDIUM] Password list empty state lacks contextual help beyond first item"
severity: MEDIUM
domain: mod_password
lens: empty-states
labels:
  - "empty-state"
  - "password"
  - "call-to-action"
---

## Summary
The Password module list view in `components/mod_password/src/PasswordModule.cpp` shows only "New Entry" as the first item when no password entries exist. While this provides a basic call-to-action, it lacks contextual help explaining what fields are needed or how to get started for first-time users.

**Location:** `components/mod_password/src/PasswordModule.cpp:404-435` (rebuildList function)

## Impact
First-time password manager users seeing an empty list with just "New Entry" may:
- Not know what information to prepare before adding an entry
- Be confused about optional vs required fields
- Need to search elsewhere for documentation on the password vault feature
- Abandon the feature due to uncertainty

## Evidence
In `rebuildList()` (lines 404-435):
```cpp
static void rebuildList() {
    if (!PasswordStore::instance().hasSlotRange()) {
        ui::showToastError(mstr(STR_SLOT_ERROR));
        return;
    }
    if (!ensureListBuffers()) {
        // ... error handling ...
        return;
    }
    s_entryCount = 0;
    s_listItems[0] = {mstr(STR_NEW_ENTRY), 0, false, nullptr};  // <-- Only item

    uint16_t count = 0;
    PasswordStore::instance().listEntriesSorted(s_entries, s_capacity, &count);
    s_entryCount = count;

    for (uint16_t i = 0; i < s_entryCount; i++) {
        // ... populates entries ...
    }

    s_listView.init(mstr(STR_PASSWORDS), s_listItems, static_cast<uint16_t>(s_entryCount + 1));
    s_listView.setHint(mstr(STR_HINT_LIST));
}
```

When `s_entryCount` is 0, the list has exactly 1 item: "New Entry".

The selection callback (line 507-529):
```cpp
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();  // <-- Just starts wizard, no info first
        return;
    }
    // ...
}
```

Clicking "New Entry" immediately starts the wizard without any explanation.

## Recommended Fix
Add an optional info/help action for first-time users, similar to the TOTP module fix.

**Recommended approach: Show info view on first "New Entry" select**
Modify `onListSelect()` to check if it's the first time and show a brief explanation:

```cpp
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        if (s_entryCount == 0) {
            // Show brief help first
            static char helpText[256];
            snprintf(helpText, sizeof(helpText),
                "Add a password entry with:\n"
                "- Title (e.g., GitHub)\n"
                "- Username\n"
                "- Password\n"
                "- URL (optional)\n"
                "- TOTP Slot (optional)\n"
                "- Notes (optional)\n\n"
                "Press Y to start, N to go back.");
            showInfo(mstr(STR_NEW_ENTRY), helpText);
        } else {
            wizardStart();
        }
        return;
    }
    // ...
}
```

Add translation strings for the help text in both English and German.

## References
- Password module: `components/mod_password/`
- ListView usage: `components/cdc_views/ListView.h`
- InfoView for help: `components/cdc_views/InfoView.h`
- Similar pattern in TOTP: `components/mod_totp/src/TotpModule.cpp:722-734`
