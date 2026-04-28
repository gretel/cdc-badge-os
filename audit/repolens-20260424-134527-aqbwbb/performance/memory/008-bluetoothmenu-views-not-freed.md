---
title: "[LOW] BluetoothMenuUi views allocated with new but never freed"
severity: LOW
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
---

## Summary

In `components/cdc_os_ui/src/BluetoothMenuUi.cpp`, two UI view objects are allocated with `new` but never freed for the application lifetime.

**Location**: `components/cdc_os_ui/src/BluetoothMenuUi.cpp:156,299` (allocations), no cleanup

```cpp
// Line 156: Bluetooth menu
s_bluetoothMenu = new ListView();

// Line 299: BLE scan view
s_bleScanView = new ListView();

// No destructor or cleanup method exists!
```

## Impact

- **Memory leak**: 2 ListView objects (~64-128 bytes total) persist for application lifetime.
- **Minor impact**: Small memory footprint, one-time allocation.
- **Consistency issue**: Follows same pattern as other modules that don't clean up.

### Context

BluetoothMenuUi provides Bluetooth pairing and scanning functionality. Views are created lazily on first use and persist until application shutdown.

## Evidence

**File**: `components/cdc_os_ui/src/BluetoothMenuUi.cpp`

**Line 153-160** (Bluetooth menu):
```cpp
if (!s_bluetoothMenu) {
    s_bluetoothMenu = new ListView();  // Allocated once, never freed
    s_bluetoothMenu->setOnSelect(onBluetoothSelect);
    // ...
}
```

**Line 296-303** (BLE scan view):
```cpp
if (!s_bleScanView) {
    s_bleScanView = new ListView();  // Allocated once, never freed
    s_bleScanView->setOnSelect(onBleScanSelect);
    // ...
}
```

**No cleanup method**: No destructor or `cleanup()` method to free views.

## Recommended Fix

### Option 1: Add static instances (recommended)

Change to static instances like other modules:

```cpp
// Change declarations
static ListView s_bluetoothMenu;
static ListView s_bleScanView;

// Change allocations (remove "new")
if (s_bluetoothMenu) {  // Already exists
    // Update if needed
} else {
    s_bluetoothMenu.setOnSelect(onBluetoothSelect);
    // ...
}
```

### Option 2: Add cleanup method

```cpp
// In BluetoothMenuUi.h
void cleanup();

// In BluetoothMenuUi.cpp
void BluetoothMenuUi::cleanup() {
    delete s_bluetoothMenu;
    s_bluetoothMenu = nullptr;
    delete s_bleScanView;
    s_bleScanView = nullptr;
}
```

## References

- Related to: Issues #001 (TOTP), #004 (Password), #005 (NVS Edit), #006 (AppUi), #007 (ExpertMenu) memory leak patterns
- Consistency: Follow same pattern as other modules

</content>