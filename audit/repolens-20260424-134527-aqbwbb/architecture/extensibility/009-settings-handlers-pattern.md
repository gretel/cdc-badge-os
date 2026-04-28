---
title: "[LOW] Settings Handlers Use Static Functions with Global State"
severity: LOW
domain: extensibility
lens: settings-system
labels:
  - "audit:architecture/extensibility"
---

## Summary
Settings handlers in `components/cdc_os_ui/src/SettingsHandlers.cpp` use static functions with global state (`s_display`, `s_sleep`, `s_lockScreen`). Adding new settings requires adding new static functions and updating switch statements.

**Evidence:**
- `components/cdc_os_ui/src/SettingsHandlers.cpp:17-22`:
  ```cpp
  static hal::IDisplay* s_display = nullptr;
  static hal::ISleepController* s_sleep = nullptr;
  static LockScreenView* s_lockScreen = nullptr;
  
  void init(hal::IDisplay* display, hal::ISleepController* sleep, LockScreenView* lockScreen) {
      s_display = display;
      s_sleep = sleep;
      s_lockScreen = lockScreen;
  }
  ```

- `components/cdc_os_ui/src/SettingsHandlers.cpp:199-230`:
  ```cpp
  static void showBadgeTextStep(uint8_t step) {
      const char* title = nullptr;
      const char* initial = nullptr;
      T9InputView::SaveCallback cb = nullptr;
      
      switch (step) {
          case BADGE_STEP_NAME:
              title = tr(StringId::NAME);
              initial = s_lockScreen->getDisplayName();
              cb = onBadgeNameSave;
              break;
          case BADGE_STEP_INFO:
              title = tr(StringId::INFO);
              initial = s_lockScreen->getInfo();
              cb = onBadgeInfoSave;
              break;
          // ...
      }
  }
  ```

- `components/cdc_os_ui/src/AppUi.cpp:444-477`: Switch in `onSettingsSelect`:
  ```cpp
  static void onSettingsSelect(uint16_t index, void* userData) {
      switch (index) {
          case SETTINGS_IDX_BRIGHTNESS:
              ViewStack::instance().push(s_brightnessSlider);
              break;
          case SETTINGS_IDX_LANGUAGE:
              ViewStack::instance().push(s_languageMenu);
              break;
          // ...
      }
  }
  ```

## Impact
**Scalability Issue:**
1. New settings require new static functions
2. Switch statements grow with each setting
3. Global state makes testing difficult
4. Modules cannot easily add settings

## Evidence
Files affected:
- `components/cdc_os_ui/src/SettingsHandlers.cpp` (handlers)
- `components/cdc_os_ui/src/AppUi.cpp:444-477` (switch)
- `components/cdc_os_ui/include/cdc_os_ui/SettingsHandlers.h` (interface)

## Recommended Fix
Use handler registry:

1. **Handler struct:**
   ```cpp
   struct SettingHandler {
       const char* name;
       ui::IView* (*createView)();
       void (*onSelect)();
       uint8_t priority;
   };
   
   class SettingsRegistry {
   public:
       void registerHandler(const SettingHandler& handler);
       void buildMenu(ListView* menu);
   };
   ```

2. **Modules register settings:**
   ```cpp
   // In module init
   static ui::IView* createGpgSettings() { return &gpgSettingsView; }
   static void onGpgSelect() { ViewStack::instance().push(&gpgSettingsView); }
   
   SettingsRegistry::instance().registerHandler({
       "GPG Keys",
       createGpgSettings,
       onGpgSelect,
       100  // priority
   });
   ```

## References
- Command Pattern for settings
- Registry Pattern
