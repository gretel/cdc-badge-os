---
title: "[MEDIUM] HID module coupled to display for badge update"
severity: MEDIUM
domain: architecture/dependency-direction
lens: dependency-direction
labels:
  - "audit:architecture/dependency-direction"
---

## Summary

The HID module (`mod_hid`) depends on the display implementation to show badge updates, creating a cross-cutting concern where a "keyboard provider" module needs to know about display hardware.

**Affected file:**
- `components/mod_hid/src/HidModule.cpp:101` - `static_cast<Gdey029T94*>(display->getNativeHandle())`

**Evidence:**
```cpp
// components/mod_hid/src/HidModule.cpp
#include <goodisplay/gdey029T94.h>

void updateBadgeText() {
    auto* display = hal::getDisplayInstance();
    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    // Uses Gdey029T94-specific methods
}
```

## Impact

1. **Module coupling**: A module that provides keyboard functionality shouldn't need to know about display implementation
2. **Violation of single responsibility**: HID module has both keyboard and display responsibilities
3. **Harder to test**: Display dependency makes unit testing harder

## Evidence

**In `components/mod_hid/src/HidModule.cpp`:**
```cpp
// Line 101
auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
```

The module needs `#include <goodisplay/gdey029T94.h>` just to update the badge display.

## Recommended Fix

1. Create a separate `BadgeDisplayService` or use existing `IDisplay` interface methods
2. Move badge text update logic to `cdc_os_ui` or a dedicated badge component
3. HID module should only handle keyboard functionality

**Scope estimate:** 1 hour

## References

- Single Responsibility Principle: HID module should only handle keyboard
- Module isolation: Modules should be self-contained and removable
