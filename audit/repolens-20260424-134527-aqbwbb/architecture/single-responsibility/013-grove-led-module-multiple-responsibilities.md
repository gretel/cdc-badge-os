---
title: "[MEDIUM] GroveLedModule combines hardware control, settings persistence, UI rendering, and menu management"
severity: MEDIUM
domain: grove_led
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
  - "component:grove_led"
---

## Summary
`components/grove_led/src/GroveLedModule.cpp` (651 lines) combines four distinct responsibilities:
1. **Hardware control** - LED strip initialization, color updates, rainbow/static effects
2. **Settings persistence** - NVS read/write for LED count, brightness, color, effect
3. **UI rendering** - Creates and manages `ListView`, `SliderView`, `RgbInputView` instances
4. **Menu management** - Builds main menu items, handles selection callbacks, i18n string registration

Key code evidence:
- Lines 100-150: Singleton initialization with NVS loading
- Lines 200-250: `GroveLedModule::init()` creates UI views (ListView, SliderView, InfoView)
- Lines 250-350: Menu building with `rebuildMainMenu()`, callback wiring
- Lines 400-500: Hardware control (`updateRainbow()`, `updateStaticColor()`, `clearLeds()`)
- Lines 500-600: NVS persistence (`loadSettings()`, `saveSettings()`)
- Lines 60-90: i18n string registration for English and German

## Impact
**Maintenance burden**: Adding a new LED effect requires modifying hardware logic, UI, and menu code in the same file.

**Testability**: Hardware testing requires UI stubs and NVS mocking because all concerns are tightly coupled.

**Reusability**: The module cannot be reused in a headless context (e.g., CLI-only mode) because UI code is embedded.

**Memory efficiency**: UI views are allocated even when the module runs in a minimal configuration without display.

## Evidence
File: `components/grove_led/src/GroveLedModule.cpp`

Lines 200-220 (UI view creation):
```cpp
s_mainMenu = new ListView();
s_brightnessSlider = new SliderView();
s_colorView = new InfoView();
s_rgbInput = new RgbInputView();
```

Lines 250-280 (Menu building):
```cpp
static void rebuildMainMenu() {
    s_mainMenuItems[MENU_LED_COUNT] = {mstr(STR_LED_COUNT), 0, false, nullptr};
    s_mainMenuItems[MENU_BRIGHTNESS] = {mstr(STR_BRIGHTNESS), 0, false, nullptr};
    // ...
}
```

Lines 400-450 (Hardware control):
```cpp
void GroveLedModule::updateRainbow(uint32_t nowMs) {
    // LED strip hardware manipulation
    for (uint8_t i = 0; i < ledCount_; i++) {
        led_strip_set_pixel(...);
    }
    led_strip_refresh();
}
```

Lines 500-550 (NVS persistence):
```cpp
void GroveLedModule::loadSettings() {
    nvs_handle_t nvs;
    nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    nvs_get_u8(nvs, NVS_KEY_LED_COUNT, &ledCount_);
    // ...
}
```

## Recommended Fix
**Split into focused components** (each 1 hour task):

1. **Extract hardware control**: Create `GroveLedDriver` class with pure hardware API (init, setColor, setEffect, refresh). Move `updateRainbow()`, `updateStaticColor()`, `clearLeds()` here.

2. **Extract settings manager**: Create `GroveLedSettings` class with NVS read/write. Provide `load()`, `save()`, `getLedCount()`, `setBrightness()`, etc.

3. **Extract UI controller**: Create `GroveLedUiController` class that:
   - Takes `GroveLedDriver` and `GroveLedSettings` as dependencies
   - Handles menu building and callback wiring
   - Manages view lifecycle

4. **Refactor `GroveLedModule`**: Keep only module interface implementation:
   - `init()`, `start()`, `stop()`
   - `getMenuItems()` delegates to UI controller
   - `onTick()` delegates to driver

**Files to create**:
- `components/grove_led/include/grove_led/GroveLedDriver.h`
- `components/grove_led/include/grove_led/GroveLedSettings.h`
- `components/grove_led/include/grove_led/GroveLedUiController.h`
- Corresponding `.cpp` files

**Migration steps**:
1. Create driver class, move hardware methods
2. Create settings class, move NVS methods
3. Create UI controller, move menu/view code
4. Update module to compose these three

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- ESP32 LED Strip: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/led_strip.html
- Embedded UI patterns: https://www.embedded.com/design/prototyping-and-development/4023983/Designing-user-interfaces-for-embedded-systems
