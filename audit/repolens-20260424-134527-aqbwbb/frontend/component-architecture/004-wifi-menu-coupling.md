---
title: "[MEDIUM] WifiMenuUi.cpp has 715 lines with tight coupling to AppUi internals"
severity: MEDIUM
domain: frontend
lens: component-architecture
labels:
  - "component-architecture"
  - "coupling"
  - "maintainability"
---

## Summary

The `WifiMenuUi.cpp` component (715 lines) is **tightly coupled** to `AppUiInternal.h` and shares static state with the main AppUi module. It uses `using namespace cdc::ui;` and imports internal AppUi structures, making it difficult to reuse or test independently.

**File:** `components/cdc_os_ui/src/WifiMenuUi.cpp`
**Lines:** 715 total

## Impact

**Tight Coupling:**
- Cannot be moved or reused without pulling in AppUi dependencies
- `using namespace cdc::ui;` pollutes the namespace
- Static view pointers (`s_wifiMainMenu`, `s_wifiScanView`) are module-global

**Maintainability:**
- Large file with mixed concerns (scan, setup wizard, connection, details)
- Changes to WiFi logic may break AppUi integration
- Hard to test WiFi functionality in isolation

**Limited Reusability:**
- WiFi menu is not a self-contained component
- Cannot be used in a different context (e.g., settings panel) without modification

## Evidence

**Line 6:** Direct import of internal AppUi header
```cpp
#include "AppUiInternal.h"
```

**Line 18-19:** Namespace pollution
```cpp
namespace cdc::ui {
using namespace cdc::ui;  // Redundant and confusing
```

**Lines 64-82:** Static state shared across module
```cpp
static ListView* s_wifiMainMenu = nullptr;
static ListView* s_wifiScanView = nullptr;
static ListView* s_wifiAuthMenu = nullptr;
static ListView* s_wifiIpMenu = nullptr;

static ListItem s_wifiMainItems[WIFI_MENU_MAX_ITEMS];
static core::ModuleMenuItem s_wifiModuleItems[12];
static uint8_t s_wifiModuleCount = 0;
// ... more static arrays and buffers
```

**Lines 208-235:** `rebuildWifiMainMenu()` uses global `core::ModuleRegistry`
```cpp
void rebuildWifiMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();
    // ...
}
```

**Lines 218-220:** Direct dependency on `WifiHandlers` singleton
```cpp
auto& wifiHandlers = WifiHandlers::instance();
```

**Lines 414-424:** Hardcoded view creation with static instance
```cpp
if (!s_wifiScanView) {
    s_wifiScanView = new ListView();
    s_wifiScanView->setOnSelect(onWifiScanSelect);
    s_wifiScanView->setItemRenderer(renderWifiRow, nullptr);
}
```

## Recommended Fix

**1. Remove namespace pollution (~10 min)**
```cpp
// Line 18-19: Remove redundant using
namespace cdc::ui {
// Remove: using namespace cdc::ui;
```

**2. Encapsulate static state in a class (~30 min)**
```cpp
// components/cdc_os_ui/src/WifiMenuManager.h
class WifiMenuManager {
public:
    static WifiMenuManager& instance();
    
    void init();
    void showMainMenu();
    void rebuildMainMenu();
    
private:
    WifiMenuManager();
    
    ListView* wifiMainMenu_ = nullptr;
    ListView* wifiScanView_ = nullptr;
    // ... move all static members here
};
```

**3. Extract internal header dependency (~20 min)**
Create a small forward-declaration header:
```cpp
// components/cdc_os_ui/WifiMenuTypes.h
#pragma once
namespace cdc::ui {
struct WifiItem { /* ... */ };
}
```

Replace `#include "AppUiInternal.h"` with forward declarations where possible.

**4. Dependency injection for callbacks (~20 min)**
Instead of static callbacks:
```cpp
// Current (line 208-235):
static void onWifiMainSelect(uint16_t index, void* userData);

// Better:
class WifiMenuManager {
public:
    using SelectCallback = std::function<void(uint16_t, void*)>;
    void setOnSelect(SelectCallback cb);
private:
    SelectCallback onSelect_;
};
```

## References

- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
- [Namespace pollution](https://en.wikipedia.org/wiki/Name_collision)
- [Encapsulation](https://en.wikipedia.org/wiki/Encapsulation_(computer_programming))

</content>