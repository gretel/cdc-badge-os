---
title: "[MEDIUM] Shared static view instances in modules create hidden coupling"
severity: MEDIUM
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
Multiple modules use static view instances that are shared across the entire module lifecycle. These views maintain internal state and are reused, creating hidden coupling between different menu operations and potential state leakage.

**Evidence locations:**
- `components/mod_gpg/src/GpgModule.cpp:203-214` - Static view instances
- `components/mod_password/src/PasswordModule.cpp:336-342` - Static view instances
- `components/mod_totp/src/TotpModule.cpp:120-126` - Static view instances

**Static view declarations:**
```cpp
// GpgModule.cpp:203-214
static ui::ListView s_menuView;
static ui::ListView s_settingsView;
static ui::PinChangeView s_pinChangeView;
static ui::T9InputView s_t9Input;
static ui::ListView s_curveView;
static ui::InfoView s_infoView;
static ui::QRCodeView s_qrView;
static bool s_viewsInitialized = false;
```

## Impact
- **State leakage**: View state persists between different operations (e.g., wizard steps)
- **Reentrancy issues**: If view is pushed twice, state may conflict
- **Memory not reclaimed**: Views allocated once for entire lifetime
- **Hard to track state**: View state not obvious from module state

## Evidence
```cpp
// components/mod_gpg/src/GpgModule.cpp:203-214
static ui::ListView s_menuView;
static ui::ListView s_settingsView;
static ui::PinChangeView s_pinChangeView;
static ui::T9InputView s_t9Input;
static ui::ListView s_curveView;
static ui::InfoView s_infoView;
static ui::QRCodeView s_qrView;
static bool s_viewsInitialized = false;

// components/mod_gpg/src/GpgModule.cpp:370-376
static void showSettings() {
    s_settingsItems[0].label = mstr(STR_USER_PIN);
    s_settingsItems[1].label = mstr(STR_ADMIN_PIN);
    s_settingsView.init(mstr(STR_SETTINGS), s_settingsItems, 2);
    s_settingsView.setOnSelect(onSettingsSelect);
    ui::ViewStack::instance().push(&s_settingsView);  // Reusing static view!
}
```

```cpp
// components/mod_password/src/PasswordModule.cpp:336-342
static ui::ListView s_listView;
static ui::T9InputView s_t9Input;
static ui::InfoView s_infoView;
static bool s_viewsInitialized = false;
static ui::ListItem* s_listItems = nullptr;
static PasswordStore::EntryIndex* s_entries = nullptr;
static uint16_t s_entryCount = 0;
static uint16_t s_capacity = 0;
```

```cpp
// components/mod_password/src/PasswordModule.cpp:542-546
static void pushT9WizardStep(const char* title, const char* initialText,
                             uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);  // Reinitializing same view!
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}
```

## Recommended Fix
**Use view factory pattern:**

1. **Create view factory in each module:**
```cpp
// components/mod_gpg/src/GpgModule.cpp
namespace cdc::mod_gpg {

class ViewFactory {
public:
    ui::ListView* createMenuView() {
        auto* view = new ui::ListView();
        view->setOnSelect(onMenuSelect);
        return view;
    }

    ui::T9InputView* createT9Input(const char* title, const char* initialText, uint16_t maxLen) {
        auto* view = new ui::T9InputView();
        view->init(title, initialText, maxLen);
        return view;
    }

    // ... other factory methods
};

static ViewFactory& getViewFactory() {
    static ViewFactory factory;
    return factory;
}

} // namespace cdc::mod_gpg
```

2. **Refactor view creation:**
```cpp
// Before:
static void wizardStart() {
    s_t9Input.init(mstr(STR_NAME), nullptr, 63);
    s_t9Input.setOnSave(onWizardName);
    ui::ViewStack::instance().push(&s_t9Input);
}

// After:
static void wizardStart() {
    auto* t9View = getViewFactory().createT9Input(mstr(STR_NAME), nullptr, 63);
    t9View->setOnSave(onWizardName);
    ui::ViewStack::instance().push(t9View);
}
```

3. **Or use static views with explicit reset:**
```cpp
// If keeping static views, ensure they're reset before reuse
static void wizardStart() {
    // Explicitly reset view state
    s_t9Input.init(mstr(STR_NAME), nullptr, 63);
    s_t9Input.setOnSave(onWizardName);
    // Clear any previous state
    s_t9View.clear();
    ui::ViewStack::instance().push(&s_t9Input);
}
```

## References
- [Factory Pattern on Wikipedia](https://en.wikipedia.org/wiki/Factory_method_pattern)
- [Object Pool Pattern](https://en.wikipedia.org/wiki/Object_pool_pattern)
