---
title: "[MEDIUM] UI components directly access HAL interfaces without abstraction"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
UI views directly call HAL (Hardware Abstraction Layer) interfaces like `hal::getDisplayInstance()`, `hal::IWifiController`, `core::PinManager` instead of going through a service layer. This couples presentation code directly to hardware implementation details.

**Location**: Multiple files including:
- `components/grove_led/src/RgbInputView.cpp:187`
- `components/mod_totp/src/TotpModule.cpp:389`
- `components/cdc_os_ui/src/AppUi.cpp:192`

## Impact
- **Hardware coupling**: UI code cannot be tested without hardware mock setup
- **Fragile changes**: Hardware interface changes break UI code
- **No clear layering**: Presentation layer knows about infrastructure (HAL)
- **Limited reusability**: Views cannot be reused in different hardware contexts

## Evidence
```cpp
// RgbInputView.cpp:187 - UI directly accesses HAL
void RgbInputView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();  // Direct HAL access
    if (!display) return;
    
    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    // ... rendering code ...
}

// TotpModule.cpp:389 - UI view directly accesses HAL
void TotpCodeView::render(bool partial) override {
    auto* display = hal::getDisplayInstance();  // Direct HAL access
    if (!display) return;
    
    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    // ...
}

// AppUi.cpp:192 - UI directly accesses multiple HAL services
void updatePowerStatusIcons() {
    auto* wifi = hal::getWifiControllerInstance();  // Direct HAL access
    auto* ble = hal::getBluetoothControllerInstance();  // Direct HAL access
    // ...
}
```

## Recommended Fix
1. **Create a ViewContext abstraction**:
   ```cpp
   class ViewContext {
   public:
       virtual hal::IDisplay* getDisplay() = 0;
       virtual hal::IKeypad* getKeypad() = 0;
       virtual core::PinManager* getPinManager() = 0;
       virtual hal::IPowerManager* getPowerManager() = 0;
   };
   ```

2. **Inject ViewContext into views**:
   ```cpp
   class RgbInputView : public ui::ViewBase {
       ViewContext& ctx_;  // Dependency injection
   public:
       void render(bool partial) override {
           auto* display = ctx_.getDisplay();  // Abstracted access
           // ...
       }
   };
   ```

3. **Implement ViewContext as a service** that wraps HAL calls

## References
- [Dependency Injection](https://en.wikipedia.org/wiki/Dependency_injection)
- [Hardware Abstraction Layer](https://en.wikipedia.org/wiki/Hardware_abstraction)
- [Presentation Model pattern](https://en.wikipedia.org/wiki/Presentation_model)
