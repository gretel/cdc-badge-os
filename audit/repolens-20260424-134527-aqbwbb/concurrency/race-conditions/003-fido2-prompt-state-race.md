---
title: "[MEDIUM] FIDO2 Prompt State Race - Static volatile State Not Thread-Safe"
severity: MEDIUM
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The FIDO2 UI module in `components/mod_fido2/src/Fido2Ui.cpp` uses `static volatile` variables for prompt state that can be accessed from multiple contexts (BLE callback, USB callback, UI task). While `volatile` prevents compiler optimization, it does not provide atomic operations or memory barriers needed for thread-safe access.

**Evidence** - Static state variables (lines 87-104):
```cpp
static SemaphoreHandle_t s_promptSem = nullptr;
static volatile fido2_user_presence_result_t s_promptResult = FIDO2_UP_PENDING;
static char s_promptRpId[FIDO2_RP_ID_MAX_LEN] = {};
static fido2_action_t s_promptAction = FIDO2_ACTION_SELECT;
static uint8_t s_promptReturnDepth = 0;
static ui::IView* s_promptReturnView = nullptr;
static bool s_promptWasLocked = false;
static bool s_promptBacklightWasOn = false;
static volatile bool s_promptActive = false;  // Race condition guard
```

**Evidence** - Race condition check in `fido2_ui_user_presence_callback()` (lines 467-487):
```cpp
fido2_user_presence_result_t fido2_ui_user_presence_callback(
    const char* rp_id,
    fido2_action_t action,
    const char* user_name
) {
    // ...
    // Race condition guard: If a SELECT prompt is active and a higher-priority
    // request comes in, approve the SELECT and process the new one.
    if (s_promptActive && s_promptAction == FIDO2_ACTION_SELECT &&
        (action == FIDO2_ACTION_REGISTER || action == FIDO2_ACTION_AUTHENTICATE)) {
        LOG_I(TAG, "Superseding active SELECT with %s - approving SELECT", actionStr);
        s_promptResult = FIDO2_UP_APPROVED;  // Non-atomic write
        s_promptActive = false;  // Non-atomic write
        if (s_promptSem) {
            xSemaphoreGive(s_promptSem);
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // Brief delay to let SELECT thread clean up
    }
    // ...
}
```

**Evidence** - State setup (lines 511-516):
```cpp
strncpy(s_promptRpId, rp_id ? rp_id : "Unknown", sizeof(s_promptRpId) - 1);
s_promptRpId[sizeof(s_promptRpId) - 1] = '\0';
s_promptAction = action;
s_promptResult = FIDO2_UP_PENDING;
s_promptActive = true;  // Mark prompt as active for race condition detection

while (xSemaphoreTake(s_promptSem, 0) == pdTRUE) {
    LOG_W(TAG, "Drained stale semaphore");
}
```

**The Race**:
1. Thread A (BLE callback) calls `fido2_ui_user_presence_callback()` with SELECT
2. Thread A writes `s_promptActive = true`, `s_promptAction = SELECT`
3. Thread B (USB callback) calls `fido2_ui_user_presence_callback()` with REGISTER
4. Thread B reads `s_promptActive` (true) and `s_promptAction` (SELECT)
5. Thread B writes `s_promptResult = APPROVED`, `s_promptActive = false`
6. Thread A continues, overwrites `s_promptActive = true`, `s_promptAction = REGISTER`

The comment says "Race condition guard" but the implementation is not truly atomic.

## Impact

**Incorrect User Experience**: If two FIDO2 operations happen concurrently (e.g., browser probing multiple devices):
1. SELECT prompt can be superseded incorrectly
2. REGISTER prompt can show SELECT result
3. Semaphore can be given twice causing undefined behavior

**Data Corruption**: Non-atomic writes to `s_promptRpId` (string copy) can leave it in a partially-written state if interrupted.

## Recommended Fix

Use a mutex to protect all prompt state access:

```cpp
// In Fido2Ui.cpp - add mutex
static portMUX_TYPE s_promptMux = portMUX_INITIALIZER_UNLOCKED;

// Wrap all state access:
fido2_user_presence_result_t fido2_ui_user_presence_callback(
    const char* rp_id,
    fido2_action_t action,
    const char* user_name
) {
    // Check for superseding atomically
    bool shouldSupersede = false;
    portENTER_CRITICAL(&s_promptMux);
    if (s_promptActive && s_promptAction == FIDO2_ACTION_SELECT &&
        (action == FIDO2_ACTION_REGISTER || action == FIDO2_ACTION_AUTHENTICATE)) {
        s_promptResult = FIDO2_UP_APPROVED;
        s_promptActive = false;
        shouldSupersede = true;
    }
    portEXIT_CRITICAL(&s_promptMux);

    if (shouldSupersede) {
        if (s_promptSem) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(s_promptSem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // ... rest of function

    portENTER_CRITICAL(&s_promptMux);
    strncpy(s_promptRpId, rp_id ? rp_id : "Unknown", sizeof(s_promptRpId) - 1);
    s_promptRpId[sizeof(s_promptRpId) - 1] = '\0';
    s_promptAction = action;
    s_promptResult = FIDO2_UP_PENDING;
    s_promptActive = true;
    
    while (xSemaphoreTake(s_promptSem, 0) == pdTRUE) {
        LOG_W(TAG, "Drained stale semaphore");
    }
    portEXIT_CRITICAL(&s_promptMux);

    // ... rest of function
}

// In promptComplete() - also protect state writes
static void promptComplete(fido2_user_presence_result_t result) {
    portENTER_CRITICAL(&s_promptMux);
    s_promptActive = false;
    s_promptResult = result;
    portEXIT_CRITICAL(&s_promptMux);

    // ... rest of function
}
```

## References

- ESP32-S3 SMP and critical sections: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html
- FIDO2 user presence flow: https://fidoalliance.org/specs/fido-v2.1-rd-20201208/fido-client-to-authenticator-protocol-v2.1-rd-20201208.html#user-verifier-interaction
