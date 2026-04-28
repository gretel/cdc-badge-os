---
title: "[MEDIUM] AppUi.cpp handles menu building, input processing, status updates, and view management"
severity: MEDIUM
domain: cdc_os_ui
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/cdc_os_ui/src/AppUi.cpp` (782 lines) handles multiple distinct responsibilities:
1. **Menu building** - `rebuildMainMenu()`, `rebuildToolsMenu()`, `rebuildMenuLabels()`
2. **Input processing** - keypad handling in `ui_process()`
3. **Status icon updates** - `updatePowerStatusIcons()`, `updateLockScreenClock()`
4. **View initialization** - all views created in `ui_init()`
5. **Callback routing** - `onMainMenuSelect()`, `onToolsSelect()`, `onSettingsSelect()`, `onLanguageSelect()`
6. **Serial command callbacks** - text/time callbacks registered in `ui_init()`
7. **Lock flow management** - `onUnlockRequested()`, `onPinVerify()`, `onPinSuccess()`

## Impact
- **High coupling**: Changes to menu structure, input handling, or status display all require modifying the same file
- **Difficult testing**: Cannot test menu building without initializing all views and dependencies
- **Merge conflicts**: Multiple developers working on different UI aspects will conflict on the same file
- **Hard to understand**: 782 lines with mixed concerns makes onboarding harder

## Evidence
File: `components/cdc_os_ui/src/AppUi.cpp`
- Lines 31-152: Static state and callback declarations
- Lines 154-230: Status icon rendering and updates
- Lines 248-300: Lock flow and PIN verification
- Lines 303-493: Menu building and selection callbacks
- Lines 496-717: View initialization (ui_init)
- Lines 739-782: Main UI tick loop with input, rendering, and sleep handling

Key function showing mixed concerns:
```cpp
void ui_process(uint32_t nowMs) {
    // Status icons
    updatePowerStatusIcons();
    // Clock
    updateLockScreenClock();
    // Keypad input
    if (s_deps.keypad) { ... }
    // Badge text
    settings::processPendingBadgeText();
    // View tick
    ViewStack::instance().dispatchTick(nowMs);
    // Rendering
    if (ViewStack::instance().needsRender()) { ... }
}
```

## Recommended Fix
Split into focused modules:
1. **MenuManager** - Extract all menu building and selection logic to `components/cdc_os_ui/src/MenuManager.cpp`
2. **StatusUpdater** - Extract status icon/clock updates to `components/cdc_os_ui/src/StatusUpdater.cpp`
3. **LockFlow** - Extract lock/unlock flow to `components/cdc_os_ui/src/LockFlow.cpp`
4. **UiInput** - Extract input processing to `components/cdc_os_ui/src/UiInput.cpp`

Each module should:
- Have its own header file
- Accept dependencies via constructor or init
- Be testable in isolation

Example split structure:
```
components/cdc_os_ui/src/
  AppUi.cpp          // Main orchestration only
  MenuManager.cpp    // Menu building logic
  StatusUpdater.cpp  // Status icon/clock updates
  LockFlow.cpp       // Unlock flow
  UiInput.cpp        // Input processing
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- Martin, R. C. (2008). Clean Code: A Handbook of Agile Software Craftsmanship
