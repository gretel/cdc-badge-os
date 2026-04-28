---
title: "[LOW] USB CDC Started After All Modules Instead of Parallel"
severity: LOW
domain: startup-perf
lens: startup-prep
labels:
  - "audit:performance/startup-perf"
---

## Summary
USB CDC is started at line 233 of `main/main.cpp` after all modules have been initialized. However, USB initialization (`usb_cdc_init()`) happens early at line 76. The USB stack can be started earlier (after init, before modules) to allow USB enumeration to happen in parallel with module initialization.

**Evidence:**
```cpp
// main/main.cpp:76-77
if (!usb_cdc_init()) {
    return;
}

// ... 150 lines of other initialization ...

// main/main.cpp:233
usb_cdc_start();  // USB enumeration starts here
```

USB enumeration typically takes 50-100ms. Starting it earlier allows this time to overlap with module initialization.

## Impact
- **Sequential Delays**: USB enumeration (50-100ms) happens after modules init instead of in parallel
- **User Perception**: First serial connection appears later than necessary
- **Missed Overlap**: Module initialization could happen while USB is enumerating

## Evidence
File: `main/main.cpp` lines 76-77, 233
- `usb_cdc_init()` called early (line 76)
- `usb_cdc_start()` deferred until after all modules (line 233)
- Comment at line 235: "Finalize USB configuration"

File: `components/usb_badge/usb_cdc.cpp` lines 140-165
- `usb_cdc_start()` creates USB task and starts TinyUSB stack
- This triggers USB enumeration on the bus

## Recommended Fix
Move `usb_cdc_start()` to immediately after `usb_cdc_init()`:

```cpp
// main/main.cpp:76-80
if (!usb_cdc_init()) {
    return;
}

// Start USB stack early to allow parallel enumeration
usb_cdc_start();

LOG_I(TAG, "USB CDC ready");
```

This allows USB enumeration to begin while other hardware (I2C, keypad, display) is being initialized. The serial output will be available sooner for debugging.

**Note**: The existing "early debug" mode (`CONFIG_USB_EARLY_DEBUG`) already does this, but production mode defers unnecessarily.

## References
- USB enumeration typically 50-100ms
- ESP32-S3 USB OTG can enumerate while other peripherals initialize
- The code already supports early start via `CONFIG_USB_EARLY_DEBUG` config option
