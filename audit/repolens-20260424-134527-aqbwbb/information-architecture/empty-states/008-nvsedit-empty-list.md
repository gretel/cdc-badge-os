---
title: "[LOW] NVS Edit namespace list shows blank area without guidance when empty"
severity: LOW
domain: mod_nvsedit
lens: empty-states
labels:
  - "empty-state"
  - "nvsedit"
  - "call-to-action"
---

## Summary
The NVS Edit module's namespace list view in `components/mod_nvsedit/src/NvsEditModule.cpp` shows a completely blank list area when `s_namespaceCount` is 0. No empty state message or guidance is provided to explain what namespaces are or how they might appear.

**Location:** `components/mod_nvsedit/src/NvsEditModule.cpp:405-420` (showNamespaceListView function)

## Impact
Users seeing an empty namespace list may:
- Think the NVS editor failed to load namespaces
- Not understand what namespaces are or where they come from
- Be confused about whether they need to create namespaces manually
- Exit the view thinking the feature is broken

The NVS editor is an expert-mode tool, so users accessing it likely expect to see some namespaces. An empty state could indicate either a fresh install or that all namespaces were deleted.

## Evidence
In `showNamespaceListView()` (lines 405-420):
```cpp
static void showNamespaceListView() {
    loadNamespaces();

    if (!s_namespaceListView) {
        s_namespaceListView = new ListView();
        s_namespaceListView->setOnSelect(onNamespaceSelect);
        s_namespaceListView->setOnMenu(onNamespaceMenu);
    }

    for (uint8_t i = 0; i < s_namespaceCount; i++) {
        s_namespaceItems[i] = {s_namespaces[i], 0, false, nullptr};
    }

    s_namespaceListView->init("NVS Namespaces", s_namespaceItems, s_namespaceCount);  // <-- count can be 0
    ViewStack::instance().push(s_namespaceListView);
}
```

The `loadNamespaces()` function (lines 94-119) iterates through NVS entries but may find none on a fresh install or after all namespaces were cleared.

When `s_namespaceCount` is 0, the ListView receives an empty array and count of 0, resulting in a blank list area (handled by the generic ListView empty state).

Similarly, `showKeyListView()` (lines 473-484) has the same issue when a namespace has no keys:

```cpp
static void showKeyListView(const char* ns) {
    loadKeys(ns);
    // ...
    rebuildKeyListView();
    ViewStack::instance().push(s_keyListView);
}
```

## Recommended Fix
Add explicit empty state handling for both namespace and key lists:

**Option 1: Add "No namespaces" placeholder item**
```cpp
static void showNamespaceListView() {
    loadNamespaces();

    if (!s_namespaceListView) {
        s_namespaceListView = new ListView();
        s_namespaceListView->setOnSelect(onNamespaceSelect);
        s_namespaceListView->setOnMenu(onNamespaceMenu);
    }

    if (s_namespaceCount == 0) {
        // Empty state with helpful message
        s_namespaceItems[0] = {"No namespaces", 0, true, nullptr};
        s_namespaceListView->init("NVS Namespaces", s_namespaceItems, 1);
    } else {
        for (uint8_t i = 0; i < s_namespaceCount; i++) {
            s_namespaceItems[i] = {s_namespaces[i], 0, false, nullptr};
        }
        s_namespaceListView->init("NVS Namespaces", s_namespaceItems, s_namespaceCount);
    }
    ViewStack::instance().push(s_namespaceListView);
}
```

**Option 2: Add "No keys" placeholder for empty namespaces**
```cpp
static void rebuildKeyListView() {
    if (s_keyCount == 0) {
        // Empty state
        static char emptyLabel[32];
        snprintf(emptyLabel, sizeof(emptyLabel), "No keys in %s", s_selectedNamespace);
        s_keyItems[0] = {emptyLabel, 0, true, nullptr};
        if (s_keyListView) {
            s_keyListView->init(s_selectedNamespace, s_keyItems, 1);
        }
        return;
    }
    
    for (uint8_t i = 0; i < s_keyCount; i++) {
        snprintf(s_keyLabels[i], sizeof(s_keyLabels[i]), "%s [%s]",
                 s_keys[i], nvsTypeToString(s_keyTypes[i]));
        s_keyItems[i] = {s_keyLabels[i], 0, false, nullptr};
    }
    if (s_keyListView) {
        s_keyListView->init(s_selectedNamespace, s_keyItems, s_keyCount);
    }
}
```

For better user experience, consider adding a helper item that explains what namespaces are:
- "No namespaces (NVS storage is empty)"
- "No keys in this namespace"

## References
- NVS Edit module: `components/mod_nvsedit/src/NvsEditModule.cpp`
- Similar empty state in FIDO2: `components/mod_fido2/src/Fido2Ui.cpp:125-135`
- ListView behavior: `components/cdc_views/src/ListView.cpp`

</content>