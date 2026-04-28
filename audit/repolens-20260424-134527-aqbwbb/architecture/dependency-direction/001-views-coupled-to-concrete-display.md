---
title: "[HIGH] UI Views tightly coupled to concrete Gdey029T94 display implementation"
severity: HIGH
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
---

## Summary

The UI views layer (`cdc_views` component) is tightly coupled to the concrete `Gdey029T94` display implementation instead of using the `IDisplay` interface abstraction. This creates an inner-layer (views) depending on an outer-layer (hardware-specific implementation) violation.

**Affected files:**
- `components/cdc_views/src/ListView.cpp:14` - `#include <goodisplay/gdey029T94.h>`
- `components/cdc_views/src/ListView.cpp:189` - `static_cast<Gdey029T94*>(display->getNativeHandle())`
- `components/cdc_views/src/RenderHelpers.h:5` - Forward declaration `class Gdey029T94;`
- `components/cdc_views/src/RenderHelpers.h:12-20` - All render functions take `Gdey029T94*` as parameter
- `components/cdc_views/src/ToastView.cpp:81`
- `components/cdc_views/src/QRCodeView.cpp:179,213,287`
- `components/cdc_views/src/TimeInputView.cpp:173`
- `components/cdc_views/src/ContextMenuView.cpp:138`
- `components/cdc_views/src/ConfirmView.cpp:69`
- `components/cdc_views/src/DateInputView.cpp:214`
- `components/cdc_views/src/MessageBox.cpp:96`
- `components/cdc_views/src/SliderView.cpp:142`
- `components/cdc_views/src/PinEntryView.cpp:234`
- `components/cdc_views/src/T9InputView.cpp:275`
- `components/cdc_views/src/InfoView.cpp:156`

**Also affected (modules depending on concrete display):**
- `components/mod_totp/src/TotpModule.cpp:15,394`
- `components/grove_led/src/RgbInputView.cpp:190`
- `components/mod_hid/src/HidModule.cpp:101`

## Impact

1. **Reduced portability**: The views layer cannot work with any other display type without modification
2. **Testing difficulty**: Unit testing requires the concrete display implementation
3. **Tight coupling**: Changes to Gdey029T94 API ripple through all views
4. **Architectural violation**: Views (inner layer) should depend on abstractions (IDisplay), not infrastructure (concrete display class)

## Evidence

**In `components/cdc_views/src/ListView.cpp`:**
```cpp
#include <goodisplay/gdey029T94.h>  // Concrete dependency

void ListView::render(bool partial) {
    auto* display = hal::getDisplayInstance();
    if (!display) return;
    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());  // Cast to concrete type
    // ... uses Gdey029T94-specific methods
}
```

**In `components/cdc_views/include/cdc_views/RenderHelpers.h`:**
```cpp
class Gdey029T94;  // Forward declaration of concrete type

void drawHeaderLeft(Gdey029T94* gfx, const char* title, int x, int y, ...);
void drawFooterBar(Gdey029T94* gfx, uint16_t width, uint16_t height, ...);
// All functions depend on concrete Gdey029T94 type
```

## Recommended Fix

**Option 1 (Preferred): Add drawing methods to IDisplay interface**
1. Add the necessary drawing methods to `components/cdc_hal/include/cdc_hal/IDisplay.h`:
   ```cpp
   virtual void drawHeaderLeft(const char* title, int x, int y, uint16_t width) = 0;
   virtual void drawFooterBar(uint16_t width, uint16_t height, const char* prefix, const char* hint) = 0;
   // ... other common drawing operations
   ```

2. Implement these methods in `components/cdc_hal/src/EpaperDisplay.cpp`

3. Update all views to use `IDisplay` methods instead of casting to `Gdey029T94`

**Option 2: Create a display wrapper class**
1. Create `components/cdc_hal/include/cdc_hal/DisplayGfx.h` with a `DisplayGfx` class that wraps `Gdey029T94`
2. Use `DisplayGfx*` in view render methods instead of `Gdey029T94*`
3. Factory function to create `DisplayGfx` from `IDisplay*`

**Scope estimate:** 1-2 hours for Option 2 (simpler, less invasive)

## References

- Clean Architecture principles: Domain/inner layers should not depend on infrastructure/outer layers
- Dependency Inversion Principle: High-level modules should not depend on low-level modules; both should depend on abstractions
- Current `IDisplay` interface: `components/cdc_hal/include/cdc_hal/IDisplay.h`
