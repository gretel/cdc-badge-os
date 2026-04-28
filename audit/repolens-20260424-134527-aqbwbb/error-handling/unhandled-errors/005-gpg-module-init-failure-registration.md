---
title: "[MEDIUM] GPG module registered even when init() fails"
severity: MEDIUM
domain: error-handling
lens: unhandled-errors
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `GpgModule.cpp` at lines 651-661, the GPG module is registered unconditionally in the initializer. When `init()` fails, the module is still registered but in an ERROR state. The menu items may still appear but return nullptr, causing potential UI issues.

**Location:** `components/mod_gpg/src/GpgModule.cpp:651-661`
```cpp
extern "C" void mod_gpg_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_gpg::GpgModule::instance();
        if (module.init()) {
            module.start();
        }
        // Module registered inside init() even if it fails!
    });
}
```

## Impact
- Module appears in menu system but clicking shows error toast instead of menu
- `getMenuItems()` can return menu entry but `getView()` returns nullptr on error
- User experience: menu item visible but non-functional
- `state_` is set to ERROR but no check in `getMenuItems()` to hide the entry
- Inconsistent with expected behavior where failed modules should be hidden

## Evidence
**GpgModule.cpp:651-661**
```cpp
extern "C" void mod_gpg_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_gpg::GpgModule::instance();
        if (module.init()) {
            module.start();
        }
        // No error handling here - module already registered in init()
    });
}
```

**GpgModule.cpp:548-569** - init() registers module unconditionally:
```cpp
bool GpgModule::init() {
    LOG_I(TAG, "Initializing GPG module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);  // <-- Always registered!
    if (!slotRange_.hasEcc) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "GPG slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;  // Returns false but module already registered
    }
    // ... more error paths that return false after registration
}
```

**GpgModule.cpp:620-640** - getMenuItems() returns menu even on error:
```cpp
uint8_t GpgModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
        if (!s_viewsInitialized) {
            s_menuView.setOnSelect(onMenuSelect);
            s_viewsInitialized = true;
        }
        if (!GpgModule::instance().slotRange_.hasEcc || !GpgModule::instance().slotRange_.hasRmem) {
            ui::showToastError(mstr(STR_SLOT_ERROR));
            return nullptr;  // Returns nullptr but menu entry still exists
        }
        rebuildMenu();
        return &s_menuView;
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
    return 1;
}
```

**Compare with proper pattern in GroveLedModule.cpp:364-376** (which has `.isVisible` field available):
```cpp
items[0] = {
    .label = mstr(STR_GROVE_LED),
    .priority = 50,
    .getView = getGroveLedMenu,
    .isVisible = nullptr,  // <-- Can be used to hide failed modules
    .moduleName = nullptr,
    .location = core::MenuLocation::TOOLS_MENU,
    .onSelect = nullptr
};
```

## Recommended Fix
Option 1 - Add `.isVisible` check to menu item:

```cpp
uint8_t GpgModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
        if (!s_viewsInitialized) {
            s_menuView.setOnSelect(onMenuSelect);
            s_viewsInitialized = true;
        }
        if (!GpgModule::instance().slotRange_.hasEcc || !GpgModule::instance().slotRange_.hasRmem) {
            ui::showToastError(mstr(STR_SLOT_ERROR));
            return nullptr;
        }
        rebuildMenu();
        return &s_menuView;
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU,
    [](void* ctx) { return cdc::mod_gpg::GpgModule::instance().getState() == cdc::core::ServiceState::STARTED; }};
    return 1;
}
```

Option 2 - Check state in getMenuItems() and return 0 if error:

```cpp
uint8_t GpgModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    // Hide menu if module failed to initialize
    if (state_ != core::ServiceState::STARTED) {
        return 0;
    }
    // ... rest of menu item setup
}
```

## References
- ModuleMenuItem structure: `components/cdc_core/include/cdc_core/IModule.h`
- IService state enum: `components/cdc_core/include/cdc_core/IService.h`
- Module registration pattern: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
