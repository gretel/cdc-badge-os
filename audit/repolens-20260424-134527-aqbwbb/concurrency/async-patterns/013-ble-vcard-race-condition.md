---
title: "[MEDIUM] Race condition in ble_vcard_poll_nearby() - check-then-act pattern"
severity: MEDIUM
domain: concurrency/async-patterns
lens: freeRTOS-synchronization
labels:
  - "race-condition"
  - "check-then-act"
---

## Summary
The `ble_vcard_poll_nearby()` function in `components/mod_vcard/src/ble_vcard.cpp` uses a check-then-act pattern where `s_nearby_available` is checked outside the critical section, then the mutex is acquired to read `s_nearby_peer`. This creates a window where the state could change between the check and the read.

**Location:** `components/mod_vcard/src/ble_vcard.cpp:1150-1160`

## Impact
The race condition occurs because:

1. Line 1151: `s_nearby_available` is checked WITHOUT holding `s_peer_mutex`
2. Line 1153: Mutex is acquired
3. Between lines 1151 and 1153, another task could:
   - Set `s_nearby_available = false` (in `processScanResults()`)
   - Update `s_nearby_peer` with new data
   - Set `s_nearby_available = true` again with different peer data

This could lead to:
- **Stale data returned**: The function returns `true` (found a peer) but `s_nearby_available` was already cleared by another task
- **Inconsistent state**: `s_nearby_peer` might be from an old scan while `s_nearby_available` reflects new state
- **Lost notifications**: If `processScanResults()` updates the peer while `poll_nearby()` is waiting for the mutex, the update could be lost

The race is narrow but real, especially on a system with multiple tasks accessing the peer list.

## Evidence
```cpp
// Line 124: Declaration
static bool s_nearby_available = false;
static vcard_peer_t s_nearby_peer;

// Line 580-597: processScanResults() sets the flag under mutex
if (xSemaphoreTake(s_peer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // ... find peer ...
    if (!found && s_peer_count < MAX_PEERS) {
        s_peers[s_peer_count++] = peer;
        s_nearby_peer = peer;      // Write 1
        s_nearby_available = true; // Write 2
    }
    xSemaphoreGive(s_peer_mutex);
}

// Line 1150-1160: poll_nearby() checks outside mutex
bool ble_vcard_poll_nearby(vcard_peer_t* out) {
    if (!out || !s_nearby_available) return false;  // Read WITHOUT mutex - RACE!
    
    if (xSemaphoreTake(s_peer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *out = s_nearby_peer;          // Read WITH mutex
        s_nearby_available = false;    // Clear WITH mutex
        xSemaphoreGive(s_peer_mutex);
        return true;
    }
    return false;
}
```

**Timeline showing the race:**
```
Task A (poll_nearby)          Task B (processScanResults)
------------------            -------------------------
1. Check s_nearby_available = true
                               2. Acquire mutex
                               3. Update s_nearby_peer = new_peer
                               4. Set s_nearby_available = true
                               5. Release mutex
6. Acquire mutex
7. Read s_nearby_peer -> new_peer (correct)
8. Clear s_nearby_available
9. Release mutex
10. Return true with new_peer
```

This seems fine, but consider:
```
Task A (poll_nearby)          Task B (processScanResults)
------------------            -------------------------
1. Check s_nearby_available = true
                               2. Acquire mutex
                               3. Update s_nearby_peer = peer_A
                               4. Set s_nearby_available = true
                               5. Release mutex
                               6. Acquire mutex (another scan)
                               7. Update s_nearby_peer = peer_B
                               8. Set s_nearby_available = true
                               9. Release mutex
10. Acquire mutex
11. Read s_nearby_peer -> peer_B (correct, but not what we expected)
```

The more subtle race is when `s_nearby_available` is checked but then cleared by another poll:
```
Task A (poll_nearby)          Task B (poll_nearby)
------------------            ---------------------
1. Check s_nearby_available = true
2. (preempted)
3. Check s_nearby_available = true
4. Acquire mutex
5. Read s_nearby_peer
6. Clear s_nearby_available
7. Release mutex
8. Return true
9. Acquire mutex (Task A resumes)
10. Read s_nearby_peer (still valid)
11. Clear s_nearby_available (already false)
12. Release mutex
13. Return true
```

Both tasks return the same peer - not necessarily wrong, but could be unexpected.

## Recommended Fix
Move the check inside the critical section:

```cpp
bool ble_vcard_poll_nearby(vcard_peer_t* out) {
    if (!out) return false;
    
    if (xSemaphoreTake(s_peer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (!s_nearby_available) {
            xSemaphoreGive(s_peer_mutex);
            return false;
        }
        *out = s_nearby_peer;
        s_nearby_available = false;
        xSemaphoreGive(s_peer_mutex);
        return true;
    }
    return false;
}
```

This ensures:
1. The check and the read happen atomically
2. No window for state to change between check and read
3. The function either returns a valid peer or false, never inconsistent state

## References
- [FreeRTOS mutex usage patterns](https://www.freertos.org/Using-counting-semaphores-as-variables.html)
- [Check-then-act race condition pattern](https://docs.oracle.com/javase/tutorial/essential/concurrency/lazy.html)
- [ESP32-S3 concurrency guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html)
