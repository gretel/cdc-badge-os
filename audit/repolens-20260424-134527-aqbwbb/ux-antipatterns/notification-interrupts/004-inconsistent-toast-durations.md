---
title: "[MEDIUM] Inconsistent toast durations across modules"
severity: MEDIUM
domain: notification-interrupts
lens: notification-interrupts
labels:
  - "toast-implementation"
  - "consistency"
---

## Summary
Toast durations are inconsistent across the codebase, with some modules using explicit duration constants and others using default values. This leads to unpredictable user experience.

**Files affected:**
- Multiple module files using showToast functions

**Evidence:**

**Inconsistent usage patterns:**

1. **Default duration (1500ms)** - used inconsistently:
```cpp
// mod_totp/src/TotpModule.cpp:467
ui::showToastSuccess("Typed");  // 1500ms

// mod_gpg/src/GpgModule.cpp:490
ui::showToastSuccess(ui::tr(ui::StringId::OK));  // 1500ms

// cdc_os_ui/src/BluetoothMenuUi.cpp:214
showToastSuccess(tr(StringId::BLUETOOTH_ON));  // 1500ms
```

2. **Explicit short duration (1000ms)**:
```cpp
// ExpertMenuUi.cpp:47
showToastSuccess("OK", TOAST_DURATION_SHORT_MS);  // 1000ms
```

3. **Explicit medium duration (1500ms)**:
```cpp
// ExpertMenuUi.cpp:50
showToastError(error ? error : tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);  // 1500ms
```

4. **Explicit long duration (2500ms)**:
```cpp
// WifiMenuUi.cpp:314
showToastSuccess(msg, TOAST_DURATION_LONG_MS);  // 2500ms
```

5. **No auto-dismiss (0ms)**:
```cpp
// BluetoothMenuUi.cpp:277
showToastInfo(tr(StringId::BLE_SCANNING), 0);  // Until dismissed

// WifiMenuUi.cpp:304
showToastInfo(msg, 0);  // Until dismissed

// ExpertMenuUi.cpp:177
showToastTask(tr(StringId::TASK_WORKING), 0);  // Until dismissed
```

## Impact
- **Confusing UX**: Similar actions produce toasts with different visibility durations
- **Inconsistent timing**: Success messages appear for 1000ms in one place, 1500ms in another
- **Maintenance burden**: No clear pattern for developers to follow

## Evidence
1. `TOAST_DURATION_SHORT_MS` (1000ms) used only in ExpertMenuUi
2. `TOAST_DURATION_MEDIUM_MS` (1500ms) used only in ExpertMenuUi and WifiMenuUi
3. `TOAST_DURATION_LONG_MS` (2500ms) used only in WifiMenuUi
4. Default duration (1500ms) used in most other modules
5. Duration 0 used for "task" and "info" toasts inconsistently

## Recommended Fix
Establish and document consistent duration guidelines:

1. **Create a central toast duration policy** in `ToastView.h`:
```cpp
/**
 * Toast Duration Guidelines:
 * - SHORT (1500ms): Quick confirmations ("OK", "Done")
 * - MEDIUM (2000ms): Standard messages ("Saved", "Updated")
 * - LONG (3000ms): Detailed messages with multiple words
 * - 0ms: Task progress, scanning states (manually dismissed)
 */
```

2. **Update all toast calls to follow the policy**:
```cpp
// Success/confirmation toasts - use SHORT
showToastSuccess("OK", TOAST_DURATION_SHORT_MS);

// Error toasts - use MEDIUM (users need more time to read)
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);

// Info toasts - use MEDIUM
showToastInfo("Bluetooth on", TOAST_DURATION_MEDIUM_MS);

// Task/progress toasts - use 0 (manual dismiss)
showToastTask("Connecting...", 0);
```

3. **Add lint rule or code review checklist** to enforce consistency

## References
- Material Design: Snackbars and toasts
- Human Interface Guidelines: Notifications
