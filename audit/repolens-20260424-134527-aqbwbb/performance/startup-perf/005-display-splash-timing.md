---
title: "[LOW] Display splash screen shown before system ready"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The display splash screen is shown **immediately after display init** (`main.cpp:197-203`), but system initialization continues for ~50+ more lines of code (modules, UI, etc.). This creates a misleading "ready" state where display is active but system isn't fully initialized.

**Location:** `main/main.cpp:197-203`

## Impact
- **User confusion**: Display shows "CDC Badge" but buttons don't respond yet
- **Perceived slow startup**: User sees UI but system still initializing
- **Missed optimization**: Could show progress during remaining initialization

## Evidence
From `main/main.cpp:193-236`:
```cpp
// === DISPLAY INITIALIZATION ===
LOG_I(TAG, "Initializing Display...");
cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
if (display && display->init() && display->start()) {
    LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());

    // Show boot splash screen
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        display->showSplash("Waking up...");
    } else {
        display->showSplash();
    }
    LOG_I(TAG, "Splash screen done");
}

// === UI INITIALIZATION ===
LOG_I(TAG, "Initializing UI...");
// ... 30+ more lines of initialization ...

// === MODULE INITIALIZATION ===
modules_register_all();
cdc::core::ModuleRegistry::instance().runAllInitializers();
// ... 20+ more lines ...

LOG_I(TAG, "System ready. Entering main loop.");
```

Display shows splash at line 200, but "System ready" message is at line 236.

## Recommended Fix
**Option 1**: Show splash after all initialization
```cpp
// Move splash to end of init sequence
// After: LOG_I(TAG, "System ready. Entering main loop.");
display->showSplash();  // Now system is actually ready
```

**Option 2**: Show progress during initialization
```cpp
display->showSplash("Init: 20%");  // After I2C
display->updateSplash("Init: 40%");  // After Power
display->updateSplash("Init: 60%");  // After modules
display->showReady();  // Final state
```

**Option 3**: Show minimal splash, then UI
```cpp
// Quick splash for hardware init only
if (display) {
    display->showSplash("Booting...");  // Brief
}

// Full init
// ...

// Show main menu directly
ui::showMainMenu();  // Faster perceived boot
```

## References
- E-Paper display: [Slow refresh rate](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module_(B)) - 296x128 takes ~1-2s to refresh
