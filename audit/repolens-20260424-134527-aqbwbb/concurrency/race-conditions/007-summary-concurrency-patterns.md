---
title: "[INFO] Concurrency Patterns Summary - ESP32-S3 Dual-Core Considerations"
severity: INFO
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

This is a summary document capturing the overall concurrency architecture and patterns used in the CDC Badge OS codebase. The ESP32-S3 is a dual-core microcontroller with FreeRTOS SMP (Symmetric Multi-Processing), meaning code can execute on two cores simultaneously. This document catalogs where concurrency protection is used and where it might be needed.

## Concurrency Protection Patterns Used

### 1. Critical Sections (portMUX_TYPE)
**Location**: `components/cdc_hal/src/TCA9535Keypad.cpp`
**Usage**: Protects key buffer (`bufferHead_`, `bufferTail_`) from concurrent ISR/task access
**Pattern**:
```cpp
static portMUX_TYPE bufferMux_ = portMUX_INITIALIZER_UNLOCKED;
portENTER_CRITICAL(&bufferMux_);
// ... buffer operations ...
portEXIT_CRITICAL(&bufferMux_);
```

### 2. Semaphores
**Location**: `components/cdc_hal/src/TCA9535Keypad.cpp`
**Usage**: Signaling between ISR and task for keypad interrupts
**Pattern**:
```cpp
SemaphoreHandle_t semaphore_ = xSemaphoreCreateBinary();
// In ISR:
xSemaphoreGiveFromISR(semaphore_, &xHigherPriorityTaskWoken);
// In task:
xSemaphoreTake(semaphore_, portMAX_DELAY);
```

### 3. Task Notifications
**Location**: `components/cdc_hal/src/EpaperDisplay.cpp`
**Usage**: Waking render task from main task
**Pattern**:
```cpp
xTaskNotifyGive(s_renderTask);
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

### 4. Mutex for Render State
**Location**: `components/cdc_hal/src/EpaperDisplay.cpp`
**Usage**: Protecting render pending state
**Pattern**:
```cpp
static SemaphoreHandle_t s_renderMutex = nullptr;
xSemaphoreTake(s_renderMutex, portMAX_DELAY);
// ... state access ...
xSemaphoreGive(s_renderMutex);
```

### 5. Volatile for ISR Communication
**Location**: Multiple files
**Usage**: Variables shared between ISR and task
**Pattern**:
```cpp
static volatile bool s_renderPending = false;
static volatile size_t rxHead_ = 0;
```

**Note**: `volatile` prevents compiler optimization but does NOT provide atomicity.

## Identified Race Conditions (See Separate Issues)

1. **BLE UART Ring Buffer** (`components/mod_ble_serial`) - Non-atomic ring buffer access
2. **PinManager Lockout** (`components/cdc_core`) - TOCTOU in lockout timer
3. **FIDO2 Prompt State** (`components/mod_fido2`) - Static state not thread-safe
4. **TropicStorage NVS** (`components/cdc_core`) - Concurrent NVS access
5. **Epaper Display Render** (`components/cdc_hal`) - Non-atomic render flags
6. **Keypad Buffer** (`components/cdc_hal`) - Missing critical section in `isKeyPressed()`

## Areas Requiring Attention

### 1. NVS Access Patterns
Multiple components access NVS without synchronization:
- `TropicStorage.cpp` - Multiple NVS reads/writes per operation
- `ModuleRegistry.cpp` - Module list persistence
- `I18n.cpp` - Language preference
- `SettingsHandlers.cpp` - Display settings

**Risk**: NVS operations are not atomic and can conflict if accessed concurrently.

### 2. Static State in UI
`components/cdc_os_ui/src/AppUi.cpp` has multiple static state variables:
- `s_lastMinute` - Used for clock update throttling
- `s_lastUsbConnected`, `s_lastCharging` - Status icon caching
- `s_ignoreKeyUntilRelease` - Key press debouncing

**Risk**: These are accessed from the main loop only, so currently safe. But if moved to a task or timer, they would need protection.

### 3. Service Registry Access
`components/cdc_core/src/ServiceRegistry.cpp` - No synchronization for service lookup

**Risk**: If services are registered/looked up from multiple tasks, race conditions could occur.

### 4. Event Bus
`components/cdc_core/src/EventBus.cpp` - Uses FreeRTOS queue for ISR safety

**Status**: Properly implemented with queue for ISR-to-task communication.

## Recommendations

1. **Use FreeRTOS primitives consistently** - The codebase mixes `portMUX_TYPE` (for short critical sections) and semaphores. Consider standardizing.

2. **Document thread-safety guarantees** - Add comments to classes indicating which methods are thread-safe and which require external synchronization.

3. **Consider std::atomic** - For simple counters/flags, `std::atomic` provides clearer semantics than `volatile`.

4. **NVS batching** - Group related NVS operations to minimize transaction conflicts.

5. **Add concurrency tests** - Create stress tests that exercise code paths from multiple tasks.

## ESP32-S3 Dual-Core Considerations

The ESP32-S3 has two Xtensa LX7 cores running at 240 MHz. FreeRTOS SMP schedules tasks across both cores. Key considerations:

1. **Cache coherency**: Both cores share cache, but memory operations may need barriers.
2. **Interrupts**: Can be pinned to specific cores or load-balanced.
3. **PortMUX**: Provides spinlock-like protection for short critical sections (disable interrupts on current core).
4. **Atomic operations**: ESP32-S3 supports hardware atomic operations for 32-bit values.

## References

- ESP-IDF FreeRTOS SMP: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/freertos-smp.html
- ESP32-S3 Technical Reference Manual: https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf
- FreeRTOS critical sections: https://www.freertos.org/a00111.html
