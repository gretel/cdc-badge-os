---
title: "[MEDIUM] NVS editor GUI allows destructive deletes with single confirmation"
severity: MEDIUM
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The NVS Editor GUI in `components/mod_nvsedit/src/NvsEditModule.cpp` allows deletion of individual keys and entire namespaces after a single confirmation dialog. Unlike the serial commands which have feature flag protection, the GUI delete actions are only protected by `FEATURE_NVS_EDIT` flag but lack per-action confirmation.

## Impact
- **Data Loss Risk**: After entering the NVS editor (which shows a warning dialog), users can delete keys/namespaces with just one menu selection
- **No Per-Action Confirmation**: The delete actions `onDeleteNamespace()` and `onDeleteKey()` execute immediately after context menu selection
- **Warning Dialog Only Once**: The initial warning at entry (line 521-525) doesn't protect individual delete operations

## Evidence
File: `components/mod_nvsedit/src/NvsEditModule.cpp`

Lines 31-32 (context menu definitions):
```cpp
static ContextMenuItem s_nsContextItems[] = {
    {"Delete NS", onDeleteNamespace}  // No per-action confirmation!
};

static ContextMenuItem s_keyContextItems[] = {
    {"Delete Key", onDeleteKey}       // No per-action confirmation!
};
```

Lines 297-322 (onDeleteNamespace - no confirmation):
```cpp
static void onDeleteNamespace() {
    if (!deleteEnabled()) {
        showDeleteDisabled();
        hideContextMenu();
        return;
    }
    if (s_selectedNamespace[0] = '\0') return;

    if (deleteNamespace(s_selectedNamespace)) {  // Deletes immediately!
        showToastInfo("Deleted");
        loadNamespaces();
        // Update list...
    } else {
        showToastError("Delete failed");
    }
    hideContextMenu();
}
```

Lines 325-358 (onDeleteKey - no confirmation):
```cpp
static void onDeleteKey() {
    if (!deleteEnabled()) {
        showDeleteDisabled();
        hideContextMenu();
        return;
    }
    if (s_selectedNamespace[0] = '\0' || s_selectedKey[0] = '\0') return;

    if (deleteKey(s_selectedNamespace, s_selectedKey)) {  // Deletes immediately!
        showToastInfo("Deleted");
        loadKeys(s_selectedNamespace);
        // ...
    }
    hideContextMenu();
}
```

Compare to entry dialog at lines 513-525 which DOES show warning:
```cpp
static IView* getNvsEditorView() {
    const char* msg = deleteEnabled()
        ? "NVS Editor is privileged. Deletes are irreversible. Continue?"
        : "NVS Browser is read-only. Continue?";
    showConfirm(msg, onNvsEditorConfirm, nullptr, ConfirmView::Icon::WARNING, nullptr);
    return nullptr;
}
```

## Recommended Fix
Add per-action confirmation dialogs for delete operations:

1. Modify `onDeleteNamespace()` to show confirmation before deleting:
```cpp
static void onDeleteNamespace() {
    if (!deleteEnabled()) {
        showDeleteDisabled();
        hideContextMenu();
        return;
    }
    if (s_selectedNamespace[0] = '\0') return;

    // Show confirmation dialog
    showConfirm(
        (std::string("Delete namespace '") + s_selectedNamespace + "'?").c_str(),
        [](void* ctx) {
            const char* ns = (const char*)ctx;
            if (deleteNamespace(ns)) {
                showToastInfo("Deleted");
                loadNamespaces();
                // Update list...
            } else {
                showToastError("Delete failed");
            }
        },
        (void*)strdup(s_selectedNamespace),
        ConfirmView::Icon::WARNING,
        hideContextMenu
    );
}
```

2. Similarly modify `onDeleteKey()` with per-key confirmation

3. Consider showing the number of keys in the namespace before deletion

## References
- NVS Editor module: `components/mod_nvsedit/`
- ConfirmView usage in codebase: `components/cdc_views/include/cdc_views/ConfirmView.h`
- Similar pattern: `cmdNvsClear` in `SerialCmd.cpp` requires `YES` confirmation
