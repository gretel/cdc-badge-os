---
title: "[MEDIUM] Static view instances create hidden dependencies in modules"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "static-state"
  - "view-coupling"
---

## Summary
Modules use static view instances that persist across module lifecycles, creating hidden coupling and potential memory/order-of-initialization issues. The `GroveLedModule` demonstrates this pattern with multiple static views that are lazily instantiated.

**Evidence:**
- `components/grove_led/src/GroveLedModule.cpp:174-181` declares static view pointers
- These views outlive the module's lifecycle and are not cleaned up on module stop/disable
- Views hold references to module-specific data and callbacks

## Impact
1. **Memory leaks**: Static views are never destroyed, even when module is disabled
2. **Stale references**: Views may hold pointers to module data that becomes invalid
3. **Initialization order coupling**: Views depend on module being initialized first
4. **Testing difficulty**: Static state persists between test runs
5. **State inconsistency**: Views may show stale data after module reinitialization

## Evidence
File: `components/grove_led/src/GroveLedModule.cpp`
```cpp
// Lines 174-181: Static view storage
static ui::ListView* s_mainMenu = nullptr;
static ui::ListItem s_mainMenuItems[5];
static char s_enableLabel[24];  // Dynamic label for "LEDs: On/Off"

static ui::SliderView* s_ledCountSlider = nullptr;
static ui::SliderView* s_brightnessSlider = nullptr;
static RgbInputView* s_rgbInput = nullptr;
static ui::ListView* s_effectMenu = nullptr;
static ui::ListItem s_effectMenuItems[1];
```

Usage pattern (lines 236-245):
```cpp
static void showLedCountView() {
    if (!s_ledCountSlider) {
        s_ledCountSlider = new ui::SliderView();  // Lazy, never freed
    }
    s_ledCountSlider->init(...);
    ui::ViewStack::instance().push(s_ledCountSlider);
}
```

## Recommended Fix
1. **Move view storage to module instance**:
   ```cpp
   class GroveLedModule : public core::IModule {
   private:
       std::unique_ptr<ui::ListView> s_mainMenu;
       std::unique_ptr<ui::SliderView> s_ledCountSlider;
       // ...
   };
   ```

2. **Or use factory pattern**: Create views on-demand without caching
   ```cpp
   static ui::IView* getGroveLedMenu() {
       auto* menu = new ui::ListView();
       // ... configure and return
       return menu;
   }
   ```

3. **Clear static state on module stop**: If static caching is necessary, add cleanup in `stop()` method

4. **Consider view pooling**: If views are frequently created/destroyed, implement a simple view pool

## References
- ViewStack pattern: `components/cdc_ui/include/cdc_ui/ViewStack.h`
- Module lifecycle: `components/cdc_core/include/cdc_core/IModule.h` (init/start/stop)
