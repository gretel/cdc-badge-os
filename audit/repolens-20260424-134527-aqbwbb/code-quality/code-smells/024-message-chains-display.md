---
title: "[MEDIUM] Message Chains: Display rendering uses long method chains"
severity: MEDIUM
domain: cdc_views
lens: code-smells
labels:
  - "refactor:hide-decorators"
  - "maintainability"
---

## Summary
Multiple view components use long chains of method calls to access the display, creating tight coupling to the display's internal structure and making code harder to modify.

**Location:** Throughout `cdc_views` components

## Evidence
```cpp
// ListView.cpp:184-191 - Long chain to get native handle
hal::IDisplay* display = hal::getDisplayInstance();
if (!display) return;

auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
if (!gfx) return;

const uint16_t width = display->getWidth();
const uint16_t height = display->getHeight();

// Then using gfx extensively...
gfx->fillScreen(EPD_WHITE);
gfx->setTextColor(EPD_BLACK);
gfx->setTextSize(1);
```

```cpp
// DateInputView.cpp:209-216 - Same pattern
hal::IDisplay* display = hal::getDisplayInstance();
if (!display) return;

auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
if (!gfx) return;

const uint16_t width = display->getWidth();
const uint16_t height = display->getHeight();
```

```cpp
// TimeInputView.cpp:169-176 - Repeated pattern
hal::IDisplay* display = hal::getDisplayInstance();
if (!display) return;

auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
if (!gfx) return;

const uint16_t width = display->getWidth();
const uint16_t height = display->getHeight();
```

## Impact
- **Tight coupling**: Every view knows about `IDisplay`, `getNativeHandle()`, and `Gdey029T94`
- **Fragile code**: Changing display implementation requires updating all views
- **Code duplication**: Same pattern repeated in every view
- **Hard to test**: Difficult to mock the display chain

## Recommended Fix
Introduce a display wrapper class that encapsulates the chain:

```cpp
// Add to IDisplay.h or new header
class DisplayContext {
public:
    DisplayContext(hal::IDisplay* display) : display_(display) {
        if (display_) {
            width_ = display_->getWidth();
            height_ = display_->getHeight();
            gfx_ = static_cast<Gdey029T94*>(display_->getNativeHandle());
        }
    }

    Gdey029T94* getGfx() const { return gfx_; }
    uint16_t getWidth() const { return width_; }
    uint16_t getHeight() const { return height_; }
    hal::IDisplay* getDisplay() const { return display_; }

    bool isValid() const { return gfx_ != nullptr; }

private:
    hal::IDisplay* display_;
    Gdey029T94* gfx_ = nullptr;
    uint16_t width_ = 0;
    uint16_t height_ = 0;
};

// Usage in views:
void ListView::render(bool partial) {
    DisplayContext ctx(hal::getDisplayInstance());
    if (!ctx.isValid()) return;

    auto* gfx = ctx.getGfx();
    const uint16_t width = ctx.getWidth();
    const uint16_t height = ctx.getHeight();

    // Use ctx and gfx...
}
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Hide Decorators, Chain of Responsibilities
- Law of Demeter: Objects should only talk to their immediate neighbors
