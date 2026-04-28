---
title: "[MEDIUM] NVS Editor Delete Actions Execute Without Confirmation"
severity: MEDIUM
domain: destructive-actions
lens: ui-delete-flows
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The NVS Editor module shows a confirmation dialog when entering the editor (`getNvsEditorView()` at line 524-530), but the actual delete operations execute immediately without additional confirmation. The `onDeleteNamespace()` and `onDeleteKey()` functions in `components/mod_nvsedit/src/NvsEditModule.cpp:301-357` call the delete functions directly from context menu actions.

**Evidence:**
- File: `components/mod_nvsedit/src/NvsEditModule.cpp`
- Lines: 301-323 (`onDeleteNamespace`)
- Lines: 328-357 (`onDeleteKey`)
- Lines: 359-365 (Context menu items)

```cpp
static void onDeleteNamespace() {
    if (!deleteEnabled()) {
        showDeleteDisabled();
        hideContextMenu();
        return;
    }
    if (s_selectedNamespace[0] == '\0') return;

    if (deleteNamespace(s_selectedNamespace)) {  // Immediate delete!
        showToastInfo("Deleted");
        loadNamespaces();
        // ... update list view
    } else {
        showToastError("Delete failed");
    }
    hideContextMenu();
}

static void onDeleteKey() {
    if (!deleteEnabled()) {
        showDeleteDisabled();
        hideContextMenu();
        return;
    }
    if (s_selectedNamespace[0] == '\0' || s_selectedKey[0] == '\0') return;

    if (deleteKey(s_selectedNamespace, s_selectedKey)) {  // Immediate delete!
        showToastInfo("Deleted");
        loadKeys(s_selectedNamespace);
        // ... update list view
    } else {
        showToastError("Delete failed");
    }
    hideContextMenu();
}

static ContextMenuItem s_nsContextItems[] = {
    {"Delete NS", onDeleteNamespace}  // No confirmation!
};

static ContextMenuItem s_keyContextItems[] = {
    {"Delete Key", onDeleteKey}  // No confirmation!
};
```

The entry confirmation at line 524-530 only warns about the editor being privileged, but doesn't confirm individual delete actions:

```cpp
static IView* getNvsEditorView() {
    const char* msg = deleteEnabled()
        ? "NVS Editor is privileged. Deletes are irreversible. Continue?"
        : "NVS Browser is read-only. Continue?";
    showConfirm(msg, onNvsEditorConfirm, nullptr, ConfirmView::Icon::WARNING, nullptr);
    return nullptr;
}
```

## Impact
- **Data Loss Risk**: Users can accidentally delete entire NVS namespaces or individual keys with a single context menu selection
- **Namespace Erasure**: Deleting a namespace wipes ALL keys within it (e.g., `gpg`, `password`, `totp` namespaces)
- **Module Breakage**: Deleting module-specific NVS data can break module functionality
- **No Recovery**: NVS data is stored in flash; once erased, it cannot be recovered
- **Insufficient Friction**: The initial entry confirmation is not enough - each destructive action should have its own confirmation

## Recommended Fix
Add confirmation dialogs for both namespace and key deletion:

```cpp
static void onDeleteNamespace() {
    if (!deleteEnabled()) {
        showDeleteDisabled();
        hideContextMenu();
        return;
    }
    if (s_selectedNamespace[0] == '\0') return;

    // Show confirmation with namespace name
    char msg[64];
    snprintf(msg, sizeof(msg), "Delete namespace '%s'? All keys will be erased.", s_selectedNamespace);
    showConfirm(msg, onNvsEditorConfirm, nullptr, ConfirmView::Icon::WARNING, nullptr);
    // Store selected namespace for confirmation callback
    // Then call deleteNamespace() in the callback
}

static void onNvsEditorConfirm(void* userData) {
    // For namespace delete:
    if (s_selectedNamespace[0] != '\0') {
        if (deleteNamespace(s_selectedNamespace)) {
            showToastInfo("Deleted");
            loadNamespaces();
            // ... update list view
        } else {
            showToastError("Delete failed");
        }
    }
    hideContextMenu();
}
```

For key deletion, use a similar pattern with the key name in the confirmation message.

## References
- Entry confirmation: `NvsEditModule.cpp:524-530` shows the existing confirmation pattern
- Similar pattern: `PasswordModule.cpp:699-704` uses `showConfirm()` for delete actions
- Context menu: `ContextMenuView` is the modal overlay for selecting delete actions
