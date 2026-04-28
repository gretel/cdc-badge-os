---
title: "[MEDIUM] TOTP list empty state lacks contextual help beyond first item"
severity: MEDIUM
domain: mod_totp
lens: empty-states
labels:
  - "empty-state"
  - "totp"
  - "call-to-action"
---

## Summary
The TOTP module list view in `components/mod_totp/src/TotpModule.cpp` shows only "Add Account" as the first item when no accounts exist. While this provides a basic call-to-action, it lacks contextual help explaining what TOTP is or how to get started for first-time users.

**Location:** `components/mod_totp/src/TotpModule.cpp:686-720` (rebuildList function)

## Impact
First-time TOTP users seeing an empty list with just "Add Account" may:
- Not understand what information they need to add an account
- Not know where to get the secret key
- Be unsure if they should use the TOTP feature at all
- Need to search elsewhere for documentation

## Evidence
In `rebuildList()` (lines 686-720):
```cpp
static void rebuildList() {
    if (!ensureListBuffers()) {
        // ... error handling ...
        return;
    }
    s_accountCount = 0;
    s_listItems[0] = {mstr(STR_ADD_ACCOUNT), 0, false, nullptr};  // <-- Only item

    auto cb = [](uint16_t slot, ...) {
        // ... populates accounts ...
    };

    cdc::core::TropicStorage::instance().forEachSlot(...);

    s_listView.init(mstr(STR_TOTP), s_listItems, static_cast<uint16_t>(s_accountCount + 1));
}
```

When `s_accountCount` is 0, the list has exactly 1 item: "Add Account".

The menu callback (line 722-734):
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

Clicking "Add Account" immediately starts the wizard without any explanation.

## Recommended Fix
Add an optional info/help action for first-time users. Two approaches:

**Option 1: Add a second item when list is empty**
```cpp
s_listItems[0] = {mstr(STR_ADD_ACCOUNT), 0, false, nullptr};
if (s_accountCount == 0) {
    s_listItems[1] = {tr(StringId::HELP), 0, false, nullptr};  // Or "What is TOTP?"
    s_listView.init(mstr(STR_TOTP), s_listItems, 2);
} else {
    s_listView.init(mstr(STR_TOTP), s_listItems, static_cast<uint16_t>(s_accountCount + 1));
}
```

**Option 2: Show info view on first "Add Account" select**
Modify `onListSelect()` to check if it's the first time and show a brief explanation:
```cpp
static void onListSelect(uint16_t index, void* userData) {
    if (index == 0) {
        if (s_accountCount == 0) {
            // Show brief help first
            showInfo(mstr(STR_ADD_ACCOUNT), 
                "Add a TOTP account by entering:\n"
                "- Account name (e.g., john@example.com)\n"
                "- Secret key (Base32 from service)\n"
                "- Optional issuer name\n\n"
                "Press Y to start, N to go back.");
        } else {
            wizardStart();
        }
        return;
    }
    // ...
}
```

## References
- TOTP module: `components/mod_totp/`
- ListView usage: `components/cdc_views/ListView.h`
- InfoView for help: `components/cdc_views/InfoView.h`
