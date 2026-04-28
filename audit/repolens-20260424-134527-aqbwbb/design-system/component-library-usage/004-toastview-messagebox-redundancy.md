---
title: "[MEDIUM] ToastView and MessageBox provide overlapping functionality - potential consolidation"
severity: MEDIUM
domain: component-library-usage
lens: ui-components
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The component library has two very similar modal overlay components (`ToastView` and `MessageBox`) that provide essentially the same functionality with minor differences. This creates redundancy and potential confusion for developers choosing which component to use.

**Locations:**
- `components/cdc_views/include/cdc_views/ToastView.h` - ToastView class definition
- `components/cdc_views/include/cdc_views/MessageBox.h` - MessageBox class definition

## Impact
1. **Developer Confusion**: When needing to show a modal message, developers must decide between `ToastView` and `MessageBox` without clear guidance on which to use.

2. **Code Duplication**: Both components implement:
   - Centered modal dialog with dialog frame
   - Icon support (SUCCESS, ERROR, INFO, WARNING/TASK, ALERT)
   - Auto-dismiss timeout
   - Key press dismissal (Y/N)
   - Similar rendering logic

3. **Maintenance Overhead**: Bug fixes or feature additions need to be applied to two similar components instead of one.

4. **Inconsistent Icon Naming**: 
   - `ToastView::Icon` has: NONE, SUCCESS, ERROR, INFO, TASK, ALERT
   - `MessageBox::MessageIcon` has: NONE, SUCCESS, ERROR, INFO, WARNING

## Evidence
**ToastView (components/cdc_views/include/cdc_views/ToastView.h:18-29):**
```cpp
class ToastView : public ViewBase {
public:
    enum class Icon : uint8_t {
        NONE = 0,
        SUCCESS,
        ERROR,
        INFO,
        TASK,
        ALERT
    };
```

**MessageBox (components/cdc_views/include/cdc_views/MessageBox.h:28-40):**
```cpp
class MessageBox : public ViewBase {
public:
    enum class MessageIcon : uint8_t {
        NONE = 0,       // No icon
        SUCCESS,        // Checkmark
        ERROR,          // X mark
        INFO,           // Info circle
        WARNING         // Warning triangle
    };
```

**Both implement similar init signatures:**
- ToastView: `void init(const char* message, Icon icon = Icon::NONE, uint16_t durationMs = 1500, bool dismissible = true);`
- MessageBox: `void init(const char* message, MessageIcon icon = MessageIcon::NONE, uint32_t timeoutMs = 0);`

**Both render centered dialog boxes with icons:**
- ToastView.cpp:95-158 - Icon rendering switch
- MessageBox.cpp:136-181 - Icon rendering switch

## Recommended Fix
**Option 1: Consolidate into single component (recommended)**

1. Keep `MessageBox` as the primary component (more flexible with timeout=0 for manual dismiss)
2. Add deprecation notice to `ToastView`:
```cpp
/**
 * \deprecated Use MessageBox instead. ToastView will be removed in next major version.
 */
class ToastView : public ViewBase {
    // ... existing implementation
};
```

3. Update `ToastView` convenience functions to delegate to `MessageBox`:
```cpp
inline void showToast(const char* message, uint16_t durationMs = 1500) {
    showMessage(message, MessageIcon::NONE, durationMs);
}
inline void showToastSuccess(const char* message, uint16_t durationMs = 1500) {
    showMessage(message, MessageIcon::SUCCESS, durationMs);
}
// ... etc
```

4. Add `TASK` and `ALERT` icons to `MessageIcon` enum if needed.

**Option 2: Clear differentiation**

If both need to exist, document clear use cases:
- `ToastView`: Quick, auto-dismissing notifications (e.g., "Saved!", "Typed")
- `MessageBox`: Important system feedback requiring attention (e.g., errors, warnings)

Add extensive comments to both classes explaining when to use each.

**Recommended: Option 1** - Consolidation reduces complexity and maintenance burden.

## References
- `components/cdc_views/src/ToastView.cpp` - ToastView implementation
- `components/cdc_views/src/MessageBox.cpp` - MessageBox implementation
- `components/cdc_views/include/cdc_views/RenderHelpers.h:16` - Shared `drawDialogFrame` used by both

</content>