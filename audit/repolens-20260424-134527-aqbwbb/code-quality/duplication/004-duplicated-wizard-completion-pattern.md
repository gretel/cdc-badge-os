---
title: "[MEDIUM] Duplicated wizard completion and list navigation patterns"
severity: MEDIUM
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

The wizard completion logic (`wizardFinish`) and list navigation patterns (`onListSelect`, `rebuildList`) are duplicated across `mod_totp` and `mod_password` modules with nearly identical control flow and UI handling code.

### Code Comparison

**`wizardFinish` in TotpModule.cpp (lines 870-914):**
```cpp
static void wizardFinish() {
    if (strlen(s_wizard.name) == 0 || strlen(s_wizard.secret) == 0) {
        ui::showToastError(mstr(STR_INVALID_INPUT));
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
        return;
    }

    bool ok = false;
    if (s_wizard.editMode) {
        ok = TotpStore::instance().updateAccount(...);
    } else {
        ok = TotpStore::instance().addAccount(...);
    }

    if (ok) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
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

**`wizardFinish` in PasswordModule.cpp (lines 517-539):**
```cpp
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

**`onListSelect` in TotpModule.cpp (lines 727-739):**
```cpp
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    if (index - 1 >= s_accountCount) return;

    uint16_t slot = s_listSlots[index - 1];
    const char* name = s_listLabels[index - 1];
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);
}
```

**`onListSelect` in PasswordModule.cpp (lines 733-745):**
```cpp
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    if (index - 1 >= s_entryCount) return;
    s_activeSlot = s_entries[index - 1].slot;
    showDetails(s_activeSlot);
}
```

**Common Pattern:**
1. Check if index 0 (new entry) → call `wizardStart()`
2. Check bounds → return if invalid
3. Extract data from arrays
4. Push detail view

## Impact

- **Maintenance Burden:** Changes to wizard completion or list navigation must be applied in multiple places.
- **Inconsistency Risk:** Different modules might handle errors or success differently over time.
- **Code Bloat:** Approximately 100+ lines of duplicated control flow logic.
- **Testing Overhead:** Same logic needs to be tested in multiple modules.

## Evidence

**Files Affected:**
- `components/mod_totp/src/TotpModule.cpp`
- `components/mod_password/src/PasswordModule.cpp`

**Duplicated Patterns:**
1. **Wizard completion flow** (lines 870-914 in TOTP, lines 517-539 in Password):
   - Validation check
   - Edit vs. add branching
   - Success/error toast
   - View stack pop loop
   - List rebuild

2. **List selection handling** (lines 727-739 in TOTP, lines 733-745 in Password):
   - Index 0 → wizardStart
   - Bounds check
   - Data extraction
   - View push

3. **View stack navigation** (repeated in multiple functions):
   ```cpp
   while (ui::ViewStack::instance().current() != &s_listView &&
          ui::ViewStack::instance().depth() > 1) {
       ui::ViewStack::instance().pop();
   }
   ```

## Recommended Fix

### Create a ModuleListHelper Class

Create `components/cdc_ui/include/cdc_ui/ModuleListHelper.h`:

```cpp
#pragma once
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ListView.h"

namespace cdc::ui {

/**
 * \brief Helper for common list-view module patterns.
 */
class ModuleListHelper {
public:
    /**
     * \brief Pop views back to anchor view.
     * \param anchor View to return to.
     */
    static void popToAnchor(IView* anchor) {
        auto& stack = ViewStack::instance();
        while (stack.current() != anchor && stack.depth() > 1) {
            stack.pop();
        }
    }

    /**
     * \brief Handle list selection with "New Entry" at index 0.
     * \param index Selected index.
     * \param count Item count (excluding "New Entry").
     * \param onNew Callback for new entry.
     * \param onSelect Callback for existing item.
     */
    template<typename OnNew, typename OnSelect>
    static void handleSelect(uint16_t index, uint16_t count,
                             OnNew onNew, OnSelect onSelect) {
        if (index == 0) {
            onNew();
            return;
        }
        if (index - 1 >= count) return;
        onSelect(index - 1);
    }
};

} // namespace cdc::ui
```

### Create a WizardBase Class

Create `components/cdc_ui/include/cdc_ui/WizardBase.h`:

```cpp
#pragma once
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ToastView.h"
#include <cstring>

namespace cdc::ui {

/**
 * \brief Base class for wizard completion logic.
 */
class WizardBase {
public:
    /**
     * \brief Complete wizard with validation and persistence.
     * \param isValid Validation function.
     * \param save Save function returning success.
     * \param anchor View to return to.
     * \param rebuild Rebuild list function.
     * \param successMsg Success message (optional).
     */
    static void finish(
        std::function<bool()> isValid,
        std::function<bool()> save,
        IView* anchor,
        std::function<void()> rebuild,
        const char* successMsg = nullptr
    ) {
        if (isValid()) {
            bool ok = save();
            if (ok) {
                if (successMsg) {
                    showToastSuccess(successMsg);
                } else {
                    showToastSuccess(tr(StringId::OK));
                }
                rebuild();
                popToAnchor(anchor);
            } else {
                showToastError(tr(StringId::FAILED));
            }
        } else {
            // Validation failed - already handled by caller
        }
    }

    /**
     * \brief Pop views back to anchor.
     * \param anchor View to return to.
     */
    static void popToAnchor(IView* anchor) {
        ModuleListHelper::popToAnchor(anchor);
    }
};

} // namespace cdc::ui
```

### Implementation Steps

1. Create helper headers in `components/cdc_ui/include/cdc_ui/`
2. Update `TotpModule.cpp`:
   - Add `#include "cdc_ui/ModuleListHelper.h"`
   - Add `#include "cdc_ui/WizardBase.h"`
   - Replace `onListSelect` with `ModuleListHelper::handleSelect()`
   - Replace `wizardFinish` with `WizardBase::finish()`
   - Replace view stack pop loop with `WizardBase::popToAnchor()`
3. Update `PasswordModule.cpp`:
   - Same changes as TOTP

## References

- [Template Methods Pattern](https://en.wikipedia.org/wiki/Template_method_pattern)
- [Strategy Pattern](https://en.wikipedia.org/wiki/Strategy_pattern)
- Related findings: #1 (token parsing), #2 (UI helpers), #3 (wizard state)
