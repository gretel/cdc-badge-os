---
title: "[MEDIUM] Duplicated module UI helper patterns across TOTP and Password modules"
severity: MEDIUM
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

Multiple helper functions and patterns are duplicated between `mod_totp` and `mod_password` modules for managing UI state and wizard flows. These include:

1. **`pushT9WizardStep`** - Identical function in both modules (lines 580-585 in TotpModule.cpp, lines 547-552 in PasswordModule.cpp)
2. **`freeListBuffers`** - Similar buffer cleanup functions
3. **`ensureListBuffers`** - Similar dynamic allocation patterns
4. **i18n `registerStrings`** - Identical registration pattern with module-specific strings

### Code Comparison

**`pushT9WizardStep` in TotpModule.cpp (lines 580-585):**
```cpp
static void pushT9WizardStep(const char* title, const char* initialText,
                              uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}
```

**`pushT9WizardStep` in PasswordModule.cpp (lines 547-552):**
```cpp
static void pushT9WizardStep(const char* title, const char* initialText,
                             uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}
```

**`freeListBuffers` in TotpModule.cpp (lines 591-600):**
```cpp
static void freeListBuffers() {
    delete[] s_listItems;
    delete[] s_listLabels;
    delete[] s_listSlots;
    s_listItems = nullptr;
    s_listLabels = nullptr;
    s_listSlots = nullptr;
    s_capacity = 0;
    s_accountCount = 0;
}
```

**`freeListBuffers` in PasswordModule.cpp (lines 367-375):**
```cpp
static void freeListBuffers() {
    delete[] s_listItems;
    delete[] s_entries;
    s_listItems = nullptr;
    s_entries = nullptr;
    s_capacity = 0;
    s_entryCount = 0;
}
```

**i18n Registration Pattern (TotpModule.cpp lines 54-93, PasswordModule.cpp lines 63-115):**
Both modules use the exact same pattern:
```cpp
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_<name>", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }
    // ... register EN translations ...
    // ... register DE translations ...
    LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
}
```

## Impact

- **Maintenance Burden:** UI helper logic must be maintained in multiple places.
- **Inconsistency Risk:** If one module's `pushT9WizardStep` is updated, the other might be forgotten.
- **Code Bloat:** Approximately 100+ lines of duplicated UI helper code.
- **Onboarding Complexity:** New developers must learn multiple implementations of the same pattern.

## Evidence

**Files Affected:**
- `components/mod_totp/src/TotpModule.cpp`
- `components/mod_password/src/PasswordModule.cpp`

**Duplicated Functions:**
- `pushT9WizardStep`: Used 5 times in TOTP, 7 times in Password
- `freeListBuffers`: Used in module `stop()` method
- `ensureListBuffers`: Used in `rebuildList()` method
- `registerStrings`: Called in module `init()` method

## Recommended Fix

### Option 1: Create a Shared UI Helper Header

Create `components/cdc_ui/include/cdc_ui/ModuleHelpers.h`:

```cpp
#pragma once
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/T9InputView.h"
#include "cdc_log.h"

namespace cdc::ui {

/**
 * \brief Helper struct for managing module i18n string registration.
 */
struct ModuleStrings {
    uint16_t baseId;
    uint16_t count;
    const char* moduleName;

    /**
     * \brief Initialize and register module strings.
     * \param moduleName Module identifier (e.g., "mod_totp").
     * \param count Number of string entries.
     * \param logTag Logging tag for messages.
     */
    void init(const char* moduleName, uint16_t count, const char* logTag) {
        auto& i18n = ui::I18n::instance();
        baseId = i18n.registerModule(moduleName, count);
        if (baseId == 0) {
            LOG_E(logTag, "Failed to register i18n strings");
            return;
        }
        count = count;
    }

    /**
     * \brief Resolve string by offset.
     * \param offset Module string-table offset.
     * \return Translated string pointer.
     */
    const char* get(uint16_t offset) const {
        return ui::tr(baseId + offset);
    }
};

/**
 * \brief Pushes a configured T9 input step for wizard flow.
 * \param t9Input T9InputView instance to configure.
 * \param title Step title.
 * \param initialText Initial input text.
 * \param maxLen Maximum accepted text length.
 * \param onSave Save callback for this step.
 */
inline void pushT9WizardStep(ui::T9InputView& t9Input, const char* title,
                             const char* initialText, uint16_t maxLen,
                             ui::T9InputView::SaveCallback onSave) {
    t9Input.init(title, initialText, maxLen);
    t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&t9Input);
}

} // namespace cdc::ui
```

### Option 2: Create a Base Module Class

Create a `ModuleBase` class in `cdc_ui` that provides common UI helper functionality, which modules can inherit from or compose.

### Implementation Steps

1. Create the shared helper header (Option 1 recommended for simplicity)
2. Update `TotpModule.cpp`:
   - Add `#include "cdc_ui/ModuleHelpers.h"`
   - Replace `pushT9WizardStep` call to use `cdc::ui::pushT9WizardStep(s_t9Input, ...)`
   - Remove local `pushT9WizardStep` definition
   - Use `cdc::ui::ModuleStrings` for i18n management
3. Update `PasswordModule.cpp`:
   - Same changes as TOTP module

## References

- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [Strategy Pattern for Module Helpers](https://en.wikipedia.org/wiki/Strategy_pattern)
- Related finding: #1 (Duplicated token-parsing helpers)
