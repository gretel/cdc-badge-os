---
title: "[LOW] Tropic01Element uses coarse-grained mutex, serializing all secure element operations"
severity: LOW
domain: concurrency
lens: resource-contention
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `Tropic01Element` class uses a single recursive mutex to protect all secure element operations (line 93-94 in `components/cdc_hal/src/Tropic01Element.cpp`). This coarse-grained locking serializes all operations even when they could run concurrently, such as independent R-Memory reads or ECC public key reads that don't modify state.

```cpp
// Single mutex for all operations
void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }
void unlock() { if (mutex_) xSemaphoreGiveRecursive(mutex_); }
```

Every operation acquires the lock:
- `eccGenerate()` - line 329
- `eccImport()` - line 357
- `eccGetPublicKey()` - line 392
- `ecdsaSign()` - line 482
- `rmemRead()` - line 545
- `rmemWrite()` - line 577
- `getRandom()` - line 777
- etc.

## Impact
1. **Over-serialization**: Independent operations (e.g., reading from different R-Memory slots) are serialized unnecessarily.
2. **Lock contention**: Long operations (like `rmemWrite()` which erases then writes) block all other operations.
3. **Priority inversion**: High-priority tasks waiting for short operations may be blocked by long-running batch operations.
4. **FIDO2 task starvation**: The FIDO2 background task (line 34 in `mod_fido2/src/fido2.cpp`) may be delayed if another task holds the lock during a slow operation.

## Evidence
- File: `components/cdc_hal/src/Tropic01Element.cpp:93-94` (lock/unlock methods)
- File: `components/mod_fido2/src/fido2.cpp:34-117` (FIDO2 task that needs regular SE access)
- FIDO2 processing loop calls USB write which may need SE for signing
- All operations hold lock during potentially slow SPI transactions

## Recommended Fix
Use finer-grained locking or a read-write semaphore pattern:

```cpp
#include "freertos/semphr.h"

class Tropic01Element {
private:
    // Use read-write semaphore for better concurrency
    SemaphoreHandle_t rwMutex_ = nullptr;
    
    // Or use separate locks for different resource types
    // SemaphoreHandle_t sessionMutex_;  // For session state
    // SemaphoreHandle_t eccMutex_;      // For ECC operations
    // SemaphoreHandle_t rmemMutex_;     // For R-Memory operations
    
    void lockSession() { xSemaphoreTake(rwMutex_, portMAX_DELAY); }
    void unlockSession() { xSemaphoreGive(rwMutex_); }
    
    // For read-heavy operations, use recursive lock
    void lockRead() { xSemaphoreTake(rwMutex_, portMAX_DELAY); }
    void lockWrite() { xSemaphoreTake(rwMutex_, portMAX_DELAY); }
    void unlock() { xSemaphoreGive(rwMutex_); }
};

// Example: R-Memory reads could potentially run concurrently
SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                    uint16_t* actualLen) {
    lockRead();  // Shorter critical section for read-only
    
    if (!ensureSession("rmemRead")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }
    
    uint16_t bytesRead = 0;
    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);
    handleSessionError(ret);
    
    unlock();
    // ... rest unchanged
}

// R-Memory writes need exclusive access (includes erase)
SeResult Tropic01Element::rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) {
    lockWrite();  // Exclusive for write + erase
    
    if (!ensureSession("rmemWrite")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }
    
    lt_ret_t ret = lt_r_mem_data_write(&handle_, slot, data, len);
    handleSessionError(ret);
    
    unlock();
    // ... rest unchanged
}
```

Alternatively, consider using a task queue pattern where all SE operations are serialized through a dedicated worker task, which naturally prevents contention.

## References
- FreeRTOS read-write semaphore: https://docs.freertos.org/~/download/Documentation/api/html/group__xSemaphoreCreateCounting.html
- Fine-grained locking patterns: https://www.kernel.org/doc/Documentation/locking/lock-class.txt
- ESP32 SPI flash locking: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/multithreading.html#flash-and-eeprom-access
