---
title: "[LOW] NVS edit module ListViews allocated with new but never freed"
severity: LOW
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/mod_nvsedit/src/NvsEditModule.cpp`, three UI views are allocated with `new` but never freed:

**Location**: `components/mod_nvsedit/src/NvsEditModule.cpp:49-51` (declarations), `components/mod_nvsedit/src/NvsEditModule.cpp:409,477,501` (allocations)

```cpp
// Line 49-51: Declarations
static ListView* s_namespaceListView = nullptr;
static ListView* s_keyListView = nullptr;
static InfoView* s_valueView = nullptr;

// Line 409: First allocation
s_namespaceListView = new ListView();

// Line 477: Second allocation  
s_keyListView = new ListView();

// Line 501: Third allocation
s_valueView = new InfoView();

// Line 557-559: stop() doesn't free them
void NvsEditModule::stop() {
    state_ = ServiceState::STOPPED;  // No delete!
}
```

## Impact

- **Memory leak**: Three UI view objects are allocated once when first accessed and never freed for the lifetime of the application.
- **Minor impact**: Since this is a one-time allocation (not repeated like TOTP/Password), the total leak is small (~100-200 bytes for 3 views).
- **Consistency issue**: Other modules (TOTP, Password) free their buffers in `stop()`, but NVS edit doesn't.

### Context

The views are created lazily on first use:
- `s_namespaceListView` created in `showNamespaceListView()` (line 409)
- `s_keyListView` created in `showKeyListView()` (line 477)  
- `s_valueView` created in `showValueView()` (line 501)

Once created, they persist until module stop, but `stop()` doesn't clean them up.

## Evidence

**File**: `components/mod_nvsedit/src/NvsEditModule.cpp`

**Line 49-51**:
```cpp
static ListView* s_namespaceListView = nullptr;
static ListView* s_keyListView = nullptr;
static InfoView* s_valueView = nullptr;
```

**Line 409-412**:
```cpp
if (!s_namespaceListView) {
    s_namespaceListView = new ListView();  // Allocated
    s_namespaceListView->setOnSelect(onNamespaceSelect);
    s_namespaceListView->setOnMenu(onNamespaceMenu);
}
```

**Line 477-480**:
```cpp
if (!s_keyListView) {
    s_keyListView = new ListView();  // Allocated
    s_keyListView->setOnSelect(onKeySelect);
    s_keyListView->setOnMenu(onKeyMenu);
}
```

**Line 501-504**:
```cpp
if (!s_valueView) {
    s_valueView = new InfoView();  // Allocated
    s_valueView->init("Value Details", "");
}
```

**Line 557-559**:
```cpp
void NvsEditModule::stop() {
    state_ = ServiceState::STOPPED;  // Missing: delete s_namespaceListView; etc.
}
```

## Recommended Fix

Add cleanup in `stop()`:

```cpp
void NvsEditModule::stop() {
    delete s_namespaceListView;
    s_namespaceListView = nullptr;
    delete s_keyListView;
    s_keyListView = nullptr;
    delete s_valueView;
    s_valueView = nullptr;
    state_ = ServiceState::STOPPED;
}
```

### Alternative: Use static instances (more efficient)

Since these views are created once and reused, use static instances like other modules:

```cpp
// Line 49-51: Change to static
static ListView s_namespaceListView;
static ListView s_keyListView;
static InfoView s_valueView;

// Line 409: Remove new
if (s_namespaceListView) {  // Already exists
    // Just update callbacks if needed
} else {
    s_namespaceListView.setOnSelect(onNamespaceSelect);
    s_namespaceListView.setOnMenu(onNamespaceMenu);
}

// Line 557: No cleanup needed
void NvsEditModule::stop() {
    state_ = ServiceState::STOPPED;
}
```

This approach:
- Eliminates dynamic allocation entirely
- Consistent with TOTP/Password module patterns
- No cleanup needed in `stop()`

## References

- Related to: Issues #001 (TOTP), #004 (Password) memory leak patterns
- Consistency: Follow same pattern as other modules

</content>