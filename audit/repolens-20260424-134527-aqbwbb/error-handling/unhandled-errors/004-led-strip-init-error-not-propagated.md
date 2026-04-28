---
title: "[MEDIUM] LED strip init error logged but module still registered in menu"
severity: MEDIUM
domain: error-handling
lens: unhandled-errors
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `GroveLedModule.cpp` at lines 633-651, when LED strip initialization fails, the error is logged and `init()` returns false, but the module is still registered in the menu system. The menu entry will be visible but clicking it may show an empty/invalid view.

**Location:** `components/grove_led/src/GroveLedModule.cpp:633-651`
```cpp
extern "C" void grove_led_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        auto& module = cdc::grove_led::GroveLedModule::instance();

        // Register first so module appears in list (even if init fails)
        moduleReg.registerModule(&module);  // <-- Always registered!

        if (!module.init()) {
            // Init failed - report error
            moduleReg.reportModuleError(module.getName(), "LED strip init failed");
            return;
        }

        module.start();
    });
}
```

## Impact
- Module appears in the Tools menu even when initialization failed
- User may select the menu entry expecting functionality but get an empty view
- `getMenuItems()` returns menu items unconditionally - no check for `state_ == ERROR`
- `getLedToggleLabel()` and `onLedToggle()` in lock-screen context may behave unexpectedly
- Inconsistent with other modules that properly hide failed modules

## Evidence
**GroveLedModule.cpp:633-651**
```cpp
extern "C" void grove_led_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        auto& module = cdc::grove_led::GroveLedModule::instance();

        // Register first so module appears in list (even if init fails)
        moduleReg.registerModule(&module);  // <-- Comment admits it's always registered

        if (!module.init()) {
            moduleReg.reportModuleError(module.getName(), "LED strip init failed");
            return;  // <-- But registration already happened!
        }

        module.start();
    });
}
```

**GroveLedModule.cpp:364-376** - getMenuItems() doesn't check state:
```cpp
uint8_t GroveLedModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {
        .label = mstr(STR_GROVE_LED),
        .priority = 50,
        .getView = getGroveLedMenu,
        .isVisible = nullptr,  // <-- No visibility check!
        .moduleName = nullptr,
        .location = core::MenuLocation::TOOLS_MENU,
        .onSelect = nullptr
    };

    return 1;
}
```

**GroveLedModule.cpp:138-145** - LED strip init failure:
```cpp
esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_);
if (err != ESP_OK) {
    LOG_E(TAG, "Failed to create LED strip: %s", esp_err_to_name(err));
    return false;  // Returns false but module already registered
}
```

**Compare with GpgModule.cpp:640-651** which has better error handling:
```cpp
uint8_t GpgModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
        // ...
        if (!GpgModule::instance().slotRange_.hasEcc || !GpgModule::instance().slotRange_.hasRmem) {
            ui::showToastError(mstr(STR_SLOT_ERROR));
            return nullptr;  // <-- Returns nullptr to hide menu
        }
        rebuildMenu();
        return &s_menuView;
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
    return 1;
}
```

## Recommended Fix
Option 1 - Add visibility check in getMenuItems():

```cpp
uint8_t GroveLedModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {
        .label = mstr(STR_GROVE_LED),
        .priority = 50,
        .getView = getGroveLedMenu,
        .isVisible = []() { return GroveLedModule::instance().getState() == core::ServiceState::STARTED; },
        .moduleName = nullptr,
        .location = core::MenuLocation::TOOLS_MENU,
        .onSelect = nullptr
    };

    return 1;
}
```

Option 2 - Move registration after successful init:

```cpp
extern "C" void grove_led_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& moduleReg = cdc::core::ModuleRegistry::instance();
        auto& module = cdc::grove_led::GroveLedModule::instance();

        if (!module.init()) {
            moduleReg.reportModuleError(module.getName(), "LED strip init failed");
            return;
        }

        module.start();
        moduleReg.registerModule(&module);  // <-- Register only on success
    });
}
```

## References
- ModuleRegistry interface: `components/cdc_core/include/cdc_core/IModule.h`
- IService state enum: `components/cdc_core/include/cdc_core/IService.h`
- GPG module pattern for menu visibility: `components/mod_gpg/src/GpgModule.cpp`
