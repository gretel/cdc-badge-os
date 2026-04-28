---
title: "[LOW] Parallel Inheritance Hierarchies: View classes mirror ViewBase structure"
severity: LOW
domain: ui
lens: code-smells
labels:
  - "parallel-inheritance"
  - "cdc_ui"
---

## Summary
In `components/cdc_views/`, there's a parallel structure where each view class (T9InputView, TimeInputView, DateInputView, etc.) follows the exact same pattern: inherits from `ViewBase`, implements `render()`, `onKey()`, `getFooterHint()`. While this is intentional for polymorphism, the repetitive structure could be reduced.

## Impact
**Boilerplate**: Each new view requires the same 3-4 method implementations.

**Inconsistency**: Small variations creep in over time.

**Learning curve**: New developers must understand the pattern for each view.

## Evidence
`components/cdc_views/include/cdc_views/T9InputView.h`:
```cpp
class T9InputView : public ViewBase {
public:
    void init(const char* title, const char* initialText, uint16_t maxLen);
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "T9InputView"; }
    const char* getFooterHint() const override;
    // ...
};
```

`components/cdc_views/include/cdc_views/TimeInputView.h`:
```cpp
class TimeInputView : public ViewBase {
public:
    void init(const char* title, uint8_t hour, uint8_t minute);
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "TimeInputView"; }
    const char* getFooterHint() const override;
    // ...
};
```

`components/cdc_views/include/cdc_views/DateInputView.h`:
```cpp
class DateInputView : public ViewBase {
public:
    void init(const char* title, uint8_t day, uint8_t month, uint16_t year);
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "DateInputView"; }
    const char* getFooterHint() const override;
    // ...
};
```

All follow the exact same pattern.

## Recommended Fix
1. **Create a base input view**:
```cpp
class TextInputView : public ViewBase {
protected:
    const char* title_;
    char text_[MAX_TEXT_LEN];
    uint16_t len_;
    uint16_t maxLen_;
    bool dirty_;
    
    void renderHeader(Gdey029T94* gfx, int y, int width);
    void renderFooter(Gdey029T94* gfx, int width, int height, const char* hint);
};
```

2. **Derive specific views**:
```cpp
class T9InputView : public TextInputView {
    // Only implement T9-specific logic
};
```

**Estimated effort**: ~1 hour to create the base class and refactor 2-3 views.

## References
- Refactoring.com: "Parallel Inheritance Hierarchies" - https://refactoring.com/catalog/moveMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
