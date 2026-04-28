---
title: "[MEDIUM] Display and Keypad HAL integration lacks end-to-end UI tests"
severity: MEDIUM
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_hal"
  - "area:ui-hardware"
---

## Summary
The display (E-Paper) and keypad (TCA9535) HAL components provide the main user interface, but **no integration tests** verify that key presses are detected, processed, and reflected on the display correctly.

## Evidence

**IDisplay API** (`components/cdc_hal/include/cdc_hal/IDisplay.h:18-149`):
```cpp
class IDisplay {
    void clear();
    void flush(RefreshMode mode);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void print(const char* text);
    void showSplash(const char* subtitle);
};
```

**IKeypad API** (`components/cdc_hal/include/cdc_hal/IKeypad.h:28-98`):
```cpp
class IKeypad {
    void poll();
    Key getNextKey();
    bool hasKey() const;
    void setCallback(KeyCallback callback);
    void setLongPressEnabled(bool enabled, uint32_t thresholdMs);
};
```

**Implementation** (`components/cdc_hal/src/EpaperDisplay.cpp`, `TCA9535Keypad.cpp`):
- E-Paper display via SPI with GDEY029T94 driver
- TCA9535 I2C GPIO expander for 12-key matrix
- Interrupt-driven key detection

**UI integration** (`components/cdc_ui/`, `components/cdc_views/`):
- ListView, T9InputView, PinEntryView use display and keypad
- No integration tests

**Usage in main** (`main/main.cpp:149-156, 193-203`):
```cpp
s_keypad = cdc::hal::getKeypadInstance();
s_keypad->init();

display = cdc::hal::getDisplayInstance();
display->init();
display->start();
display->showSplash();
```

**Current test coverage**: None

## Impact
- **Keypad debouncing**: May miss keys or register duplicates
- **Display refresh**: Ghosting or corruption from incorrect refresh modes
- **UI flow**: Navigation between views may have bugs
- **Long press**: Long-press detection not verified

## Recommended Fix

Create integration test `test_ui_hardware_integration/` that verifies:

1. **Keypad detection**: All 12 keys detected correctly
2. **Key buffer**: Multiple keys buffered and retrieved in order
3. **Long press**: Long-press triggers after threshold
4. **Display drawing**: Pixels, text, rectangles render correctly
5. **Display refresh**: Full and partial refresh work
6. **UI navigation**: View stack operations work

**Test structure** (example):
```cpp
// test/test_ui_hardware_integration/test_keypad.cpp
#include "cdc_hal/IKeypad.h"

void test_keypad_all_keys() {
    IKeypad* keypad = getKeypadInstance();
    keypad->init();
    
    // Test each key
    for (char k = '1'; k <= '9'; k++) {
        // Simulate key press (hardware-dependent)
        keypad->poll();
        ASSERT_TRUE(keypad->hasKey());
        ASSERT_EQ(keypad->getNextKey(), k);
    }
}

void test_keypad_long_press() {
    bool longPressTriggered = false;
    keypad->setLongPressEnabled(true, 500);  // 500ms for testing
    keypad->setLongPressCallback([](Key key) {
        longPressTriggered = true;
    });
    
    // Hold key for 600ms
    // Verify callback triggered
}
```

## References
- [IDisplay interface](components/cdc_hal/include/cdc_hal/IDisplay.h)
- [IKeypad interface](components/cdc_hal/include/cdc_hal/IKeypad.h)
- [EpaperDisplay implementation](components/cdc_hal/src/EpaperDisplay.cpp)
- [TCA9535Keypad implementation](components/cdc_hal/src/TCA9535Keypad.cpp)
