---
title: "[MEDIUM] Inconsistent null-checking pattern for callback userData"
severity: MEDIUM
domain: cdc_views
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `ListItem.userData` field is used to pass pointer data to callbacks, but null-checking patterns are inconsistent across the codebase. Some callbacks check for null `userData`, while others assume it's always valid. This can lead to null pointer dereferences if `userData` is not properly initialized.

## Evidence
In `components/cdc_views/include/cdc_views/ListView.h`:
```cpp
struct ListItem {
    const char* label;
    uint8_t icon = 0;
    bool iconDisabled = false;
    void* userData = nullptr;  // Default is nullptr
};
```

In `components/cdc_os_ui/src/BluetoothMenuUi.cpp` line 76-78:
```cpp
static bool renderBleScanItem(Gdey029T94* gfx, const ListItem& item,
                              uint16_t index, int x, int y, int w, int h,
                              bool selected, void* userCtx) {
    (void)index;
    (void)h;
    (void)w;
    (void)userCtx;
    if (!gfx || !item.userData) return false;  // ✓ Checks userData

    const auto* dev = reinterpret_cast<const hal::BleScanResult*>(item.userData);
    // ...
}
```

In `components/cdc_os_ui/src/WifiMenuUi.cpp` line 178:
```cpp
const auto* net = reinterpret_cast<const WifiItem*>(item.userData);  // ✗ No null check
```

In `components/cdc_os_ui/src/WifiMenuUi.cpp` line 438:
```cpp
const auto* item = reinterpret_cast<const WifiItem*>(userData);  // ✗ No null check
```

## Impact
1. **Inconsistent behavior**: Some callbacks handle null `userData` gracefully, others may crash.
2. **Maintenance burden**: Future developers may not know which callbacks expect null vs. non-null.
3. **Potential crashes**: If a `ListItem` is created without initializing `userData`, callbacks that don't check for null will dereference nullptr.

## Recommended Fix
Establish a consistent pattern for null-checking `userData` in callbacks:

1. **Document the convention**: Add documentation to `ListItem` and callback typedefs indicating whether `userData` can be null.

2. **Add null checks**: Update callbacks that currently don't check for null:

```cpp
// In WifiMenuUi.cpp
static void onWifiSelect(uint16_t index, void* userData) {
    if (!userData) return;  // Add null check
    const auto* item = reinterpret_cast<const WifiItem*>(userData);
    // ...
}
```

3. **Initialize all ListItems**: Ensure all `ListItem` structures explicitly initialize `userData`:
```cpp
s_listItems[i] = {label, icon, false, nullptr};  // Always init all fields
```

## References
- C++ Core Guidelines F.22: "Use 'noexcept' only for functions that won't throw"
- C++ Core Guidelines F.41: "Use 'noexcept' for functions that won't call virtual functions"

</content>