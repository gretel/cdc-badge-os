---
title: "[LOW] Expert menu shows warning toast before each access without context"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - modal-dialogs
  - warning-fatigue
---

## Summary

The `showExpertMenu()` function (`ExpertMenuUi.cpp:205-220`) displays a warning toast ("EXPERT_WARNING") every time the expert menu is opened. This creates an extra step for users who access the menu regularly, leading to warning fatigue.

**Files:**
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:205-220` (expert menu entry)
- `components/cdc_os_ui/include/cdc_os_ui/AppUiInternal.h:54-56` (function declaration)

## Impact

**User Experience:** Regular users who frequently access the expert menu must dismiss the same warning repeatedly. The warning is shown as a toast that auto-dismisses, but still adds friction to the workflow.

**Evidence:**
From `ExpertMenuUi.cpp:210-218`:
```cpp
void showExpertMenu() {
    showToastInfo(tr(StringId::EXPERT_WARNING), TOAST_DURATION_MEDIUM_MS);
    if (!s_expertMenu) {
        s_expertMenu = new ListView();
        s_expertMenu->setOnSelect(onExpertMenuSelect);
    }

    rebuildExpertMenu();
    ViewStack::instance().push(s_expertMenu);
}
```

The toast is shown unconditionally every time `showExpertMenu()` is called.

## Recommended Fix

1. **Show warning only on first access** during a session (track with a flag)
2. **Move warning to the menu itself** as the first item or header
3. **Add a "Don't show again" option** for power users
4. **Consider if the warning is necessary** if the menu items are self-explanatory

Quick fix: Add a static boolean flag to track if the warning has been shown in the current session:
```cpp
static bool s_expertWarningShown = false;
if (!s_expertWarningShown) {
    showToastInfo(tr(StringId::EXPERT_WARNING), TOAST_DURATION_MEDIUM_MS);
    s_expertWarningShown = true;
}
```

## References

- Nielsen Norman Group: "Warning Fatigue: When Users Stop Reading Warnings"
- Material Design: "Toasts" vs. "Dialogs" usage patterns
