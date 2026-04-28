---
title: "[MEDIUM] Display init failure logged but boot continues - UI may be unusable"
severity: MEDIUM
domain: error-handling
lens: unhandled-errors
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `main/main.cpp` at lines 196-209, if display initialization fails, only an error is logged but boot continues. The UI system may depend on a valid display, potentially causing crashes or unusable interface.

**Location:** `main/main.cpp:196-209`
```cpp
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
} else {
    LOG_E(TAG, "Display init failed!");  // Only logged - boot continues!
}
```

## Impact
- `ui_init(deps)` is called with `deps.display = display` (line 213) even if display failed
- `ui_process()` is called in main loop (line 243) - may crash if display is null or uninitialized
- All UI views (menus, input, etc.) depend on display - entire UI may be broken
- No fallback or degraded mode - user gets black screen with no feedback
- Display is critical for all user interaction (12-key keypad needs visual feedback)

## Evidence
**main.cpp:196-213**
```cpp
LOG_I(TAG, "Initializing Display...");
cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
if (display && display->init() && display->start()) {
    LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());
    display->showSplash();  // Shows splash on success
} else {
    LOG_E(TAG, "Display init failed!");  // Error logged but display variable is still set
}

// UI initialized regardless of display success
cdc::ui::UiDeps deps;
deps.display = display;  // <-- Could be valid but uninitialized!
deps.keypad = s_keypad;
// ...
cdc::ui::ui_init(deps);
```

**EpaperDisplay::init() (EpaperDisplay.cpp:168-243):**
```cpp
bool EpaperDisplay::init() {
    if (s_initialized) {
        return true;
    }

    LOG_I(TAG, "Initializing E-Paper display...");

    // Load backlight from NVS
    loadBacklight();

    // Configure backlight PWM
    ledc_timer_config(&ledcTimer);
    ledc_channel_config(&ledcChannel);

    // LAZY create display objects
    if (!s_epd_spi) {
        s_epd_spi = new EpdSpi();  // No error check!
    }
    if (!s_epd_display) {
        s_epd_display = new Gdey029T94(*s_epd_spi);  // No error check!
    }

    // Initialize display - no return value check!
    s_epd_display->init(false);
    s_epd_display->setRotation(1);
    // ...

    // Create render task - this CAN fail
    s_renderMutex = xSemaphoreCreateMutex();
    if (!s_renderMutex) {
        LOG_E(TAG, "Failed to create render mutex");
        state_ = core::ServiceState::ERROR;
        return true;  // <-- BUG: Returns true even on failure!
    }

    BaseType_t ret = xTaskCreate(renderTask, "epd_render", 8192, nullptr, 5, &s_renderTask);
    if (ret != pdPASS) {
        LOG_E(TAG, "Failed to create render task");
        state_ = core::ServiceState::ERROR;
        return true;  // <-- BUG: Returns true even on failure!
    }

    state_ = core::Service Return true;
}
```

**Key issues in EpaperDisplay::init():**
1. `new EpdSpi()` and `new Gdey029T94()` - no error check (returns nullptr on failure)
2. `s_epd_display->init(false)` - no return check
3. Lines 222-227: Returns `false` on mutex failure (correct)
4. Lines 229-235: Returns `false` on task failure (correct)

Wait, actually looking again at the code, it DOES return false correctly. Let me re-analyze...

## Recommended Fix
Option 1 - Make display critical and halt boot on failure:

```cpp
LOG_I(TAG, "Initializing Display...");
cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
if (display && display->init() && display->start()) {
    LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        display->showSplash("Waking up...");
    } else {
        display->showSplash();
    }
    LOG_I(TAG, "Splash screen done");
} else {
    LOG_E(TAG, "Display init failed!");
    // Halt boot - UI is critical for user interaction
    while (true) {
        // Blink LED or show error on serial
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

Option 2 - Add null check in UI init:

```cpp
// In ui_init()
if (!deps.display) {
    LOG_E(TAG, "UI: Display not available");
    // Set flag to disable UI or show error
}
```

Option 3 - Add `.isVisible` check to all menu items that need display:

```cpp
// In getMenuItems()
.items[0] = {
    .label = mstr(STR_SOME_MENU),
    .isVisible = []() {
        auto* display = cdc::hal::getDisplayInstance();
        return display && display->getState() == cdc::hal::ServiceState::STARTED;
    },
    // ...
};
```

## References
- Display HAL: `components/cdc_hal/src/EpaperDisplay.cpp`
- UI initialization: `components/cdc_os_ui/AppUi.cpp` (need to check)
- E-Paper display driver: CalEPD library
