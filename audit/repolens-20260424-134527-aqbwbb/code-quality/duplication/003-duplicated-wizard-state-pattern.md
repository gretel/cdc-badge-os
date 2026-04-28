---
title: "[LOW] Duplicated wizard state management pattern across modules"
severity: LOW
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

Multiple modules implement nearly identical wizard state management patterns with `WizardState` structs and associated flow control. While the data fields differ based on module needs, the overall structure and flow are duplicated:

- **`mod_totp/src/TotpModule.cpp`** (lines 557-568)
- **`mod_password/src/PasswordModule.cpp`** (lines 351-356)
- **`mod_gpg/src/GpgModule.cpp`** (lines 216-220)

### Code Comparison

**TotpModule.cpp WizardState:**
```cpp
struct WizardState {
    char name[TotpStore::NAME_LEN + 1];
    char secret[128];
    char issuer[TotpStore::ISSUER_LEN + 1];
    uint8_t digits;
    uint8_t algorithm;
    uint32_t period;
    bool editMode;
    uint16_t editSlot;
};

static WizardState s_wizard = {};
```

**PasswordModule.cpp WizardState:**
```cpp
struct WizardState {
    PasswordEntry entry;
    bool editMode;
    uint16_t editSlot;
};

static WizardState s_wizard = {};
```

**GpgModule.cpp WizardState:**
```cpp
struct WizardState {
    char name[64];
    char email[64];
    uint8_t curve;
};

static WizardState s_wizard = {};
```

Additionally, all three modules use identical patterns for:
- Static view instances (e.g., `static ui::ListView s_listView;`, `static ui::T9InputView s_t9Input;`)
- Boolean flag for initialization (e.g., `static bool s_viewsInitialized = false;`)
- Wizard flow callbacks (onWizardName, onWizardSecret, etc.)

## Impact

- **Maintenance Burden:** Similar changes to wizard flow must be applied across multiple modules.
- **Inconsistency Risk:** Different modules might implement wizard features differently over time.
- **Memory Usage:** Each module maintains its own static wizard state, even though the pattern is identical.
- **Code Bloat:** Approximately 60-80 lines of duplicated struct definitions and state management per module.

## Evidence

**Files Affected:**
- `components/mod_totp/src/TotpModule.cpp`
- `components/mod_password/src/PasswordModule.cpp`
- `components/mod_gpg/src/GpgModule.cpp`

**Common Patterns:**
1. Static `WizardState` struct definition
2. Static `s_wizard` instance
3. Static view instances (ListView, T9InputView, etc.)
4. `s_viewsInitialized` flag
5. Sequential wizard callback chain (e.g., `onWizardName` → `onWizardSecret` → `onWizardIssuer`)

**Example Wizard Flow in TOTP (lines 744-910):**
- `wizardStart()` - Initialize wizard state
- `onWizardName()` - Save name, advance to secret
- `onWizardSecret()` - Save secret, advance to issuer
- `onWizardIssuer()` - Save issuer, advance to digits menu
- `onWizardDigits()` - Save digits, advance to algorithm menu
- `onWizardAlgo()` - Save algorithm, advance to period menu
- `onWizardPeriod()` - Save period, call `wizardFinish()`
- `wizardFinish()` - Validate and persist

## Recommended Fix

### Create a Generic Wizard Framework

Create `components/cdc_ui/include/cdc_ui/Wizard.h`:

```cpp
#pragma once
#include "cdc_ui/ViewStack.h"
#include "cdc_views/T9InputView.h"
#include <cstring>
#include <functional>

namespace cdc::ui {

/**
 * \brief Base class for multi-step wizard flows.
 * \tparam StateData User-defined wizard state structure.
 */
template<typename StateData>
class Wizard {
public:
    using StepCallback = std::function<void()>;
    using SaveCallback = std::function<void(const char*)>;

    /**
     * \brief Initialize wizard with state data.
     * \param clearState Clear existing state (default: true).
     */
    void init(bool clearState = true) {
        if (clearState) {
            std::memset(&state_, 0, sizeof(StateData));
        }
    }

    /**
     * \brief Push a T9 input step with save callback.
     * \param title Step title.
     * \param initialText Initial input text.
     * \param maxLen Maximum text length.
     * \param onSave Callback when user saves.
     */
    void pushT9Step(const char* title, const char* initialText,
                    uint16_t maxLen, SaveCallback onSave) {
        auto& t9 = t9Input();  // Override provides instance
        t9.init(title, initialText, maxLen);
        t9.setOnSave([onSave](const char* text) {
            onSave(text);
        });
        ViewStack::instance().push(&t9);
    }

    /**
     * \brief Push a list selection step.
     * \param title Step title.
     * \param items List items.
     * \param count Item count.
     * \param onSelect Callback when user selects.
     */
    void pushListStep(const char* title, ui::ListItem* items,
                      uint16_t count, std::function<void(uint16_t)> onSelect) {
        auto& listView = listView();  // Override provides instance
        listView.init(title, items, count);
        listView.setOnSelect([onSelect](uint16_t idx, void*) {
            onSelect(idx);
        });
        ViewStack::instance().push(&listView);
    }

    /**
     * \brief Pop all wizard views back to anchor.
     * \param anchor View to pop back to.
     */
    void finish(ui::IView* anchor) {
        while (ViewStack::instance().current() != anchor &&
               ViewStack::instance().depth() > 1) {
            ViewStack::instance().pop();
        }
    }

    /**
     * \brief Access current state data.
     * \return Reference to state structure.
     */
    StateData& state() { return state_; }
    const StateData& state() const { return state_; }

protected:
    /**
     * \brief Override to provide T9InputView instance.
     * \return T9InputView reference.
     */
    virtual ui::T9InputView& t9Input() = 0;

    /**
     * \brief Override to provide ListView instance.
     * \return ListView reference.
     */
    virtual ui::ListView& listView() = 0;

private:
    StateData state_ = {};
};

} // namespace cdc::ui
```

### Implementation Steps

1. Create `components/cdc_ui/include/cdc_ui/Wizard.h` with the template class
2. Update each module to inherit from or compose with `Wizard<WizardState>`:
   - `mod_totp`: `class TotpModule : public Wizard<TotpModule::WizardState>`
   - `mod_password`: `class PasswordModule : public Wizard<PasswordModule::WizardState>`
   - `mod_gpg`: `class GpgModule : public Wizard<GpgModule::WizardState>`
3. Remove duplicate `pushT9WizardStep` functions
4. Use `Wizard::pushT9Step()` and `Wizard::pushListStep()` instead

### Alternative: Simpler Helper Class

If templates are too complex, create a simpler non-template helper:

```cpp
class WizardHelper {
public:
    static void pushT9Step(ui::T9InputView& t9, const char* title,
                           const char* initialText, uint16_t maxLen,
                           ui::T9InputView::SaveCallback onSave);
};
```

## References

- [Template Metaprogramming](https://en.cppreference.com/w/cpp/language/template)
- [State Pattern](https://en.wikipedia.org/wiki/State_pattern)
- Related findings: #1 (token parsing), #2 (UI helpers)
