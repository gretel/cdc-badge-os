---
title: "[LOW] View Stack Missing onEnter/onExit Animation Hooks"
severity: LOW
domain: animation-transitions
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
The `IView` interface has `onEnter()`, `onExit()`, and `onResume()` lifecycle methods but no dedicated animation hooks. Views must implement their own timing logic for entrance/exit animations, leading to inconsistent patterns.

**Files affected:**
- `components/cdc_ui/include/cdc_ui/IView.h:27-46` - View lifecycle interface
- `components/cdc_ui/src/ViewStack.cpp:58, 78, 118` - Lifecycle method calls

## Impact
- **Code Duplication:** Each view implements its own animation timing
- **Inconsistency:** Different views may use different animation patterns
- **Maintainability:** Adding animation support requires modifying each view individually

## Evidence
```cpp
// IView.h:27-46
class IView {
    virtual void onEnter(void* context = nullptr) = 0;
    virtual void onExit() = 0;
    virtual void onResume() = 0;
    // No animation-specific hooks
};

// ViewStack.cpp:58
void ViewStack::push(IView* view, void* context) {
    stack_[depth_++] = view;
    view->onEnter(context);  // No animation support
    needsFullRefresh_ = !(isListView(view) && ...);
}

// ViewStack.cpp:78
void ViewStack::pop() {
    IView* top = stack_[--depth_];
    if (top) {
        top->onExit();  // No animation support
    }
}
```

## Recommended Fix
Add animation hooks to the `IView` interface:

1. **Extend `IView.h`:**
   ```cpp
   class IView {
       // Existing methods...
       
       // New animation hooks
       virtual void onEnterAnimation(float progress) { (void)progress; }
       virtual void onExitAnimation(float progress) { (void)progress; }
       virtual uint16_t getEnterDuration() { return 0; }  // 0 = no animation
       virtual uint16_t getExitDuration() { return 0; }
   };
   ```

2. **Update `ViewStack` to handle animations:**
   ```cpp
   void ViewStack::push(IView* view, void* context) {
       uint16_t duration = view->getEnterDuration();
       if (duration > 0) {
           // Animate in
           for (float p = 0; p <= 1; p += 0.1f) {
               view->onEnterAnimation(p);
               vTaskDelay(pdMS_TO_TICKS(duration / 10));
           }
       }
       view->onEnter(context);
   }
   ```

3. **Views can opt-in by implementing animation hooks**

## References
- iOS View Controller Lifecycle
- Android Activity Lifecycle with transitions
