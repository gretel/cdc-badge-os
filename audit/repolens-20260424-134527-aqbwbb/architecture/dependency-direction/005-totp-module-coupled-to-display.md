---
title: "[MEDIUM] TOTP module coupled to concrete display implementation"
severity: MEDIUM
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
---

## Summary

The TOTP module (`mod_totp`) directly depends on `Gdey029T94` display implementation to render the code view, violating the module isolation principle and creating a dependency from a business logic module to infrastructure.

**Affected file:**
- `components/mod_totp/src/TotpModule.cpp:15` - `#include <goodisplay/gdey029T94.h>`
- `components/mod_totp/src/TotpModule.cpp:394` - `static_cast<Gdey029T94*>(display->getNativeHandle())`

**Evidence:**
```cpp
// components/mod_totp/src/TotpModule.cpp
#include <goodisplay/gdey029T94.h>

class TotpCodeView : public ui::ViewBase {
    void render(bool partial) override {
        auto* display = hal::getDisplayInstance();
        if (!display) return;
        auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
        // Uses Gdey029T94-specific methods
        gfx->fillScreen(EPD_WHITE);
        gfx->setTextSize(2);
        // ...
    }
}
```

## Impact

1. **Module coupling**: TOTP module cannot be used without the GDEY029T94 display
2. **Reduced reusability**: Module is not portable to other hardware
3. **Testing difficulty**: Requires display implementation for testing

## Evidence

**In `components/mod_totp/src/TotpModule.cpp:394-430`:**
```cpp
void render(bool partial) override {
    auto* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    if (!partial) {
        gfx->fillScreen(EPD_WHITE);
    }

    gfx->setTextColor(EPD_BLACK);
    gfx->setTextSize(1);
    gfx->setCursor(8, 6);
    gfx->print(mstr(STR_CODE));
    // ... more Gdey029T94-specific calls
}
```

## Recommended Fix

1. Use `IDisplay` interface methods instead of casting to `Gdey029T94`
2. Add necessary drawing methods to `IDisplay` interface (see issue 001)
3. TOTP module should only depend on `cdc_hal` for display abstraction

**Scope estimate:** 30-45 minutes

## References

- Module Architecture: Modules should be self-contained and isolated
- Dependency Direction: Domain modules should not depend on infrastructure
