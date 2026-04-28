---
title: "[LOW] NVS Editor confirmation uses fear-based copy to discourage exploration"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary

The NVS Editor confirmation dialog uses fear-based language ("privileged", "irreversible") that may discourage users from exploring a feature they legitimately need.

**Evidence:**

File: `components/mod_nvsedit/src/NvsEditModule.cpp`, lines 524-528

```cpp
static IView* getNvsEditorView() {
    const char* msg = deleteEnabled()
        ? "NVS Editor is privileged. Deletes are irreversible. Continue?"
        : "NVS Browser is read-only. Continue?";
    showConfirm(msg, onNvsEditorConfirm, nullptr, ConfirmView::Icon::WARNING, nullptr);
    return nullptr;  // We pushed view ourselves
}
```

**Fear-based elements:**

1. **"Privileged"**: Implies special access is needed, may intimidate casual users
2. **"Irreversible"**: Strong negative framing that emphasizes risk over utility
3. **WARNING icon**: Reinforces danger rather than neutral information
4. **Asymmetric framing**: The read-only version doesn't use similar caution language

## Impact

1. **Feature discoverability**: Users may avoid the NVS Editor even when they need it for debugging
2. **Cognitive load**: Users must process fear-based language before understanding the actual risk
3. **Inconsistent tone**: The read-only "NVS Browser" doesn't get similar cautionary language despite also being a technical feature
4. **May create false urgency**: "Irreversible" is dramatic for what is essentially a key-value store edit

## Recommended Fix

Use neutral, informative language that states facts without emotional framing:

```cpp
static IView* getNvsEditorView() {
    const char* msg = deleteEnabled()
        ? "NVS Editor allows deleting keys and namespaces. Continue?"
        : "NVS Browser shows key-value data. Continue?";
    showConfirm(msg, onNvsEditorConfirm, nullptr, ConfirmView::Icon::QUESTION, nullptr);
    return nullptr;
}
```

**Key changes:**
1. Replace "privileged" with descriptive "allows deleting"
2. Replace "irreversible" with the actual action "deleting keys and namespaces"
3. Change icon from `WARNING` to `QUESTION` for neutral tone
4. Use parallel structure for both modes

Alternatively, provide more context:

```cpp
const char* msg = deleteEnabled()
    ? "NVS Editor: View and edit device storage. Deletes remove keys permanently. Continue?"
    : "NVS Browser: View device storage (read-only). Continue?";
```

## References

- Dark Patterns Quick Guide: [Confirmshaming](https://www.darkpatterns.org/types-of-dark-pattern#confirmshaming) - using emotional language to discourage choices
- Nielsen Norman Group: [Fear vs. Clarity](https://www.nngroup.com/articles/affirmative-actions/) - informative vs. intimidating copy
- Material Design: [Dialogs](https://material.io/components/dialogs) - clear, actionable information

</content>