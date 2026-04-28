---
title: "[MEDIUM] Incomplete race-condition guard for FIDO2 user-presence prompt state"
severity: MEDIUM
domain: concurrency/async-patterns
lens: freeRTOS-synchronization
labels:
  - "race-condition"
  - "shared-state"
---

## Summary
The FIDO2 user-presence prompt state in `components/mod_fido2/src/Fido2Ui.cpp` uses a `volatile` guard variable (`s_promptActive`) to detect race conditions when multiple browser requests arrive, but the related state variables are not consistently protected. This creates a window where the race-condition detection logic can read stale or partially-updated state.

**Location:** `components/mod_fido2/src/Fido2Ui.cpp:97-104`, `components/mod_fido2/src/Fido2Ui.cpp:476-486`

## Impact
When a browser sends a SELECT request followed quickly by a REGISTER or AUTH request:
1. The code checks `s_promptActive && s_promptAction == FIDO2_ACTION_SELECT` (line 476)
2. If true, it updates `s_promptResult` and `s_promptActive` (lines 479-480)
3. However, `s_promptResult` is `volatile` but `s_promptAction` and `s_promptRpId` are not
4. On ESP32-S3 (32-bit XTENDS/LX7), multi-byte writes are not atomic
5. A context switch between writes could leave state inconsistent

This could lead to:
- **Incorrect prompt state**: The new prompt might inherit stale `s_promptAction` or `s_promptRpId` values
- **Missed race detection**: If `s_promptActive` is read before `s_promptAction` is updated
- **UI inconsistency**: The displayed RP ID might not match the actual action

## Evidence
```cpp
// Line 95-104: State declaration
static SemaphoreHandle_t s_promptSem = nullptr;
static volatile fido2_user_presence_result_t s_promptResult = FIDO2_UP_PENDING;  // Only result is volatile
static char s_promptRpId[FIDO2_RP_ID_MAX_LEN] = {};                               // Not volatile
static fido2_action_t s_promptAction = FIDO2_ACTION_AUTHENTICATE;                 // Not volatile
static uint8_t s_promptReturnDepth = 0;
static ui::IView* s_promptReturnView = nullptr;
static bool s_promptWasLocked = false;
static bool s_promptBacklightWasOn = false;
static volatile bool s_promptActive = false;  // Race condition guard

// Line 476-486: Race detection with partial protection
if (s_promptActive && s_promptAction == FIDO2_ACTION_SELECT &&
    (action == FIDO2_ACTION_REGISTER || action == FIDO2_ACTION_AUTHENTICATE)) {
    LOG_I(TAG, "Superseding active SELECT with %s - approving SELECT", actionStr);
    s_promptResult = FIDO2_UP_APPROVED;  // Write 1
    s_promptActive = false;               // Write 2
    if (s_promptSem) {
        xSemaphoreGive(s_promptSem);
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // Context switch possible here
}

// Line 510-514: State update for new prompt (no synchronization)
strncpy(s_promptRpId, rp_id ? rp_id : "Unknown", sizeof(s_promptRpId) - 1);  // Write 1
s_promptAction = action;                                                       // Write 2
s_promptResult = FIDO2_UP_PENDING;                                             // Write 3
s_promptActive = true;                                                         // Write 4
```

The race-condition guard checks `s_promptActive` AND `s_promptAction` together, but only `s_promptActive` is `volatile`. Between the check on line 476 and the semaphore give on line 482, another task could potentially see inconsistent state.

## Recommended Fix
Option 1 (preferred for simplicity): Protect all prompt state with a mutex:

```cpp
// Add mutex declaration
static SemaphoreHandle_t s_promptMutex = nullptr;

// In fido2_ui_init():
if (!s_promptMutex) {
    s_promptMutex = xSemaphoreCreateMutex();
}

// Wrap all prompt state access:
fido2_user_presence_result_t fido2_ui_user_presence_callback(...) {
    // Check for superseding with mutex
    xSemaphoreTake(s_promptMutex, portMAX_DELAY);
    bool supersede = s_promptActive && s_promptAction == FIDO2_ACTION_SELECT &&
                     (action == FIDO2_ACTION_REGISTER || action == FIDO2_ACTION_AUTHENTICATE);
    if (supersede) {
        s_promptResult = FIDO2_UP_APPROVED;
        s_promptActive = false;
        if (s_promptSem) {
            xSemaphoreGive(s_promptSem);
        }
    }
    xSemaphoreGive(s_promptMutex);
    
    if (supersede) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // ... rest of function ...
    
    // Update state with mutex
    xSemaphoreTake(s_promptMutex, portMAX_DELAY);
    strncpy(s_promptRpId, rp_id ? rp_id : "Unknown", sizeof(s_promptRpId) - 1);
    s_promptAction = action;
    s_promptResult = FIDO2_UP_PENDING;
    s_promptActive = true;
    xSemaphoreGive(s_promptMutex);
    
    // ... rest of function ...
}
```

Option 2 (lighter weight): Make all state variables `volatile` and use memory barriers:

```cpp
static volatile fido2_user_presence_result_t s_promptResult = FIDO2_UP_PENDING;
static volatile char s_promptRpId[FIDO2_RP_ID_MAX_LEN] = {};
static volatile fido2_action_t s_promptAction = FIDO2_ACTION_AUTHENTICATE;
static volatile uint8_t s_promptReturnDepth = 0;
static volatile ui::IView* s_promptReturnView = nullptr;
static volatile bool s_promptWasLocked = false;
static volatile bool s_promptBacklightWasOn = false;
static volatile bool s_promptActive = false;

// Add memory barrier after writing state
#define PROMPT_STATE_BARRIER() portMEMORY_BARRIER()
```

Option 1 is recommended because:
- Mutex provides both atomicity and ordering guarantees
- Clearer intent and easier to maintain
- Semaphore already used for prompt signaling, so mutex addition is consistent

## References
- [FreeRTOS synchronization primitives](https://www.freertos.org/Using-counting-semaphores-as-variables.html)
- [ESP32-S3 memory mapping and atomicity](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html)
- [CTAP2 specification - User Presence](https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-v2.1-ps-20210615.html#user-presence)
