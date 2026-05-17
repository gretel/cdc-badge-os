---
title: "[LOW] Duplicated view navigation patterns across TOTP and Password modules"
severity: LOW
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

Multiple modules implement identical view stack navigation patterns for returning to the list view after wizard completion. The same `while` loop pattern appears in at least 2 modules:

- **`mod_totp/src/TotpModule.cpp`** - Lines 875-879, 907-911
- **`mod_password/src/PasswordModule.cpp`** - Lines ~520-524, ~535-539

### Code Comparison

**Identical Pattern (appears 4+ times across 2 modules):**

```cpp
// TotpModule.cpp (lines 875-879)
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}

// TotpModule.cpp (lines 907-911) - Identical
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}

// PasswordModule.cpp - Identical pattern, different locations
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}
```

**Additional Duplicated Navigation Patterns:**

```cpp
// Push detail view pattern - appears in both modules
s_codeView.init(slot, name);
ui::ViewStack::instance().push(&s_codeView);

// Toast success pattern - appears in both modules
ui::showToastSuccess(ui::tr(ui::StringId::OK));
// or
ui::showToastSuccess(mstr(STR_SAVED));

// Toast error pattern - appears in both modules
ui::showToastError(ui::tr(ui::StringId::FAILED));
// or
ui::showToastError(mstr(STR_INVALID_INPUT));
```

## Impact

- **Maintenance Burden:** Changes to navigation logic must be applied in multiple places.
- **Code Bloat:** Approximately 20-30 lines of duplicated navigation code.
- **Inconsistency Risk:** One module might be updated with improved navigation while the other is forgotten.
- **Testing:** Same navigation logic needs testing in multiple modules.

## Evidence

**Files Affected:**
- `components/mod_totp/src/TotpModule.cpp` (lines 875-879, 907-911)
- `components/mod_password/src/PasswordModule.cpp` (similar locations)

**Duplicated Patterns:**
1. **View stack pop loop** - 4 occurrences across 2 modules
2. **Toast success/error calls** - Multiple occurrences in both modules
3. **View push patterns** - Similar patterns for pushing detail views

**Specific Locations:**

```cpp
// TotpModule.cpp
// Line 875-879: wizardFinish validation failure
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}

// Line 907-911: wizardFinish success
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}

// PasswordModule.cpp - Similar pattern at wizardFinish
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}
```

## Recommended Fix

### Create a View Navigation Helper

Create `components/cdc_ui/include/cdc_ui/ViewNavigation.h`:

```cpp
#pragma once
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/IView.h"

namespace cdc::ui {

/**
 * \brief Helper class for common view navigation patterns.
 */
class ViewNavigation {
public:
    /**
     * \brief Pop views back to anchor view.
     * \param anchor View to return to.
     */
    static void popTo(IView* anchor) {
        auto& stack = ViewStack::instance();
        while (stack.current() != anchor && stack.depth() > 1) {
            stack.pop();
        }
    }

    /**
     * \brief Pop views back to anchor view with depth limit.
     * \param anchor View to return to.
     * \param maxPops Maximum number of views to pop.
     */
    static void popToLimited(IView* anchor, uint16_t maxPops) {
        auto& stack = ViewStack::instance();
        uint16_t count = 0;
        while (stack.current() != anchor && stack.depth() > 1 && count < maxPops) {
            stack.pop();
            count++;
        }
    }

    /**
     * \brief Push view and clear forward stack.
     * \param view View to push.
     * \param clearForward Clear forward stack if true.
     */
    static void pushClear(IView* view, bool clearForward = true) {
        auto& stack = ViewStack::instance();
        if (clearForward) {
            while (stack.depth() > 1) {
                stack.pop();
            }
        }
        stack.push(view);
    }
};

} // namespace cdc::ui
```

### Implementation Steps

1. **Create `components/cdc_ui/include/cdc_ui/ViewNavigation.h`** with the helper class above.

2. **Update `TotpModule.cpp`:**
   - Add `#include "cdc_ui/ViewNavigation.h"`
   - Replace view stack pop loops with `ui::ViewNavigation::popTo(&s_listView)`

3. **Update `PasswordModule.cpp`:**
   - Same changes as TOTP module

4. **Future modules** can use the same helper for consistent navigation.

### Alternative: Inline Helper Function

For simpler usage, create an inline function:

```cpp
// components/cdc_ui/include/cdc_ui/ViewNavigation.h
namespace cdc::ui {

/**
 * \brief Pop views back to anchor view.
 * \param anchor View to return to.
 */
inline void popToAnchor(ui::IView* anchor) {
    auto& stack = ui::ViewStack::instance();
    while (stack.current() != anchor && stack.depth() > 1) {
        stack.pop();
    }
}

} // namespace cdc::ui
```

## References

- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [Strategy Pattern](https://en.wikipedia.org/wiki/Strategy_pattern)
- Related findings: #1 (token parsing), #2 (UI helpers), #3 (wizard state), #4 (wizard completion), #5 (lifecycle boilerplate), #6 (storage layer patterns)
