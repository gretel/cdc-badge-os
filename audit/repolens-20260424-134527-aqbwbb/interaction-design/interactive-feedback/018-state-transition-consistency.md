---
title: "[LOW] Inconsistent state transition feedback across views"
severity: LOW
domain: interaction-design
lens: interactive-feedback
labels:
  - "audit:interaction-design/interactive-feedback"
---

## Summary
Different views use inconsistent patterns for showing state changes. Some views mark `dirty_` and rely on the render cycle, while others update immediately. This creates unpredictable visual feedback timing.

### Inconsistencies Found:

1. **Immediate vs. Deferred Updates**:
   - `ListView::navigate()` marks `dirty_ = true` and waits for render cycle
   - `LockScreenView::setBatteryPercent()` marks `dirty_ = true` but doesn't trigger immediate render
   - `T9InputView::processKey()` marks `dirty_ = true` but render may be delayed

2. **Modal Render Behavior**:
   - `showConfirm()` calls `ViewStack::instance().render()` immediately (line 214)
   - `showContextMenu()` does NOT call render immediately (line 263)
   - `showToast()` calls `ViewStack::instance().render()` immediately (line 176)
   - `showMessage()` does NOT call render immediately

3. **Tick Handler Consistency**:
   - `PinEntryView` has `onTick()` for lockout countdown
   - `T9InputView` has `onTick()` for multi-tap timeout
   - `MessageBox` has `onTick()` for auto-dismiss
   - `SliderView`, `ListView`, `DateInputView` do NOT have `onTick()` (no periodic updates)

## Impact
**Predictability**: Users experience inconsistent feedback timing. Some actions feel instant, others feel delayed.

**Maintenance**: Developers copying patterns may use the wrong approach for their use case.

**Performance**: Some views may render more often than needed, others may render too infrequently.

## Evidence
1. **Modal render inconsistency** (`components/cdc_views/src/ConfirmView.cpp:207-214`):
   ```cpp
   void showConfirm(const char* message, ...) {
       s_sharedConfirm.init(message, icon);
       s_sharedConfirm.setOnConfirm(onConfirm, userData);
       s_sharedConfirm.setOnCancel(onCancel, userData);
       ViewStack::instance().showModal(&s_sharedConfirm);
       ViewStack::instance().render();  // <-- Immediate render
   }
   ```

2. **Context menu no immediate render** (`components/cdc_views/src/ContextMenuView.cpp:255-260`):
   ```cpp
   ContextMenuView* showContextMenu(const char* title, const ContextMenuItem* items, uint8_t count) {
       s_sharedContextMenu.init(title, items, count);
       ViewStack::instance().showModal(&s_sharedContextMenu);
       // <-- No render() call here!
       return &s_sharedContextMenu;
   }
   ```

3. **Toast immediate render** (`components/cdc_views/src/ToastView.cpp:172-176`):
   ```cpp
   static void showToastInternal(...) {
       s_sharedToast.init(message, icon, durationMs, dismissible);
       ViewStack::instance().showModal(&s_sharedToast);
       ViewStack::instance().render();  // <-- Immediate render
   }
   ```

## Recommended Fix
1. **Standardize modal render behavior**:
   - All modal show functions should call `render()` immediately
   - Or use a consistent pattern where `showModal()` triggers render

2. **Document the pattern**:
   - Add comments in `ViewStack::showModal()` explaining when to call `render()`
   - Create a helper like `showModalAndRender()` for common use

3. **Review tick handlers**:
   - Identify which views need periodic updates
   - Add `onTick()` where needed (e.g., `SliderView` could show value change animation)

### Implementation Steps (1-hour scope):
1. Add `render()` call to `showContextMenu()` (5 min)
2. Add `render()` call to `showMessage()` if missing (5 min)
3. Document pattern in `ViewStack::showModal()` (15 min)
4. Create `showModalAndRender()` helper (20 min)
5. Update 2-3 existing calls to use new helper (15 min)

## References
- ViewStack modal handling: `components/cdc_ui/include/cdc_ui/ViewStack.h:106-115`
- ConfirmView helper: `components/cdc_views/src/ConfirmView.cpp:207-214`
- ContextMenuView helper: `components/cdc_views/src/ContextMenuView.cpp:255-260`
- ToastView helper: `components/cdc_views/src/ToastView.cpp:172-176`
