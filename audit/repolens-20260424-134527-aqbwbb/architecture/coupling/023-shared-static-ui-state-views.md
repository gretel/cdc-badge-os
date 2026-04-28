---
title: "[HIGH] Shared static view instances in cdc_views create global state"
severity: HIGH
domain: architecture/coupling
lens: shared-view-instances
labels:
  - "audit:architecture/coupling"
---

## Summary
The `cdc_views` component uses shared static view instances (`s_sharedListView` in `ListView.cpp`, `s_sharedSlider` in `SliderView.cpp`) that are reused across the entire application. This creates hidden global state: multiple views can share the same underlying instance, causing race conditions and unexpected behavior.

**Evidence:**
- `components/cdc_views/src/ListView.cpp` (line 278): `static ListView s_sharedListView;`
- `components/cdc_views/src/SliderView.cpp` (line 225): `static SliderView s_sharedSlider;`
- These shared instances are used by multiple modules simultaneously

## Impact
**Race conditions:** If two modules try to use `ListView` at the same time, they share the same instance. State from one module leaks into another.

**Hard to debug:** The same view instance is used in different contexts. A bug in one module can manifest as a bug in another.

**No isolation:** Modules can't be tested independently because they share global view instances.

**Example from codebase:**
```cpp
// ListView.cpp (line 278)
static ListView s_sharedListView;

// Used by multiple modules:
// - mod_totp: showList()
// - mod_password: showList()
// - mod_gpg: showMenu()
// - cdc_os_ui: showMainMenu()
// All share the SAME instance!
```

## Evidence
**File: `components/cdc_views/src/ListView.cpp` (lines 276-285)**
```cpp
// Shared ListView instance for quick usage
static ListView s_sharedListView;

/**
 * \brief Get shared ListView instance.
 * \return Pointer to shared instance.
 */
ListView* ListView::getShared() {
    return &s_sharedListView;
}
```

**File: `components/cdc_views/src/SliderView.cpp` (lines 223-232)**
```cpp
// Shared SliderView instance for quick usage
static SliderView s_sharedSlider;

/**
 * \brief Get shared SliderView instance.
 * \return Pointer to shared instance.
 */
SliderView* SliderView::getShared() {
    return &s_sharedSlider;
}
```

**File: `components/cdc_os_ui/src/AppUi.cpp` (lines 85-87)**
```cpp
static SliderView* s_brightnessSlider = nullptr;
static SliderView* s_sleepSlider = nullptr;
static SliderView* s_timezoneSlider = nullptr;
```
All three sliders might use the same shared instance!

## Recommended Fix
**Option 1: Remove shared instances**
Each view should be created independently:
```cpp
// Instead of:
auto* list = ListView::getShared();

// Use:
auto* list = new ListView();
```

**Option 2: View pooling**
Use a view pool that manages view instances:
```cpp
class ViewPool {
public:
    ListView* acquireListView();
    void releaseListView(ListView* view);
};
```

**Option 3: Clear ownership**
Each module owns its views as member variables. No sharing.

## References
- `components/cdc_views/src/ListView.cpp` - Shared ListView
- `components/cdc_views/src/SliderView.cpp` - Shared SliderView
- `components/cdc_os_ui/src/AppUi.cpp` - Multiple sliders
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - View management
