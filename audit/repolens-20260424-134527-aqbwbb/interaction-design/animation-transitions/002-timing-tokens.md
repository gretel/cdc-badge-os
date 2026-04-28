---
title: "[MEDIUM] No Shared Timing Design Tokens for Transitions"
severity: MEDIUM
domain: interaction-design
lens: animation-transitions
labels:
  - "audit:interaction-design/animation-transitions"
---

## Summary
The codebase uses hardcoded timing values scattered across different files without centralized timing tokens. This leads to inconsistent timing for similar interactions.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp:29` - `durationMs = 1500` (default toast)
- `components/cdc_views/src/ToastView.h:57` - `durationMs_ = 1500`
- `components/cdc_views/include/cdc_views/MessageBox.h:88` - `timeoutMs = 2000` (success)
- `components/cdc_views/include/cdc_views/MessageBox.h:93` - `timeoutMs = 2000` (error)
- `components/cdc_os_ui/src/SleepManager.cpp:112` - `vTaskDelay(pdMS_TO_TICKS(350))` (E-Paper refresh wait)
- `components/cdc_os_ui/src/SleepManager.cpp:168` - `vTaskDelay(pdMS_TO_TICKS(350))` (timer wakeup)
- `components/cdc_os_ui/src/AppUi.cpp:51` - `INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000` (300 seconds)

## Impact
- **Maintenance:** Changing timing requires finding all occurrences
- **Consistency:** Toast uses 1500ms, MessageBox uses 2000ms for similar "message" purpose
- **Scalability:** Adding new timing values leads to more duplication
- **E-Paper Timing:** The 350ms wait is hardcoded without explanation - should be a constant

## Evidence
```cpp
// ToastView.h:57
uint16_t durationMs_ = 1500;  // Magic number

// MessageBox.h:88-93
inline void showSuccess(const char* message, uint32_t timeoutMs = 2000) {
    showMessage(message, MessageIcon::SUCCESS, timeoutMs);
}
inline void showError(const char* message, uint32_t timeoutMs = 2000) {
    showMessage(message, MessageIcon::ERROR, timeoutMs);
}

// SleepManager.cpp:112
vTaskDelay(pdMS_TO_TICKS(350));  // Why 350ms? No constant

// AppUi.cpp:51
static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;
```

## Recommended Fix
Create a centralized timing constants file:

1. **Create `components/cdc_ui/include/cdc_ui/AnimationTiming.h`:**
   ```cpp
   namespace cdc::ui {
   namespace timing {
       constexpr uint16_t MODAL_ENTER = 150;
       constexpr uint16_t MODAL_EXIT = 150;
       constexpr uint16_t TOAST_DEFAULT = 1500;
       constexpr uint16_t MESSAGE_AUTO_DISMISS = 2000;
       constexpr uint16_t EPD_PARTIAL_REFRESH_DELAY = 350;
       constexpr uint32_t INACTIVITY_TIMEOUT = 5 * 60 * 1000;
   }
   }
   ```

2. **Replace all hardcoded values with these tokens**

3. **Add documentation for each timing value explaining the rationale**

## References
- Design tokens best practices: https://design-tokens.github.io/
- E-Paper refresh timing documentation from CalEPD library
