---
title: "[LOW] Modal dialogs don't integrate with browser-style back navigation"
severity: LOW
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
Modal overlays (Toast, Confirm, ContextMenu) are shown using `showModal()` but do not integrate with the normal back navigation flow. Users pressing 'N' during a modal may not get expected behavior, and modals can block navigation without clear dismissal path.

**Files affected:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Modal API (lines 109-125)
- `components/cdc_views/src/ContextMenuView.cpp` - Context menu modal
- `components/cdc_views/src/ConfirmView.cpp` - Confirmation dialog
- `components/mod_fido2/src/Fido2Ui.cpp` - User presence modal (lines 560-570)

## Impact
Users may experience confusion when:
1. A modal appears and 'N' doesn't dismiss it as expected
2. Multiple modals stack without clear way to dismiss all
3. Modal blocks back navigation to previous view

This affects flows like:
- FIDO2 user presence prompt (modal that blocks navigation)
- Context menu with multiple options
- Toast notifications that auto-dismiss

## Evidence
Modal stacking is possible but not well-controlled:

```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h (lines 109-115)
void showModal(IView* modal);
void hideModal();
bool hasModal() const { return modal_ != nullptr; }
IView* getModal() const { return modal_; }
```

FIDO2 user presence shows modal and tracks return depth:

```cpp
// components/mod_fido2/src/Fido2Ui.cpp (lines 560-570)
s_promptReturnDepth = stack.depth();
s_promptReturnView = stack.current();

s_promptView->init(title, prompt_text);
stack.push(s_promptView);  // Note: uses push, not showModal
```

Context menu uses showModal:

```cpp
// components/cdc_views/src/ContextMenuView.cpp (line 260)
ViewStack::instance().showModal(&s_sharedContextMenu);
```

No mechanism to dismiss modal with 'N' key. The modal sits on top of the view stack:

```cpp
// components/cdc_ui/src/ViewStack.cpp - dispatchKey handles modal first
void ViewStack::dispatchKey(char key) {
    if (modal_) {
        // Key goes to modal
        auto result = modal_->onKey(key);
        // ...
    }
}
```

But modals don't handle 'N' consistently:

```cpp
// components/cdc_views/src/ConfirmView.cpp - ConfirmView.onKey
// Only handles Y/N for confirm/deny, not 'N' for dismiss
```

## Recommended Fix
Add modal back-navigation support:

1. **Handle 'N' in modal views to dismiss**:
   ```cpp
   // In ContextMenuView.onKey
   InputResult onKey(char key) override {
       if (key == 'N') {
           ViewStack::instance().hideModal();
           return InputResult::CONSUMED;
       }
       // ...
   }
   ```

2. **Add modal stacking limit** to prevent accumulation:
   ```cpp
   static constexpr uint8_t MAX_MODALS = 2;
   void showModal(IView* modal) {
       if (modalCount_ >= MAX_MODALS) {
           hideModal();  // Pop oldest
       }
       // ...
   }
   ```

3. **Document modal dismissal** in footer:
   ```cpp
   // Context menu footer: "[Y] Action  [3] Next  [N] Cancel"
   ```

Scope: ~1 hour to add 'N' dismissal to ContextMenuView and ConfirmView.

## References
- Modal API: `components/cdc_ui/include/cdc_ui/ViewStack.h` lines 109-125
- FIDO2 modal flow: `components/mod_fido2/src/Fido2Ui.cpp` lines 530-570
- Context menu: `components/cdc_views/src/ContextMenuView.cpp`
