---
title: "[HIGH] Modules directly access ViewStack singleton creating tight UI coupling"
severity: HIGH
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
Multiple modules directly access the `ViewStack::instance()` singleton to push/pop views, creating tight coupling between business logic modules and the UI navigation system. This makes it impossible to reuse modules without the UI framework.

**Evidence locations:**
- `components/mod_gpg/src/GpgModule.cpp:246` - `ui::ViewStack::instance().push()`
- `components/mod_password/src/PasswordModule.cpp:461` - `ui::ViewStack::instance().push()`
- `components/mod_totp/src/TotpModule.cpp:180` - `ui::ViewStack::instance().push()`

**Direct ViewStack access in modules:**
```cpp
// GpgModule.cpp:246
ui::ViewStack::instance().push(&s_pinChangeView);

// PasswordModule.cpp:461
ui::ViewStack::instance().push(&s_infoView);

// PasswordModule.cpp:526
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}
```

## Impact
- **Modules cannot be tested without UI**: Unit tests for GPG/Password/TOTP must initialize ViewStack
- **UI framework becomes mandatory dependency**: Even simple modules need full UI stack
- **Tight coupling**: Changing ViewStack API breaks all modules
- **Violates dependency inversion**: Modules depend on concrete ViewStack instead of abstraction

## Evidence
**Module code directly calling ViewStack:**

```cpp
// components/mod_gpg/src/GpgModule.cpp:238-247
static void showSettings() {
    s_settingsItems[0].label = mstr(STR_USER_PIN);
    s_settingsItems[1].label = mstr(STR_ADMIN_PIN);
    s_settingsView.init(mstr(STR_SETTINGS), s_settingsItems, 2);
    s_settingsView.setOnSelect(onSettingsSelect);
    ui::ViewStack::instance().push(&s_settingsView);  // Direct coupling!
}
```

```cpp
// components/mod_gpg/src/GpgModule.cpp:409-413
static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_t9Input.init(mstr(STR_NAME), nullptr, 63);
    s_t9Input.setOnSave(onWizardName);
    ui::ViewStack::instance().push(&s_t9Input);  // Direct coupling!
}
```

```cpp
// components/mod_password/src/PasswordModule.cpp:514-528
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
               ui::ViewStack::instance().depth() > 1) {  // Direct coupling!
            ui::ViewStack::instance().pop();
        }
    }
}
```

```cpp
// components/mod_totp/src/TotpModule.cpp:175-182
static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_t9Input.init(mstr(STR_LABEL), nullptr, 63);
    s_t9Input.setOnSave(onWizardLabel);
    ui::ViewStack::instance().push(&s_t9Input);  // Direct coupling!
}
```

**Count of direct ViewStack calls in modules:**
- GpgModule.cpp: ~15 calls
- PasswordModule.cpp: ~12 calls
- TotpModule.cpp: ~8 calls
- Total: ~35+ direct ViewStack calls in modules

## Recommended Fix
**Introduce a navigation abstraction:**

1. **Create `INavigation` interface:**
```cpp
// components/cdc_ui/include/cdc_ui/INavigation.h
namespace cdc::ui {

class IView;

class INavigation {
public:
    virtual ~INavigation() = default;
    virtual void push(IView* view, void* context = nullptr) = 0;
    virtual void pop() = 0;
    virtual void replace(IView* view, void* context = nullptr) = 0;
    virtual void popToRoot() = 0;
    virtual IView* current() const = 0;
    virtual uint8_t depth() const = 0;
};

} // namespace cdc::ui
```

2. **Implement wrapper around ViewStack:**
```cpp
// components/cdc_ui/include/cdc_ui/ViewStackNavigation.h
namespace cdc::ui {

class ViewStackNavigation : public INavigation {
public:
    void push(IView* view, void* context = nullptr) override {
        ViewStack::instance().push(view, context);
    }

    void pop() override {
        ViewStack::instance().pop();
    }

    // ... other implementations
};

// Global accessor
INavigation* getNavigation();

} // namespace cdc::ui
```

3. **Initialize in main.cpp:**
```cpp
// In ui_init()
static cdc::ui::ViewStackNavigation s_navigation;
void ui_init(const UiDeps& deps) {
    // ... existing init
    s_navigation = cdc::ui::ViewStackNavigation();
    cdc::ui::initNavigation(&s_navigation);
}
```

4. **Refactor modules to use navigation:**
```cpp
// Before:
ui::ViewStack::instance().push(&s_settingsView);

// After:
ui::getNavigation()->push(&s_settingsView);
```

5. **For testing, provide mock navigation:**
```cpp
// In test file
class MockNavigation : public ui::INavigation {
    std::vector<IView*> pushedViews;
    void push(ui::IView* view, void* context = nullptr) override {
        pushedViews.push_back(view);
    }
    // ... other mocks
};

TEST(ModuleTest, CanNavigate) {
    MockNavigation nav;
    ui::initNavigation(&nav);
    module.showSettings();
    EXPECT_EQ(nav.pushedViews.size(), 1);
}
```

## References
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
- [Navigation Pattern (software architecture)](https://en.wikipedia.org/wiki/Navigation_pattern)
