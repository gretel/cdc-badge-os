---
title: "[MEDIUM] Wizard Navigation Can Leave Stale Stack State"
severity: MEDIUM
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
Multiple modules (TOTP, GPG, Password) use a "pop until target view" pattern that can leave stale navigation state if the target view is not found or if the stack is in an unexpected state.

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp:875-878, 907-910`
- `components/mod_gpg/src/GpgModule.cpp:494-496`
- `components/mod_password/src/PasswordModule.cpp:531-534, 687-690`

## Impact
If navigation state gets out of sync (e.g., due to errors, timeouts, or edge cases), the `while` loop may:
1. Pop the root view (lock screen) if target not found
2. Leave the UI in a state where the expected view isn't visible
3. Not provide feedback about what went wrong

## Evidence
From `TotpModule.cpp:875-878`:
```cpp
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}
```

From `GpgModule.cpp:494-496`:
```cpp
while (ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();
}
```

**Problem:** If `s_listView` is somehow not in the stack (e.g., memory issue, reinitialization), the loop will pop until `depth == 1`, potentially leaving the wrong view on top.

## Recommended Fix
Add safety checks and logging:

1. **Verify target exists before popping:**
```cpp
bool found = false;
uint8_t targetDepth = 1;
for (uint8_t i = 0; i < depth(); i++) {
    if (at(i) == &s_listView) {
        found = true;
        targetDepth = i + 1;
        break;
    }
}
if (!found) {
    // Log error, maybe show toast
    LOG_W(TAG, "Target view not in stack, using popToRoot");
    targetDepth = 1;
}
while (depth() > targetDepth) {
    pop();
}
```

2. **Add timeout to prevent infinite loops** (though unlikely with depth check)

3. **Provide user feedback** if navigation fails:
```cpp
if (current() != &s_listView) {
    showToastError("Navigation reset");
}
```

## References
- Similar patterns in GPG and Password modules should be updated together for consistency
- Consider adding a `popToView(IView* target)` helper method to ViewStack
