---
title: "[LOW] Inconsistent toast duration patterns"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "toast-messages"
  - "ux-consistency"
---

## Summary
Toast messages use inconsistent duration patterns without a clear convention for when to use different durations:

1. **Multiple duration constants used:**
   - `TOAST_DURATION_SHORT_MS` (ExpertMenuUi.cpp line 47)
   - `TOAST_DURATION_MEDIUM_MS` (ExpertMenuUi.cpp lines 50, 98; WifiMenuUi.cpp lines 576, 591, 606)
   - `TOAST_DURATION_LONG_MS` (WifiMenuUi.cpp lines 314, 317, 711)
   - No duration specified (uses default)

2. **Inconsistent duration choices for similar messages:**
   - FIDO2 PIN success: 2000ms (Fido2Ui.cpp line 306)
   - FIDO2 PIN error: 2000ms for "Too many attempts", 1000ms for "Wrong PIN" (Fido2Ui.cpp lines 361, 364)
   - Expert menu success: TOAST_DURATION_SHORT_MS (line 47)
   - Expert menu error: TOAST_DURATION_MEDIUM_MS (line 50)
   - WiFi errors: TOAST_DURATION_MEDIUM_MS for "Invalid IP" (lines 576, 591, 606)
   - WiFi operations: TOAST_DURATION_LONG_MS for connect/sync (lines 314, 317, 711)

3. **Sticky toasts vs timed toasts:**
   - `showToastAlertSticky()` used for critical errors (ExpertMenuUi.cpp line 110, AppUi.cpp line 552)
   - No clear pattern for when to use sticky vs timed

4. **Zero duration for "task" messages:**
   - `showToastTask(tr(StringId::TASK_WORKING), 0)` - 0ms means persistent (ExpertMenuUi.cpp lines 177, 191)

## Impact
- **User experience inconsistency**: Similar messages disappear at different rates
- **Confusion**: Users may miss important messages if duration is too short
- **Clutter**: Too many persistent toasts can clutter the interface
- **Maintainability**: No clear convention for what duration to use

## Evidence
**File: `components/mod_fido2/src/Fido2Ui.cpp` lines 306, 361, 364**
```cpp
ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
ui::showToastError(ui::tr(ui::StringId::TOO_MANY_ATTEMPTS), 2000);
ui::showToastError(ui::tr(StringId::WRONG_PIN), 1000);
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` lines 47, 50, 110, 177**
```cpp
showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
showToastError(error ? error : tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
showToastAlertSticky(tr(StringId::USB_REPLUG_REQUIRED));
showToastTask(tr(StringId::TASK_WORKING), 0);
```

**File: `components/cdc_os_ui/src/WifiMenuUi.cpp` lines 576, 314, 711**
```cpp
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
showToastSuccess(msg, TOAST_DURATION_LONG_MS);
showToastError(tr(StringId::NTP_TIMEOUT), TOAST_DURATION_LONG_MS);
```

**File: `components/cdc_os_ui/src/AppUi.cpp` line 552**
```cpp
showToastAlertSticky(msg ? msg : "Slot map invalid");
```

## Recommended Fix
1. **Establish clear duration convention:**
   - **Short (1000ms)**: Simple confirmations, quick status updates
   - **Medium (2000ms)**: Error messages, warnings (time to read)
   - **Long (3000ms)**: Complex messages, multi-word status
   - **Sticky**: Critical errors requiring user acknowledgment

2. **Standardize existing calls:**
   - FIDO2 success: Use TOAST_DURATION_SHORT_MS (consistent with other successes)
   - FIDO2 errors: Use TOAST_DURATION_MEDIUM_MS (consistent)
   - WiFi "Invalid IP": Use TOAST_DURATION_MEDIUM_MS (already correct)
   - WiFi operations: Use TOAST_DURATION_LONG_MS (already correct)

3. **Define when to use sticky toasts:**
   - Critical errors that block operation
   - Configuration issues requiring user action
   - Hardware unavailable states

4. **Document the pattern:**
   ```cpp
   // Duration guidelines:
   // - Success/toast: SHORT (1000ms)
   // - Errors/warnings: MEDIUM (2000ms)
   // - Complex messages: LONG (3000ms)
   // - Critical/blocking: STICKY (showToastAlertSticky)
   // - Ongoing tasks: 0ms (persistent until dismissed)
   ```

5. **Consider removing explicit durations where default suffices:**
   - Use default duration for standard cases
   - Only specify non-standard durations when needed

## References
- UI Copy Consistency: Toast message patterns
- UX best practices: Toast duration guidelines
- Microcopy: Feedback timing
