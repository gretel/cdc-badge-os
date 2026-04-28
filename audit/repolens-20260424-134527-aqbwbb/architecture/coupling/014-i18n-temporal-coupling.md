---
title: "[MEDIUM] I18n module registration creates temporal coupling"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "i18n-coupling"
  - "initialization-order"
---

## Summary
Modules must register i18n strings in a specific order during initialization, creating temporal coupling. If a module's menu items are accessed before `registerStrings()` is called, the strings will have invalid IDs. The `GroveLedModule` shows this pattern with `registerStrings()` called from `init()`.

**Evidence:**
- `components/grove_led/src/GroveLedModule.cpp:51-93`: `registerStrings()` called from `init()`
- `components/grove_led/src/GroveLedModule.cpp:28`: `static uint16_t s_strIdBase = 0;` - depends on registration order
- Menu items use `mstr()` helper that depends on `s_strIdBase` being set first

## Impact
1. **Initialization order dependency**: Menu items must be created after `init()` completes
2. **Crash risk**: Accessing strings before registration returns 0 or invalid IDs
3. **Hard to debug**: Issues may appear as empty strings or wrong translations
4. **Module coupling**: Modules cannot be initialized in arbitrary order

## Evidence
File: `components/grove_led/src/GroveLedModule.cpp`
```cpp
// Line 28: Static string ID base
static uint16_t s_strIdBase = 0;

// Lines 43-48: Helper function depends on s_strIdBase
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

// Lines 51-93: Registration in init()
bool GroveLedModule::init() {
    registerStrings();  // Must be called first!
    loadSettings();
    // ...
}

// Line 220: Menu creation uses mstr() - must be after init()
static ui::IView* getGroveLedMenu() {
    s_mainMenuItems[0] = {mstr(STR_LEDS), ...};  // Depends on s_strIdBase
    ...
}
```

File: `main/main.cpp`
```cpp
// Lines 226-231: Initialization order
modules_register_all();  // Modules register
ModuleRegistry::instance().runAllInitializers();  // Modules init (register i18n)
ui_on_modules_ready();  // Menus built (use i18n strings)
```

## Recommended Fix
1. **Return string IDs from registration**:
   ```cpp
   struct ModuleI18n {
       uint16_t baseId;
       void registerStrings();
   };
   ```

2. **Use string IDs in menu items**:
   ```cpp
   struct ModuleMenuItem {
       uint16_t labelStringId;  // Instead of const char*
   };
   ```

3. **Validate initialization state**:
   ```cpp
   static const char* mstr(uint16_t offset) {
       if (s_strIdBase == 0) {
           return "ERR";  // Debug indicator
       }
       return ui::tr(s_strIdBase + offset);
   }
   ```

4. **Document initialization order**: Explicitly document in module header that `init()` must be called before `getMenuItems()`

## References
- I18n registration: `components/cdc_ui/include/cdc_ui/I18n.h:250-265`
- Module init order: `main/main.cpp:226-231`
- Menu item structure: `components/cdc_core/include/cdc_core/IModule.h:30-38`
