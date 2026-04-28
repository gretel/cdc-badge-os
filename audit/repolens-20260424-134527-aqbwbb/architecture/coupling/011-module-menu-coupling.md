---
title: "[MEDIUM] Module menu items hold references to dynamic module state"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "menu-coupling"
  - "dynamic-labels"
---

## Summary
Modules register menu items that contain pointers to dynamically-allocated or module-scoped strings, creating temporal coupling between menu rendering and module state. The `GroveLedModule` example shows this with `getLedToggleLabel()` returning a pointer to `mstr(STR_LEDS)`.

**Evidence:**
- `components/grove_led/src/GroveLedModule.cpp:389-391`: `getLedToggleLabel()` returns pointer to i18n string
- Menu items store pointers that become invalid if module strings are re-registered
- Lock screen context items use callback functions that reference module state

## Impact
1. **Dangling pointers**: If module re-registers i18n strings, menu labels may point to freed memory
2. **State inconsistency**: Dynamic labels may not update when module state changes
3. **Initialization order dependency**: Menu items must be registered after module i18n setup
4. **Debugging difficulty**: Issues may only appear after specific module operations

## Evidence
File: `components/grove_led/src/GroveLedModule.cpp`
```cpp
// Lines 389-391: Returns pointer to i18n string
static const char* getLedToggleLabel() {
    return mstr(STR_LEDS);  // Pointer to dynamically-registered string
}

// Lines 398-403: Context menu item holds function pointer
items[0] = {
    .getLabel = getLedToggleLabel,  // Callback returns dynamic string
    .callback = onLedToggle,
    .priority = 50,
    .moduleName = nullptr
};
```

File: `components/grove_led/src/GroveLedModule.cpp:220-229`
```cpp
// Dynamic label construction in getGroveLedMenu()
static ui::IView* getGroveLedMenu() {
    snprintf(s_enableLabel, sizeof(s_enableLabel), "%s: %s",
             mstr(STR_LEDS),
             GroveLedModule::instance().isEnabled() ? mstr(STR_ON) : mstr(STR_OFF));
    // s_enableLabel is static buffer, but menu holds pointer to it
    s_mainMenuItems[0] = {s_enableLabel, 0, false, nullptr};
    ...
}
```

## Recommended Fix
1. **Copy string data into menu item**:
   ```cpp
   struct ModuleMenuItem {
       char label[64];  // Fixed-size buffer instead of pointer
       // ...
   };
   ```

2. **Or use string ID references**:
   ```cpp
   struct ModuleMenuItem {
       uint16_t labelStringId;  // Reference to i18n string
       // ...
   };
   ```

3. **Ensure menu rebuild on state change**: When module state changes, call `ui_rebuild_menus()` to refresh all menu items

4. **Document string lifetime requirements**: Add comments to `IModule.h` explaining that menu item labels must remain valid for module lifetime

## References
- Menu item structure: `components/cdc_core/include/cdc_core/IModule.h:30-38`
- I18n registration: `components/cdc_ui/include/cdc_ui/I18n.h:250-265`
- Menu rebuild: `components/cdc_os_ui/include/cdc_os_ui/AppUi.h:29-30`
