---
title: "[LOW] InfoView uses ambiguous key mappings for Y/N callbacks vs navigation"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary

The `InfoView` component has dual behavior for the Y key depending on whether callbacks are set, which can confuse users about the primary dismiss action.

**Evidence:**

File: `components/cdc_views/src/InfoView.cpp`, lines 108-129

```cpp
InputResult InfoView::onKey(char key) {
    if (key == 'Y' && onYes_) {
        onYes_(callbackUserData_);
        return InputResult::CONSUMED;
    }
    if (key == 'N' && onNo_) {
        onNo_(callbackUserData_);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case '2': // Up
            scroll(false);
            return InputResult::CONSUMED;

        case '8': // Down
            scroll(true);
            return InputResult::CONSUMED;

        case 'N': // Back
        case 'Y': // Also back (info is read-only)
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}
```

**The ambiguity:**

1. **Y key has dual meaning**:
   - If `onYes_` callback is set: triggers the yes action
   - If `onYes_` is null: acts as "Back" (pops the view)

2. **N key has dual meaning**:
   - If `onNo_` callback is set: triggers the no action
   - If `onNo_` is null: acts as "Back" (pops the view)

3. **Footer hint doesn't clarify**: The default hint `HINT_SCROLL_BACK` says `[N] Back` but doesn't mention Y behavior

## Impact

1. **User confusion**: Users may not know whether Y will trigger a callback or just close the view
2. **Inconsistent UX**: The same key behaves differently depending on internal state not visible to the user
3. **Discoverability issue**: The dual behavior is only discoverable through code inspection or trial-and-error
4. **Accessibility concern**: Screen readers or help text cannot explain the behavior without knowing callback state

## Recommended Fix

**Option 1: Separate the concerns** - Use different keys for callbacks vs dismiss:

```cpp
InputResult InfoView::onKey(char key) {
    if (key == 'Y' && onYes_) {
        onYes_(callbackUserData_);
        return InputResult::CONSUMED;
    }
    if (key == 'N' && onNo_) {
        onNo_(callbackUserData_);
        return InputResult::CONSUMED;
    }

    switch (key) {
        case '2': // Up
            scroll(false);
            return InputResult::CONSUMED;

        case '8': // Down
            scroll(true);
            return InputResult::CONSUMED;

        case '3': // Context/Options (for Y/N callbacks)
            if (onYes_) {
                onYes_(callbackUserData_);
            }
            return InputResult::CONSUMED;

        case 'N': // Back (always dismiss)
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}
```

**Option 2: Make the hint dynamic** - Update the footer hint based on callback state:

```cpp
const char* InfoView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    // If callbacks are set, show Y/N hint; otherwise show Back hint
    if (onYes_ || onNo_) {
        return "[Y] Yes  [N] No  [Back]";
    }
    return tr(StringId::HINT_SCROLL_BACK);
}
```

**Option 3: Simplify the API** - Remove the dual behavior entirely:
- Y/N always trigger callbacks (if set) or do nothing (if not set)
- Use a separate key (e.g., '3' or 'Back') for dismiss action

## References

- Nielsen Norman Group: [Consistency in UX](https://www.nngroup.com/articles/consistency-and-familiarity/) - predictable key behavior
- Material Design: [Input patterns](https://material.io/design/usability/basics.html) - clear, single-purpose actions
- WCAG 2.1: [Predictable](https://www.w3.org/WAI/WCAG21/Understanding/predictable.html) - same key should have consistent meaning

</content>