---
title: "[MEDIUM] E-Paper display refresh blocks boot sequence"
severity: MEDIUM
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The E-Paper display initialization (`main.cpp:191-211`) is a synchronous blocking operation that happens before the UI is ready. E-Paper displays are slow (typically 2-5 seconds for full refresh), and the boot splash is shown immediately after init, blocking the user from seeing any feedback until the display is fully ready.

**Location:** `main/main.cpp:191-211`

## Impact
- **Long time-to-first-pixel**: User waits for display init + splash render before seeing anything
- **Blocking SPI transfer**: E-Paper displays use SPI with large framebuffers (~1.7KB for 290x128)
- **No early feedback**: System could be responsive (USB CDC, serial commands) while display initializes

## Evidence
From `main/main.cpp:191-211`:
```cpp
// === DISPLAY INITIALIZATION ===
LOG_I(TAG, "Initializing Display...");
cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
if (display && display->init() && display->start()) {
    LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());

    // Show boot splash screen (with wakeup text if resuming from deep sleep)
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        display->showSplash("Waking up...");
    } else {
        display->showSplash();
    }
    LOG_I(TAG, "Splash screen done");
} else {
    LOG_E(TAG, "Display init failed!");
}
```

The `showSplash()` call triggers a full E-Paper refresh which takes 2-5 seconds.

## Recommended Fix
**Defer display splash to after UI is ready**:

1. Initialize display early (hardware setup)
2. Defer splash screen to after modules are initialized
3. Show a simple "booting" indicator first (LED or USB status)

```cpp
// Phase 1: Init display hardware only
if (display && display->init()) {
    LOG_I(TAG, "Display initialized (not started)");
}

// ... continue with other init ...

// Phase 2: Show splash after modules ready
if (display && display->start()) {
    display->showSplash();  // Happens later in boot
}
```

**Alternative**: Show splash on a timer
```cpp
// Start splash after 1 second delay
xTimerCreate("splash", pdMS_TO_TICKS(1000), pdFALSE, 
             display, [](TimerHandle_t t) {
                 auto* disp = (cdc::hal::IDisplay*)timerGetId(t);
                 disp->showSplash();
             });
```

## References
- E-Paper displays: [Refresh time](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module) - 2-5 seconds typical
- ESP-IDF: [SPI bus initialization](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html)