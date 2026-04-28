---
title: "[MEDIUM] ViewStack singleton creates implicit coupling for views"
severity: MEDIUM
domain: architecture/coupling
lens: coupling-analysis
labels:
  - "view-coupling"
  - "navigation-stack"
---

## Summary
The `ViewStack` singleton pattern creates implicit coupling where all views must depend on the global singleton to navigate. Views call `ViewStack::instance().push()` directly instead of receiving a reference, making views hard to test and creating hidden dependencies.

**Evidence:**
- `components/cdc_ui/src/ViewStack.cpp:28-31`: Singleton pattern implementation
- `components/grove_led/src/GroveLedModule.cpp:271, 298, 319, 340`: Views call `ViewStack::instance().push()` directly
- No way to inject a mock ViewStack for testing

## Impact
1. **Testing difficulty**: Cannot unit test views without initializing the singleton ViewStack
2. **Hidden coupling**: View navigation depends on global state not visible in view interface
3. **Hard to reuse**: Views cannot easily be used in different navigation contexts
4. **Single navigation stack**: Cannot have multiple independent navigation stacks (e.g., for split views)

## Evidence
File: `components/cdc_ui/src/ViewStack.cpp`
```cpp
// Lines 28-31: Singleton pattern
ViewStack& ViewStack::instance() {
    static ViewStack instance;
    return instance;
}
```

File: `components/grove_led/src/GroveLedModule.cpp`
```cpp
// Lines 271, 298, 319, 340: Direct singleton access
ui::ViewStack::instance().push(s_ledCountSlider);
ui::ViewStack::instance().push(s_brightnessSlider);
ui::ViewStack::instance().push(s_rgbInput);
ui::ViewStack::instance().push(s_effectMenu);
```

## Recommended Fix
1. **Inject ViewStack into views**:
   ```cpp
   class IView {
   public:
       virtual void setViewStack(ViewStack* stack) { viewStack_ = stack; }
   protected:
       ViewStack* viewStack_ = nullptr;
   };
   ```

2. **Or pass ViewStack to onEnter**:
   ```cpp
   virtual void onEnter(void* context = nullptr, ViewStack* stack = nullptr) = 0;
   ```

3. **Factory pattern for views**: Create views with injected dependencies:
   ```cpp
   class ViewFactory {
   public:
       IView* createLedCountView(ViewStack* stack);
   };
   ```

4. **For modules**: Store ViewStack reference in module, pass to helper functions:
   ```cpp
   static void showLedCountView(ViewStack* stack) {
       auto* slider = new ui::SliderView();
       slider->init(...);
       stack->push(slider);
   }
   ```

## References
- ViewStack implementation: `components/cdc_ui/src/ViewStack.cpp`
- View interface: `components/cdc_ui/include/cdc_ui/IView.h`
- Module usage: `components/grove_led/src/GroveLedModule.cpp:271-340`
