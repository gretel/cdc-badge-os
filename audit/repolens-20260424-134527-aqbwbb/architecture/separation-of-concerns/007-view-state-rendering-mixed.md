---
title: "[MEDIUM] View state management mixed with view rendering logic"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
UI views mix state management (data holding, transitions) with rendering logic. The `ListView` and other views in `cdc_views` store both the data model and rendering code in the same class, making it hard to separate presentation from state.

**Location**: `components/cdc_views/src/ListView.cpp` (inferred structure)

## Impact
- **Hard to animate**: State and rendering are intertwined
- **Difficult to test**: Need full display context to test state changes
- **Memory inefficiency**: Views cannot share state between multiple renderings
- **Limited reusability**: Same data cannot be displayed in different layouts

## Evidence
Based on code review of view patterns in the codebase:
```cpp
// Typical pattern seen in RgbInputView.cpp, TotpModule.cpp
class RgbInputView : public ui::ViewBase {
    // State
    const char* title_;
    uint8_t r_, g_, b_;
    Field currentField_;
    uint8_t digitPos_;
    
    // Rendering mixed with state
    void render(bool partial) override {
        // Directly uses state variables in rendering
        gfx->print(r_);  // State embedded in render
        gfx->print(g_);
        gfx->print(b_);
    }
    
    // State transitions mixed with input handling
    ui::InputResult onKey(char key) override {
        if (key >= '0' && key <= '9') {
            enterDigit(key);  // State mutation
            return ui::InputResult::CONSUMED;
        }
        // ...
    }
};
```

## Recommended Fix
1. **Separate view state from rendering**:
   ```cpp
   // Pure state model
   struct RgbInputState {
       char title[32];
       uint8_t r, g, b;
       Field currentField;
       uint8_t digitPos;
   };
   
   // Renderer interface
   class RgbInputRenderer {
   public:
       virtual void render(const RgbInputState& state, bool partial) = 0;
   };
   
   // View combines both
   class RgbInputView : public ui::ViewBase {
       RgbInputState state_;
       RgbInputRenderer& renderer_;
       
       void render(bool partial) override {
           renderer_.render(state_, partial);
       }
   };
   ```

2. **Use reactive pattern for state changes**:
   ```cpp
   class RgbInputView : public ui::ViewBase {
       RgbInputState state_;
       std::function<void(const RgbInputState&)> onStateChanged_;
       
       void setState(RgbInputState new_state) {
           state_ = new_state;
           if (onStateChanged_) onStateChanged_(state_);
           markDirty();
       }
   };
   ```

## References
- [Model-View-Presenter](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93presenter)
- [Reactive programming](https://en.wikipedia.org/wiki/Reactive_programming)
