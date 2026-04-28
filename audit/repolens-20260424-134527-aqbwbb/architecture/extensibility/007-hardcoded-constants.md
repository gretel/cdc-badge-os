---
title: "[LOW] Hardcoded constants scattered across core modules"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
Numerous hardcoded constants are embedded directly in source files throughout the codebase. These should be centralized in configuration headers or externalized for easier tuning.

**Evidence:**
- `components/cdc_os_ui/src/AppUi.cpp:54-58` - UI constants
- `components/serial_cmd/src/SerialCmd.cpp:33-39` - Command processor constants
- `components/cdc_core/include/cdc_core/ModuleRegistry.h:19-21` - Registry limits

## Impact
- **Maintenance**: Constants scattered across files make tuning harder
- **Discoverability**: Hard to find all related constants
- **Flexibility**: Changing a value requires finding and editing multiple files

## Evidence
File: `components/cdc_os_ui/src/AppUi.cpp:54-58`
```cpp
static constexpr uint8_t MAIN_MENU_MAX_ITEMS = 16;
static constexpr uint8_t MAIN_MENU_FIXED_COUNT = 2;  // Tools + Settings
static constexpr uint8_t TOOLS_FIXED_COUNT = 4;       // Modules, WiFi, Bluetooth, Expert
static constexpr uint8_t TOOLS_MAX_ITEMS = 16;
static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
```

File: `components/serial_cmd/src/SerialCmd.cpp:33-39`
```cpp
static constexpr size_t HISTORY_MAX = 10;
static constexpr size_t HEX_DUMP_WIDTH = 16;
static constexpr size_t NVS_KEY_MAX_LEN = 15;
static constexpr size_t NVS_NAMESPACE_MAX_LEN = 15;
static constexpr int YEAR_MIN = 2020;
static constexpr int YEAR_MAX = 2100;
```

File: `components/cdc_core/include/cdc_core/ModuleRegistry.h:19-21`
```cpp
static constexpr uint8_t MAX_MODULES = 16;
static constexpr uint8_t MAX_MENU_ITEMS = 32;
static constexpr uint8_t MAX_INITIALIZERS = 16;
```

## Recommended Fix
Centralize configuration constants:

1. **Create configuration headers**:
   ```cpp
   // components/cdc_core/include/cdc_core/ui_config.h
   #pragma once
   
   namespace cdc::config {
       // UI Configuration
       constexpr uint8_t UI_MAIN_MENU_MAX_ITEMS = 16;
       constexpr uint8_t UI_TOOLS_MAX_ITEMS = 16;
       constexpr uint32_t UI_INACTIVITY_TIMEOUT_MS = 5 * 1000 * 60;
       
       // Menu structure
       constexpr uint8_t UI_MAIN_MENU_FIXED_COUNT = 2;
       constexpr uint8_t UI_TOOLS_FIXED_COUNT = 4;
   }
   ```

2. **Create serial command configuration**:
   ```cpp
   // components/serial_cmd/include/serial_cmd/serial_config.h
   #pragma once
   
   namespace cdc::serial::config {
       constexpr size_t CMD_HISTORY_MAX = 10;
       constexpr size_t CMD_HEX_DUMP_WIDTH = 16;
       constexpr size_t CMD_NVS_KEY_MAX_LEN = 15;
       constexpr size_t CMD_NVS_NAMESPACE_MAX_LEN = 15;
       constexpr int CMD_YEAR_MIN = 2020;
       constexpr int CMD_YEAR_MAX = 2100;
   }
   ```

3. **Update code to use config**:
   ```cpp
   #include "cdc_core/ui_config.h"
   
   void rebuildMainMenu() {
       using namespace cdc::config;
       // ...
       s_mainMenuPluginCount = moduleReg.getMenuItems(
           core::MenuLocation::MAIN_MENU,
           s_mainMenuModuleItems,
           UI_MAIN_MENU_MAX_ITEMS - UI_MAIN_MENU_FIXED_COUNT
       );
   }
   ```

This centralizes configuration for easier tuning and better discoverability.

## References
- Configuration Management: Centralize tunable parameters
- Magic Numbers: Named constants improve readability and maintainability
