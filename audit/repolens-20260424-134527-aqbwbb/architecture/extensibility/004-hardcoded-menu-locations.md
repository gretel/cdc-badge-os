---
title: "[LOW] Menu locations are hardcoded enum instead of dynamic registration"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
Menu locations are defined as a fixed enum in `components/cdc_core/include/cdc_core/IModule.h:11-21`. New menu sections require modifying the core `IModule.h` file, which violates the Open/Closed Principle.

**Evidence:**
- `components/cdc_core/include/cdc_core/IModule.h:11-21` - Hardcoded `MenuLocation` enum

## Impact
- **Extension Barrier**: Adding a new menu section (e.g., "GAMES", "UTILITIES") requires editing core code
- **Coupling**: All modules depend on the core enum definition
- **Scalability**: As the system grows, more menu locations will be needed

## Evidence
File: `components/cdc_core/include/cdc_core/IModule.h:11-21`
```cpp
enum class MenuLocation : uint8_t {
    MAIN_MENU,       // Top-level main menu
    TOOLS_MENU,      // Under Tools submenu
    SETTINGS_MENU,   // Under Settings submenu
    BLUETOOTH_MENU,  // Under Bluetooth submenu (for BLE services)
    WIFI_MENU,       // Under WiFi submenu (for WiFi-related features)
    EXPERT_MENU      // Under Expert submenu (for advanced tools)
};
```

File: `components/cdc_os_ui/src/AppUi.cpp:328-362` - Menu building logic with hardcoded locations
```cpp
void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    s_mainMenuPluginCount = moduleReg.getMenuItems(
        core::MenuLocation::MAIN_MENU,
        s_mainMenuModuleItems,
        MAIN_MENU_MAX_ITEMS - MAIN_MENU_FIXED_COUNT
    );
    // ...
}

void rebuildToolsMenu() {
    // Fixed items
    s_toolsItems[0] = {tr(StringId::MODULES), 0, false, nullptr};
    s_toolsItems[1] = {tr(StringId::WIFI_MENU), 0, false, nullptr};
    // ...
}
```

## Recommended Fix
Implement dynamic menu location registration:

1. **Change `MenuLocation` to a string-based identifier**:
   ```cpp
   struct MenuLocation {
       const char* id;       // Unique identifier (e.g., "main", "tools")
       const char* parentId; // Parent menu ID (nullptr for root)
       uint8_t priority;     // Sort order among sibling locations
   };
   ```

2. **Add registration API**:
   ```cpp
   class MenuRegistry {
   public:
       static MenuRegistry& instance();
       void registerLocation(const MenuLocation& loc);
       MenuLocation* getLocation(const char* id);
       const std::vector<MenuLocation>& getAllLocations();
   };
   ```

3. **Pre-define standard locations** in a core module:
   ```cpp
   void registerStandardMenuLocations() {
       MenuRegistry::instance().registerLocation({"main", nullptr, 0});
       MenuRegistry::instance().registerLocation({"tools", "main", 1});
       MenuRegistry::instance().registerLocation({"settings", "main", 2});
       // ...
   }
   ```

4. **Allow modules to register custom locations**:
   ```cpp
   // In a module that wants a custom menu
   MenuRegistry::instance().registerLocation({
       "games", "tools", 10  // Add "Games" under "Tools"
   });
   ```

This allows modules to create their own menu sections without editing core code.

## References
- Plugin Architecture: Dynamic extension points
- Registry Pattern: Centralized management of extensible items
- Hierarchical Data Structures: Tree-based menu organization
