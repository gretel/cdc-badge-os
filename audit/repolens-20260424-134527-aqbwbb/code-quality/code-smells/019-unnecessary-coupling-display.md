---
title: "[LOW] Unnecessary Coupling: Views depend on specific display driver"
severity: LOW
domain: ui
lens: code-smells
labels:
  - "coupling"
  - "cdc_views"
---

## Summary
In `components/cdc_views/`, views directly include and use `Gdey029T94` display driver instead of using the `hal::IDisplay` abstraction. This couples the views to a specific display hardware.

## Impact
**Limited flexibility**: Cannot easily switch to a different display.

**Testing difficulty**: Need to mock the specific driver.

**Tight coupling**: Views know too much about display internals.

## Evidence
`components/cdc_views/src/T9InputView.cpp:14`:
```cpp
#include <goodisplay/gdey029T94.h>
```

`components/cdc_views/src/T9InputView.cpp:266-270`:
```cpp
void T9InputView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;
    // ...
}
```

Same pattern in `TimeInputView.cpp:11` and `DateInputView.cpp:11`.

## Recommended Fix
1. **Use IDisplay methods directly**:
```cpp
void T9InputView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    if (!partial) {
        display->clear();
    }
    display->drawText(TEXT_MARGIN, TEXT_Y, text_);
    // ...
}
```

2. **Or create a display abstraction layer**:
```cpp
class DisplayView {
protected:
    void drawHeader(const char* text, int y);
    void drawFooter(const char* hint, int y);
    // ...
};
```

**Estimated effort**: ~1 hour to abstract display usage in 2-3 views.

## References
- Refactoring.com: "Excessive Coupling" - https://refactoring.com/catalog/moveMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
