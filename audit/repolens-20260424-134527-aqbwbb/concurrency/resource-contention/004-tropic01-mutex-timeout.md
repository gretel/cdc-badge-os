---
title: "[LOW] TROPIC01 Mutex Reentrancy Without Timeout"
severity: LOW
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

The TROPIC01 secure element uses a recursive mutex (`xSemaphoreCreateRecursiveMutex()`) but **never specifies a timeout** when acquiring it. If a task blocks forever waiting for the mutex, it can cause system deadlock.

**Location**: `components/cdc_hal/src/Tropic01Element.cpp:95-96, 195-225`

## Impact

**Resource Contention Risk**: If the TROPIC01 mutex is held for an extended period (e.g., during a long SPI transaction) and another task tries to acquire it, the waiting task will block forever (`portMAX_DELAY`). This can lead to:

1. **Deadlock**: Task waiting for TROPIC01 blocks indefinitely
2. **Starvation**: Lower-priority tasks cannot make progress
3. **System freeze**: Critical tasks blocked on TROPIC01 access

**Evidence**:
- `Tropic01Element.cpp:95`: `void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }`
- All secure-element operations use `portMAX_DELAY` with no fallback

## Evidence

**Mutex Definition** (`components/cdc_hal/src/Tropic01Element.cpp:95-96`):
```cpp
void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }
void unlock() { if (mutex_) xSemaphoreGiveRecursive(mutex_); }
```

**Usage in Operations** (`components/cdc_hal/src/Tropic01Element.cpp:325-350`):
```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();  // <-- Blocks forever if mutex held

    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    // ...

    unlock();
    return mapResult(ret);
}
```

**Mutex Creation** (`components/cdc_hal/src/Tropic01Element.cpp:134-142`):
```cpp
// Create mutex
mutex_ = xSemaphoreCreateRecursiveMutex();
if (!mutex_) {
    LOG_E(TAG, "Failed to create mutex");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

**Potential Deadlock Scenario**:
1. Task A calls `sessionStart()` - acquires mutex, starts secure session (can take 50-100ms)
2. Task B calls `eccGenerate()` - waits for mutex with `portMAX_DELAY`
3. If session start is slow, Task B waits forever
4. If Task A needs Task B to complete some I/O, deadlock

## Recommended Fix

1. **Add timeout to lock()** with error return:
```cpp
// In Tropic01Element.h
bool lock(uint32_t timeoutMs = 1000);  // Default 1s timeout
void unlock();

// In Tropic01Element.cpp
bool Tropic01Element::lock(uint32_t timeoutMs) {
    if (!mutex_) return false;
    TickType_t timeout = pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTakeRecursive(mutex_, timeout) == pdTRUE;
}

void Tropic01Element::unlock() {
    if (mutex_) xSemaphoreGiveRecursive(mutex_);
}
```

2. **Update all callers to handle timeout**:
```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    if (!lock(1000)) {  // 1s timeout
        return SeResult::TIMEOUT;  // New error code
    }

    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    // ...
    unlock();
    return mapResult(ret);
}
```

3. **Add lock statistics** for debugging:
```cpp
// Track lock contention
static uint32_t s_lockAttempts = 0;
static uint32_t s_lockTimeouts = 0;

bool Tropic01Element::lock(uint32_t timeoutMs) {
    s_lockAttempts++;
    if (!mutex_) return false;
    TickType_t start = xTaskGetTickCount();
    bool success = xSemaphoreTakeRecursive(mutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
    if (!success) {
        s_lockTimeouts++;
        LOG_W(TAG, "Mutex timeout after %lu ms", xTaskGetTickCount() - start);
    }
    return success;
}
```

## References

- FreeRTOS: Recursive mutex usage and timeouts
- ESP-IDF: Semaphore API documentation
- Concurrency: Deadlock prevention strategies
