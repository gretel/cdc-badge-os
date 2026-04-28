---
title: "[LOW] ExpertMenuUi views allocated with new but never freed"
severity: LOW
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/cdc_os_ui/src/ExpertMenuUi.cpp`, two UI view objects are allocated with `new` but never freed for the application lifetime.

**Location**: `components/cdc_os_ui/src/ExpertMenuUi.cpp:158,212` (allocations), no cleanup

```cpp
// Line 158: Modules view
s_modulesView = new ListView();

// Line 212: Expert menu
s_expertMenu = new ListView();

// No destructor or cleanup method exists!
```

## Impact

- **Memory leak**: 2 ListView objects (~64-128 bytes total) persist for application lifetime.
- **Minor impact**: Small memory footprint, one-time allocation.
- **Consistency issue**: Follows same pattern as AppUi and other modules that don't clean up.

### Context

ExpertMenuUi provides access to advanced settings and module management. Views are created lazily on first use and persist until application shutdown.

## Evidence

**File**: `components/cdc_os_ui/src/ExpertMenuUi.cpp`

**Line 155-162** (Modules view):
```cpp
if (!s_modulesView) {
    s_modulesView = new ListView();  // Allocated once, never freed
    s_modulesView->setOnSelect(onModuleSelect);
    s_modulesView->setOnMenu(onModuleMenu);
}
```

**Line 209-216** (Expert menu):
```cpp
if (!s_expertMenu) {
    s_expertMenu = new ListView();  // Allocated once, never freed
    s_expertMenu->setOnSelect(onExpertSelect);
    // ...
}
```

**No cleanup method**: No destructor or `cleanup()` method to free views.

## Recommended Fix

### Option 1: Add static instances (recommended)

Change to static instances like other modules:

```cpp
// Change declarations
static ListView s_modulesView;
static ListView s_expertMenu;

// Change allocations (remove "new")
if (s_modulesView) {  // Already exists
    // Update if needed
} else {
    s_modulesView.setOnSelect(onModuleSelect);
    s_modulesView.setOnMenu(onModuleMenu);
}
```

### Option 2: Add cleanup method

```cpp
// In ExpertMenuUi.h
void cleanup();

// In ExpertMenuUi.cpp
void ExpertMenuUi::cleanup() {
    delete s_modulesView;
    s_modulesView = nullptr;
    delete s_expertMenu;
    s_expertMenu = nullptr;
}
```

## References

- Related to: Issues #001 (TOTP), #004 (Password), #005 (NVS Edit), #006 (AppUi) memory leak patterns
- Consistency: Follow same pattern as other modules

</content>