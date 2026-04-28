---
title: "[LOW] Display Initialization Failure Silent - No Fallback or Status Indication"
severity: LOW
domain: ui-system
lens: graceful-degradation
labels:
  - "audit:error-handling/graceful-degradation"
---

## Summary
In `main/main.cpp:196-205`, when display initialization fails, only an error log is printed. The system continues without any visual feedback or indication that the display is unavailable. Since the display is a primary output device, this creates a "black screen" scenario where users cannot see status, errors, or any feedback.

Lines of interest:
- `main.cpp:196-205` - Display init with silent failure
- `main.cpp:210-220` - UI initialization continues even if display failed

## Impact
When display initialization fails:
1. **No visual feedback** - users see nothing on the screen
2. **Silent failure** - only visible via serial output (which may not be connected)
3. **System appears frozen** - no indication of what state system is in
4. **UI continues without display** - `ui_init()` is called regardless of display state

## Evidence
```cpp
// main.cpp:196-205
LOG_I(TAG, "Initializing Display...");
cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
if (display && display->init() && display->start()) {
    LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());
    // Show boot splash
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        display->showSplash("Waking up...");
    } else {
        display->showSplash();
    }
} else {
    LOG_E(TAG, "Display init failed!");  // Silent failure
}

// main.cpp:210-220
LOG_I(TAG, "Initializing UI...");
cdc::ui::UiDeps deps;
deps.display = display;  // Could be nullptr
// ...
cdc::ui::ui_init(deps);  // Continues even with nullptr display
```

## Recommended Fix
Implement display failure handling:

1. **Add display status check with fallback**:
   ```cpp
   // main.cpp
   cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
   bool displayReady = (display && display->init() && display->start());
   
   if (displayReady) {
       LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());
       display->showSplash();
   } else {
       LOG_E(TAG, "Display init failed - system will run in headless mode");
       // Try to indicate failure via LED or serial
       #ifdef DEBUG_MODE
       // Blink LED to indicate display failure
       #endif
   }
   ```

2. **Add display availability check to UI**:
   ```cpp
   // AppUi.cpp
   void ui_init(UiDeps deps) {
       if (!deps.display) {
           LOG_W(TAG, "No display - UI will be minimal");
           // Initialize minimal UI (just status icons updated via serial)
           return;
       }
       // Full UI initialization
   }
   ```

3. **Add display error event to EventBus** - notify modules of display failure so they can adapt (e.g., use serial output instead).

4. **Add runtime display recovery** - retry display init periodically in case of transient failure.

## References
- [Embedded UI Failure Modes](https://www.embedded.com/design/prototyping-and-development/4024616/Graceful-degradation-in-embedded-systems)
