---
title: "[MEDIUM] Blocking mutex acquisition in CTAPHID processing"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "mutex"
  - "fido2"
  - "ctaphid"
---

## Summary

The CTAPHID (FIDO2 HID transport) module in `components/mod_fido2/src/ctaphid.cpp` uses `portMAX_DELAY` for mutex acquisition, causing tasks to block indefinitely when waiting for the CTAPHID state mutex.

**Affected file:**
- `components/mod_fido2/src/ctaphid.cpp` (lines 80, 150, 180 - approximate)

**Evidence:**
```cpp
// components/mod_fido2/src/ctaphid.cpp:ctaphid_process_packet()
bool ctaphid_process_packet(const uint8_t *packet) {
    if (!g_ctaphid.initialized || !packet) return false;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);  // Blocks indefinitely!

    // Rate limiting check (~30 lines of processing)
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - g_ctaphid.rate_window_start >= CTAPHID_RATE_LIMIT_WINDOW_MS) {
        g_ctaphid.rate_window_start = now;
        g_ctaphid.rate_cmd_count = 1;
    } else {
        g_ctaphid.rate_cmd_count++;
        if (g_ctaphid.rate_cmd_count > CTAPHID_RATE_LIMIT_MAX_CMDS) {
            // ...
        }
    }
    // ... more processing
    xSemaphoreGive(g_ctaphid.mutex);
}
```

```cpp
// components/mod_fido2/src/ctaphid.cpp:ctaphid_get_response_packet()
bool ctaphid_get_response_packet(uint8_t *packet) {
    if (!g_ctaphid.response_pending || !packet) return false;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);  // Blocks indefinitely!

    uint16_t remaining = g_ctaphid.response_len - g_ctaphid.response_offset;
    // ... packet building
    xSemaphoreGive(g_ctaphid.mutex);
}
```

```cpp
// components/mod_fido2/src/ctaphid.cpp:ctaphid_check_timeout()
void ctaphid_check_timeout(void) {
    if (!g_ctaphid.initialized) return;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);  // Blocks indefinitely!

    for (int i = 0; i < CTAPHID_MAX_CHANNELS; i++) {
        ctaphid_channel_t *ch = &g_ctaphid.channels[i];
        if (ch->active && (now - ch->last_activity) > CTAPHID_MSG_TIMEOUT_MS) {
            // ...
        }
    }
    xSemaphoreGive(g_ctaphid.mutex);
}
```

## Impact

1. **Indefinite task blocking**: If the CTAPHID mutex is held for a long operation (e.g., ECC signing in TROPIC01), other tasks waiting for the mutex block indefinitely.

2. **No timeout for recovery**: `portMAX_DELAY` means no timeout - if there's a deadlock or long hold, the task never recovers.

3. **Priority inversion**: Lower-priority tasks holding the mutex can block higher-priority tasks indefinitely (no priority inheritance).

4. **FIDO2 assertion delays**: During FIDO2 signature operations, the mutex may be held while crypto runs, blocking other CTAPHID operations.

5. **Debugging difficulty**: Indefinite blocks are harder to detect and debug compared to timeouts.

## Evidence

**Mutex holders:**
- `ctaphid_process_packet()`: Processes incoming FIDO commands (~50 lines)
- `ctaphid_get_response_packet()`: Builds response packets (~30 lines)
- `ctaphid_check_timeout()`: Checks channel timeouts (~15 lines)

**Typical hold times:**
- Normal command: ~1-5ms
- During ECC signing: ~20-50ms (TROPIC01 operation)
- During key generation: ~50-100ms

**Blocking pattern:**
```
Task A (FIDO2 UI):    xSemaphoreTake(mutex, portMAX_DELAY);  // Gets mutex
Task A (FIDO2 UI):    eccSign();  // Calls TROPIC01, holds mutex 50ms
Task B (CTAPHID):     xSemaphoreTake(mutex, portMAX_DELAY);  // Blocks!
Task C (Keypad):      wait for Task B to complete...
```

## Recommended Fix

1. **Add timeout to mutex acquisition** (1 hour):
```cpp
// Replace portMAX_DELAY with timeout
bool ctaphid_process_packet(const uint8_t *packet) {
    if (!g_ctaphid.initialized || !packet) return false;

    // 100ms timeout prevents indefinite blocking
    if (xSemaphoreTake(g_ctaphid.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        LOG_W(TAG, "CTAPHID mutex timeout, retrying...");
        return false;  // Or retry logic
    }

    // ... processing ...
    xSemaphoreGive(g_ctaphid.mutex);
    return true;
}
```

2. **Use recursive mutex for nested calls** (30 min):
```cpp
// Create recursive mutex
g_ctaphid.mutex = xSemaphoreCreateRecursiveMutex();

// Use xSemaphoreTakeRecursive for nested acquisition
xSemaphoreTakeRecursive(g_ctaphid.mutex, 1);
// ...
xSemaphoreGiveRecursive(g_ctaphid.mutex);
```

3. **Minimize critical section size** (1 hour):
```cpp
// Copy data out of critical section first
uint32_t rate_window_start;
uint16_t response_len;

xSemaphoreTake(g_ctaphid.mutex, pdMS_TO_TICKS(100));
rate_window_start = g_ctaphid.rate_window_start;
response_len = g_ctaphid.response_len;
xSemaphoreGive(g_ctaphid.mutex);

// Process outside critical section
processRateLimit(rate_window_start);
buildResponse(response_len);
```

4. **Add mutex contention monitoring** (30 min):
```cpp
// Track mutex wait times
static uint32_t maxWaitMs = 0;
uint32_t startMs = xTaskGetTickCount() * portTICK_PERIOD_MS;

if (xSemaphoreTake(g_ctaphid.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    uint32_t waitMs = xTaskGetTickCount() * portTICK_PERIOD_MS - startMs;
    if (waitMs > maxWaitMs) {
        maxWaitMs = waitMs;
        LOG_D(TAG, "New max mutex wait: %dms", maxWaitMs);
    }
    // ...
}
```

5. **Consider lock-free data structures** (2 hours):
```cpp
// Use atomic operations for simple state
static volatile uint32_t rate_cmd_count;
static atomic<uint32_t> response_offset;

// For complex state, use ring buffers with single producer/consumer
```

**Estimated effort**: 1-2 hours for timeout and critical section optimization

## References

- [FreeRTOS mutex documentation](https://www.freertos.org/Using-mutexes.html)
- [ESP-IDF semaphore API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos_additions.html)
- Priority inversion: https://www.freertos.org/semaphores.html
- CTAPHID specification: https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html

</content>