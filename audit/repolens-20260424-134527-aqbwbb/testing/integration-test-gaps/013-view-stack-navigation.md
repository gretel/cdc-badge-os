---
title: "[HIGH] ViewStack navigation lacks integration tests for view lifecycle and key dispatch"
severity: HIGH
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_ui"
  - "area:view-stack"
---

## Summary
The `ViewStack` component (`components/cdc_ui/include/cdc_ui/ViewStack.h`) manages view navigation (push/pop/replace) and key dispatch, but **no integration tests** verify that views receive correct lifecycle callbacks and keys are dispatched properly through the stack.

## Impact
- **View lifecycle bugs**: `onEnter()`/`onExit()` may not be called in correct order
- **Key dispatch failures**: Keys may go to wrong view or be lost
- **Stack corruption**: Push/pop operations may leave stack in invalid state
- **Modal overlay issues**: Modal views may not block background views correctly

## Evidence

**ViewStack API** (`components/cdc_ui/include/cdc_ui/ViewStack.h:17-173`):
```cpp
class ViewStack {
    void push(IView* view, void* context = nullptr);
    void pop();
    void replace(IView* view, void* context = nullptr);
    void popToRoot();
    IView* current() const;
    void dispatchKey(char key);
    void dispatchLongPress(char key);
};
```

**IView interface** (`components/cdc_ui/include/cdc_ui/IView.h`):
```cpp
class IView {
    virtual void onEnter(void* context);
    virtual void onExit();
    virtual void onKey(char key);
    virtual void onLongPress(char key);
    virtual void render();
};
```

**Usage in AppUi** (`components/cdc_os_ui/src/AppUi.cpp:100-300`):
```cpp
// ViewStack is used extensively for navigation
ViewStack::instance().push(listView);
ViewStack::instance().dispatchKey('Y');
```

**Views using ViewStack**:
- `PinEntryView` - PIN entry with verify callback
- `ListView` - Scrollable lists with select callback
- `T9InputView` - Text input with save callback
- `PinChangeView` - PIN change flow

**Current test coverage**: None

## Recommended Fix

Create integration test `test_view_stack_integration/` that verifies:

1. **View lifecycle**: `onEnter()` called after push, `onExit()` called after pop
2. **Key dispatch**: Keys go to current (top) view only
3. **Long press dispatch**: Long presses reach current view
4. **Stack operations**: push/pop/replace maintain correct state
5. **Context passing**: Context parameter reaches `onEnter()` correctly
6. **Modal behavior**: Modal views block background view input

**Test structure** (example):
```cpp
// test/test_view_stack_integration/test_view_stack.cpp
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/IView.h"

class TestView : public ui::IView {
    bool onEnterCalled = false;
    bool onExitCalled = false;
    char lastKey = 0;
    
    void onEnter(void* context) override {
        onEnterCalled = true;
    }
    
    void onExit() override {
        onExitCalled = true;
    }
    
    void onKey(char key) override {
        lastKey = key;
    }
};

void test_view_lifecycle() {
    TestView view;
    ui::ViewStack& stack = ui::ViewStack::instance();
    
    stack.push(&view);
    ASSERT_TRUE(view.onEnterCalled);
    
    stack.pop();
    ASSERT_TRUE(view.onExitCalled);
}

void test_key_dispatch() {
    TestView view;
    ui::ViewStack& stack = ui::ViewStack::instance();
    
    stack.push(&view);
    stack.dispatchKey('Y');
    
    ASSERT_EQ(view.lastKey, 'Y');
}
```

## References
- [ViewStack header](components/cdc_ui/include/cdc_ui/ViewStack.h)
- [ViewStack implementation](components/cdc_ui/src/ViewStack.cpp)
- [IView interface](components/cdc_ui/include/cdc_ui/IView.h)
- [AppUi usage](components/cdc_os_ui/src/AppUi.cpp)

</content>