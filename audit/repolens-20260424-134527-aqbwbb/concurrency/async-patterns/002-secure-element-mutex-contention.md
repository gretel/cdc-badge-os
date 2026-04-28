---
title: "[MEDIUM] TROPIC01 secure element mutex held during blocking SPI operations"
severity: MEDIUM
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "mutex"
  - "secure-element"
  - "blocking-io"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp`, the mutex is held for the entire duration of each secure-element operation, including blocking SPI communication with the TROPIC01 chip. This can cause priority inversion and task starvation when multiple tasks need to access the secure element, especially during longer operations like ECC signing or R-memory reads.

**Location:** `components/cdc_hal/src/Tropic01Element.cpp:332-352` (example: `eccGenerate`)

```cpp
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    // ...
    lock();  // Mutex acquired

    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    // ^ SPI communication happens here, blocking for ~10-50ms
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    handleSessionError(ret);

    unlock();  // Mutex released
    return mapResult(ret);
}
```

## Impact
- **Priority inversion:** High-priority tasks (e.g., keypad scanning, USB HID) may block waiting for the mutex while a low-priority task holds it during a long SPI operation
- **UI lag:** If the UI task needs to read the secure element (e.g., for FIDO2 display), it may block other critical operations
- **Timeout risks:** Tasks waiting on the mutex may exceed their timeout windows, causing cascading failures
- **No timeout on mutex:** The `lock()` uses `portMAX_DELAY`, meaning tasks can wait indefinitely

## Evidence
File: `components/cdc_hal/src/Tropic01Element.cpp`
- Lines 95-96: Simple mutex lock/unlock without timeout
- Lines 193-224: `sessionStart()` holds mutex for entire session establishment (~50-100ms)
- Lines 332-352: `eccGenerate()` holds mutex during ECC key generation
- Lines 481-508: `ecdsaSign()` holds mutex during signing operation
- Line 77: `void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }`

Multiple methods follow the same pattern: acquire mutex -> perform blocking SPI operation -> release mutex.

## Recommended Fix
Implement a timeout-based lock with fallback behavior:

```cpp
// Add timeout constant
static constexpr TickType_t SE_MUTEX_TIMEOUT = pdMS_TO_TICKS(100);  // 100ms timeout

// Replace lock() with timeout version
bool tryLock() {
    if (!mutex_) return true;
    return xSemaphoreTakeRecursive(mutex_, SE_MUTEX_TIMEOUT) == pdTRUE;
}

void unlock() {
    if (mutex_) xSemaphoreGiveRecursive(mutex_);
}

// Usage in eccGenerate():
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();  // Try to acquire with timeout

    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                   TR01_CURVE_ED25519 : TR01_CURVE_P256;

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

// Updated with timeout check:
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    // ...
    if (!tryLock()) {
        LOG_W(TAG, "Failed to acquire mutex for eccGenerate (timeout)");
        return SeResult::ERROR;  // Or retry logic
    }
    // ... rest same, unlock() still called
}
```

**Alternative: Split session management from operations**
```cpp
// Allow session restart without holding mutex for entire operation
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    // First, check session without holding mutex
    {
        lock();
        bool sessionOk = sessionActive_;
        unlock();
        
        if (!sessionOk) {
            // Try to start session outside mutex
            if (!sessionStart()) {
                return SeResult::SESSION_REQUIRED;
            }
        }
    }  // Mutex released before blocking operation

    // Now acquire mutex only for the actual operation
    lock();
    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                   TR01_CURVE_ED25519 : TR01_CURVE_P256;
    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    // ...
    unlock();
    return mapResult(ret);
}
```

## References
- [FreeRTOS Mutex Documentation](https://www.freertos.org/Using-mutexes.html)
- [Priority Inversion in FreeRTOS](https://www.freertos.org/Priority-Inversion.html)
- ESP-IDF Driver Locking Best Practices: `components/driver/include/driver/spi_master.h`
