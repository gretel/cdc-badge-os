---
title: "[LOW] Main menu has no recommended/default action"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - "choice-paralysis"
  - "navigation"
---

## Summary
The main menu in `components/cdc_os_ui/src/AppUi.cpp` presents 2+ fixed items (Tools, Settings) plus dynamic module items with no visual prioritization or recommended action. Users must scan all options to decide what to do next.

**File:** `components/cdc_os_ui/src/AppUi.cpp`
**Lines:** 310-340 (rebuildMainMenu, rebuildToolsMenu)

## Impact
- **Decision Fatigue:** Users see all options equally and must decide without guidance
- **No Onboarding:** New users don't know what to do first
- **Hidden Features:** Module items are interspersed without highlighting important actions

## Evidence
The main menu structure:

```cpp
// Line 310-330: Main menu with no prioritization
void rebuildMainMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    s_mainMenuPluginCount = moduleReg.getMenuItems(
        core::MenuLocation::MAIN_MENU,
        s_mainMenuModuleItems,
        MAIN_MENU_MAX_ITEMS - MAIN_MENU_FIXED_COUNT
    );

    for (uint8_t i = 0; i < s_mainMenuPluginCount; i++) {
        s_mainMenuItems[i] = {s_mainMenuModuleItems[i].label, 0, false, nullptr};
    }

    s_mainMenuItems[getToolsIndex()] = {tr(StringId::TOOLS), 0, false, nullptr};
    s_mainMenuItems[getSettingsIndex()] = {tr(StringId::SETTINGS), 0, false, nullptr};

    if (s_mainMenu) {
        s_mainMenu->init(tr(StringId::MAIN_MENU), s_mainMenuItems, getMainMenuCount());
    }
}
```

All items are rendered with equal weight. No item is marked as "recommended" or "default".

Tools menu (line 330-350) also has 4 fixed items:
```cpp
s_toolsItems[0] = {tr(StringId::MODULES), 0, false, nullptr};
s_toolsItems[1] = {tr(StringId::WIFI_MENU), 0, false, nullptr};
s_toolsItems[2] = {tr(StringId::BLUETOOTH), ...};
s_toolsItems[3] = {tr(StringId::EXPERT), 0, false, nullptr};
```

Plus dynamic module items. Users see 4-16 options with no guidance.

## Recommended Fix
Add visual prioritization:

**Option 1:** Mark the first item as "default" with special styling:
```cpp
// In ListView render, check if item has a "default" flag
bool isDefault = (itemIndex == 0 && title_ == tr(StringId::MAIN_MENU));
if (isDefault) {
    // Draw with different background or prefix
    gfx->print("> ");
}
```

**Option 2:** Add a welcome/toast on first unlock:
```cpp
static void onPinSuccess() {
    ViewStack::instance().replace(s_mainMenu);
    core::ModuleRegistry::instance().dispatchUnlock();

    // Show welcome hint on first use
    if (!hasShownWelcome()) {
        showToastInfo("Press Y on an item to select", 3000);
        setWelcomeShown();
    }
}
```

**Option 3:** Highlight most common action with an asterisk or icon:
```cpp
// In rebuildMainMenu, mark frequently-used items
s_mainMenuItems[getToolsIndex()] = {tr(StringId::TOOLS), '*', false, nullptr};
```

## References
- Choice Architecture: Use defaults to guide decisions
- Progressive Disclosure: Show complexity gradually
- Nielsen Norman Group: "Help Users Make Decisions"
