---
title: "[MEDIUM] TOTP add/edit wizard has no loading feedback during storage operations"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
The TOTP module's add/edit account wizard (`components/mod_totp/src/TotpModule.cpp:714-910`) performs storage operations (Tropic secure element writes) that can take several seconds, but provides **no visual feedback** during these operations. The display may appear frozen while `addAccount()`, `updateAccount()`, or `deleteAccount()` execute.

**Evidence:**
- `wizardFinish()` (line 868-910) calls `TotpStore::addAccount()` or `updateAccount()` which write to secure element
- `onListSelect()` (line 721-731) calls `TotpStore::generateCode()` which may involve secure element reads
- No loading state or toast is shown during these operations
- The Tropic secure element operations can take 1-3 seconds per operation

## Impact
**User Experience:** Users press "Save" in the wizard and see no feedback for 1-3 seconds. They may think the button didn't register and press again, potentially causing duplicate operations or confusion.

**Technical:** Secure element operations are inherently slow but predictable. Users should be informed that work is in progress.

## Evidence
**File: `components/mod_totp/src/TotpModule.cpp`**

```cpp
// Line 868-910: wizardFinish() - No loading indicator during storage
static void wizardFinish() {
    if (strlen(s_wizard.name) == 0 || strlen(s_wizard.secret) == 0) {
        ui::showToastError(mstr(STR_INVALID_INPUT));
        // ...
        return;
    }

    bool ok = false;
    if (s_wizard.editMode) {
        ok = TotpStore::instance().updateAccount(  // BLOCKING: Secure element write
            s_wizard.editSlot,
            s_wizard.name,
            // ...
        );
    } else {
        ok = TotpStore::instance().addAccount(  // BLOCKING: Secure element write
            s_wizard.name,
            // ...
        );
    }

    if (ok) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
        rebuildList();
        // ...
    }
}
```

**File: `components/mod_totp/src/TotpStore.h`** (typical storage operation signature)
- `addAccount()` and `updateAccount()` perform secure element writes
- These operations can take 1-3 seconds depending on TROPIC01 performance

## Recommended Fix
Add a loading toast before storage operations:

```cpp
static void wizardFinish() {
    // Show loading indicator
    ui::showToastTask("Saving...");
    ui::ViewStack::instance().render();  // Force render
    
    if (strlen(s_wizard.name) == 0 || strlen(s_wizard.secret) == 0) {
        ui::ViewStack::instance().hideModal();
        ui::showToastError(mstr(STR_INVALID_INPUT));
        // ...
        return;
    }

    bool ok = false;
    if (s_wizard.editMode) {
        ok = TotpStore::instance().updateAccount(...);
    } else {
        ok = TotpStore::instance().addAccount(...);
    }

    // Dismiss loading, show result
    ui::ViewStack::instance().hideModal();
    if (ok) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
        // ...
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}
```

## References
- Secure element operations typically take 1-3 seconds
- ToastView::Icon::TASK is designed for this use case
