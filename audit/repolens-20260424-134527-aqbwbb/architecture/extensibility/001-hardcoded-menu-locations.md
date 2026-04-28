---
title: "[MEDIUM] Hardcoded Menu Location Enum Limits Module Flexibility"
severity: MEDIUM
domain: extensibility
lens: menu-architecture
labels:
  - "audit:architecture/extensibility"
---

## Summary
The `MenuLocation` enum in `components/cdc_core/include/cdc_core/IModule.h:15-22` defines fixed menu locations (MAIN_MENU, TOOLS_MENU, SETTINGS_MENU, BLUETOOTH_MENU, WIFI_MENU, EXPERT_MENU). Adding a new menu category requires modifying this central enum and updating all menu-rebuild logic in `cdc_os_ui` components.

**Evidence:**
- `components/cdc_core/include/cdc_core/IModule.h:15-22`:
  ```cpp
  enum class MenuLocation : uint8_t {
      MAIN_MENU,       // Top-level main menu
      TOOLS_MENU,      // Under Tools submenu
      SETTINGS_MENU,   // Under Settings submenu
      BLUETOOTH_MENU,  // Under Bluetooth submenu (for BLE services)
      WIFI_MENU,       // Under WiFi submenu (for WiFi-related features)
      EXPERT_MENU      // Under Expert submenu (for advanced features)
  };
  ```

- `components/cdc_os_ui/src/AppUi.cpp:316-360`: Each menu location has its own rebuild function with hardcoded logic
- `components/cdc_os_ui/src/WifiMenuUi.cpp:197-230`: WiFi menu rebuild has fixed item count `WIFI_MENU_FIXED_COUNT = 4`
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp:115-145`: Bluetooth menu has similar hardcoded structure

## Impact
**Maintenance Burden:** Every new menu category requires:
1. Adding a new enum value
2. Creating a new rebuild function in `cdc_os_ui`
3. Updating `onMainMenuSelect()` or `onToolsSelect()` switch statements
4. Adding new static menu item arrays

This violates the Open/Closed Principle - the core menu system is not closed to modification when adding new menu types.

## Evidence
Files affected:
- `components/cdc_core/include/cdc_core/IModule.h:15-22` (enum definition)
- `components/cdc_os_ui/src/AppUi.cpp:316-360` (rebuild functions)
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:220-250` (expert menu with fixed count)
- `components/cdc_os_ui/src/WifiMenuUi.cpp:197-230` (WiFi menu with hardcoded items)
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp:115-145` (Bluetooth menu)

Menu item handling in `AppUi.cpp:419-435`:
```cpp
static void onToolsSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case 0: showModulesView(); return;
        case 1: showWifiMainMenu(); return;
        case 2: showBluetoothMenu(); return;
        case 3: showExpertMenu(); return;
    }
    // ... module items handled after fixed count
}
```

## Recommended Fix
Implement a registry-based menu system where menu locations are defined dynamically:

1. **Add MenuLocationRegistry:**
   ```cpp
   class MenuLocationRegistry {
   public:
       struct MenuDef {
           const char* id;
           const char* title;
           uint8_t priority;
           void (*showFunc)();
       };
       void registerLocation(const char* id, const char* title, uint8_t priority, void (*showFunc)());
       uint8_t getLocationCount();
       MenuDef getLocation(uint8_t index);
   };
   ```

2. **Modules register their own menu locations:**
   ```cpp
   // In module init
   MenuLocationRegistry::instance().registerLocation(
       "weather", "Weather", 100, showWeatherMenu
   );
   ```

3. **Menu rebuild becomes data-driven:**
   ```cpp
   void rebuildToolsMenu() {
       uint8_t fixedCount = 0;
       for (auto& def : MenuLocationRegistry::instance().getLocations()) {
           s_toolsItems[fixedCount++] = {def.title, 0, false, nullptr};
       }
       // ... add module items
   }
   ```

This allows new menu categories without modifying core UI code.

## References
- Open/Closed Principle: https://en.wikipedia.org/wiki/Open%E2%80%93closed_principle
- Strategy Pattern for menu rendering
- Registry Pattern for dynamic discovery
