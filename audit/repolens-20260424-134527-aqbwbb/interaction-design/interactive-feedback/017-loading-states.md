---
title: "[MEDIUM] Missing loading/processing state indicators for async operations - General pattern"
severity: MEDIUM
domain: interaction-design
lens: interactive-feedback
labels:
  - "audit:interaction-design/interactive-feedback"
---

## Summary
When operations take time to complete (e.g., saving to NVS, verifying PIN, FIDO2 operations), there is no visual indicator that work is in progress. Users may think the system is frozen and press keys repeatedly.

**Note**: See also `007-task-loading-indicator.md` for ToastView/MesageBox-specific TASK icon animation details.

### Missing Loading States:
1. **PIN verification** (`PinEntryView`): No indicator while verifying
2. **Saving values** (e.g., `SliderView` save): No indicator during NVS write
3. **FIDO2 operations** (credential creation, assertion): Can take 500-2000ms
4. **GPG operations** (key generation, signing): Can take 1-5 seconds
5. **Bluetooth/WiFi operations**: Scanning, connecting can take several seconds

## Impact
**User Confidence**: Without a loading indicator, users may think the system froze and repeatedly press keys.

**Duplicate Actions**: Users may trigger the same action multiple times, causing:
- Multiple save operations
- Duplicate credential creation
- Confusing state changes

**Perceived Performance**: Operations feel slower without feedback. A 1-second operation with a spinner feels like 0.5 seconds.

## Evidence
1. **PinEntryView** (`components/cdc_views/src/PinEntryView.cpp:132-167`):
   ```cpp
   void PinEntryView::verify() {
       if (length_ < minLength_) {
           // Immediate feedback for short PIN
           showMessage(tr(StringId::PIN_TOO_SHORT), ...);
           return;
       }

       if (!onVerify_) {
           // Immediate success
           if (onSuccess_) onSuccess_();
           return;
       }

       bool valid = onVerify_(buffer_);  // <-- Can take time, no feedback!
       if (valid) {
           if (onSuccess_) onSuccess_();
       } else {
           // Only shows error AFTER completion
           showMessage(tr(StringId::WRONG_PIN), ...);
       }
   }
   ```
   The `onVerify_()` callback can take time (especially with TROPIC01 secure element), but no loading indicator is shown.

2. **SliderView** (`components/cdc_views/src/SliderView.cpp:111-117`):
   ```cpp
   case 'Y': // Save
       if (onSave_) {
           onSave_(value_);  // NVS write can take 50-100ms
       }
       return InputResult::REQUEST_POP;
   ```
   No visual feedback during save.

3. **FIDO2 module** (`components/mod_fido2/src/Fido2Ui.cpp`):
   - Credential creation can take 1-2 seconds
   - No loading indicator during this time
   - User sees static screen

4. **ToastView** (`components/cdc_views/src/ToastView.cpp`) has a `TASK` icon (line 130-138) that could be used for loading, but it's not systematically used.

## Recommended Fix
1. **Add loading state to views that need it**:
   - `PinEntryView`: Show "Verifying..." toast before `onVerify_()`
   - `SliderView`: Show "Saving..." toast during `onSave_()`
   - FIDO2 module: Show loading toast during long operations

2. **Create a reusable loading indicator**:
   - Add `showLoading(const char* message)` helper
   - Use `ToastView` with `Icon::TASK` for consistent look
   - Auto-dismiss when operation completes

3. **Disable input during loading**:
   - Block key presses while operation is in progress
   - Prevent duplicate actions

### Implementation Steps (1-hour scope):
1. Create `showLoading()` and `hideLoading()` helpers (15 min)
2. Integrate into `PinEntryView::verify()` (20 min)
3. Integrate into FIDO2 module operations (15 min)
4. Test with slow operations (10 min)

**Note**: ToastView TASK icon animation is covered in `007-task-loading-indicator.md`.

## References
- ToastView with TASK icon: `components/cdc_views/src/ToastView.cpp:130-138`
- PinEntryView verify: `components/cdc_views/src/PinEntryView.cpp:132-167`
- FIDO2 UI: `components/mod_fido2/src/Fido2Ui.cpp`
