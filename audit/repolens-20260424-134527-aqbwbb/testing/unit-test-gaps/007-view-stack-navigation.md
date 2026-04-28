---
title: "[MEDIUM] ViewStack Navigation and Modal System Lacks Unit Test Coverage"
severity: MEDIUM
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `ViewStack` class (`components/cdc_ui/src/ViewStack.cpp`, 367 lines) manages UI navigation and modal overlays with no unit tests. Critical untested functions include:

- `push()` (line 40) - View push with context
- `pop()` (line 68) - View pop with state management
- `replace()` (line 96) - View replacement
- `popToRoot()` (line 128) - Pop to root
- `dispatchKey()` (line 159) - Key dispatch to modal/current
- `dispatchLongPress()` (line 182) - Long press with global N behavior
- `dispatchTick()` (line 217) - Tick to modal and current
- `render()` (line 234) - Render with refresh mode
- `showModal()` (line 286) - Modal display
- `hideModal()` (line 299) - Modal hide
- `checkInactivity()` (line 350) - Inactivity timeout

## Impact
**UI Navigation Risk:** ViewStack controls all screen navigation:
1. Stack overflow (MAX_DEPTH=8) is untested
2. `pop()` with depth=1 should noop - untested
3. `dispatchLongPress()` global N back behavior is unproven
4. Modal priority over current view is untested
5. `render()` refresh mode (FULL vs PARTIAL) logic is untested
6. `checkInactivity()` timeout callback is unproven

## Evidence
File: `components/cdc_ui/src/ViewStack.cpp`

Line 40-63: `push()` - Stack push
```cpp
void ViewStack::push(IView* view, void* context) {
    if (!view) {
        LOG_W(TAG, "Attempted to push null view");
        return;  // Null check
    }

    if (depth_ >= MAX_DEPTH) {
        LOG_E(TAG, "ViewStack overflow (max %d)", MAX_DEPTH);
        return;  // Overflow
    }

    stack_[depth_++] = view;
    view->onEnter(context);
    needsFullRefresh_ = !(isListView(view) && isListView(depth_ > 1 ? stack_[depth_ - 2] : nullptr));
}
```

Line 159-180: `dispatchKey()` - Modal priority
```cpp
void ViewStack::dispatchKey(char key) {
    resetInactivityTimer();

    if (modal_) {
        InputResult result = modal_->onKey(key);
        if (result == InputResult::REQUEST_POP) {
            hideModal();
        }
        return;  // Modal gets priority
    }

    InputResult result = view->onKey(key);
    if (result == InputResult::REQUEST_POP) {
        pop();
    }
}
```

Line 182-212: `dispatchLongPress()` - Global N back
```cpp
void ViewStack::dispatchLongPress(char key) {
    if (key == 'N') {
        if (modal_) {
            hideModal();
        } else if (depth_ > 1) {
            pop();
        }
        return;  // Universal back
    }

    if (modal_) {
        modal_->onLongPress(key);
        return;
    }

    view->onLongPress(key);
}
```

Line 234-270: `render()` - Refresh mode
```cpp
void ViewStack::render() {
    if (!view) return;

    bool modalNeedsRender = modal_ && modal_->needsRender();
    bool viewNeedsRender = view->needsRender();

    if (!viewNeedsRender && !modalNeedsRender) {
        return;  // Early exit
    }

    if (viewNeedsRender) {
        view->render(false);  // Full refresh
    }

    if (modal_ && modalNeedsRender) {
        modal_->render(true);  // Partial
    }

    hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
    display->flush(mode);
    needsFullRefresh_ = false;
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "ViewStack" {} \;
# Returns nothing - no ViewStack tests exist
```

## Recommended Fix
Create `test/test_view_stack/test_view_stack.cpp` with test cases:

1. **Stack operation tests:**
   - Test `push()` with valid view
   - Test `push()` with null view (noop)
   - Test `push()` with 8 views (overflow)
   - Test `pop()` with depth=1 (noop)
   - Test `pop()` decrements depth

2. **Navigation tests:**
   - Test `replace()` swaps top view
   - Test `popToRoot()` pops all but first
   - Test `current()` returns top view
   - Test `at(depth)` returns view at index

3. **Dispatch tests:**
   - Test `dispatchKey()` calls current view
   - Test `dispatchKey()` with modal calls modal
   - Test `dispatchLongPress('N')` always goes back
   - Test `dispatchTick()` calls both modal and current

4. **Modal tests:**
   - Test `showModal()` displays modal
   - Test `hideModal()` hides modal
   - Test `hasModal()` returns correct state

5. **Inactivity tests:**
   - Test `checkInactivity()` calls callback after timeout
   - Test `resetInactivityTimer()` resets timer

Example test:
```cpp
void test_push_overflow() {
    auto& stack = ViewStack::instance();
    for (int i = 0; i < 8; i++) {
        stack.push(new MockView("v1"));
    }
    TEST_ASSERT_EQUAL(8, stack.depth());
    
    stack.push(new MockView("v9"));  // Overflow
    TEST_ASSERT_EQUAL(8, stack.depth());  // Should not increase
}

void test_dispatch_long_press_N() {
    auto& stack = ViewStack::instance();
    stack.push(new MockView("root"));
    stack.push(new MockView("child"));
    
    stack.dispatchLongPress('N');
    TEST_ASSERT_EQUAL(1, stack.depth());  // Popped to root
}

void test_modal_priority_over_current() {
    auto& stack = ViewStack::instance();
    auto* view = new MockView("main");
    auto* modal = new MockView("modal");
    
    stack.push(view);
    stack.showModal(modal);
    
    stack.dispatchKey('A');
    TEST_ASSERT_TRUE(modal->keyACalled);  // Modal got key
    TEST_ASSERT_FALSE(view->keyACalled);  // View did not
}

void test_render_refresh_mode() {
    auto& stack = ViewStack::instance();
    stack.push(new MockView("main"));
    
    stack.render();  // Should use FULL (needsFullRefresh_ = true)
    // Verify display->flush(FULL) was called
}
```

## References
- File: `components/cdc_ui/include/cdc_ui/ViewStack.h` - Full API
- File: `components/cdc_ui/include/cdc_ui/IView.h` - View interface
