---
title: "[HIGH] BLE UART Ring Buffer Race Condition - Non-Atomic RX Buffer Access"
severity: HIGH
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `BleUartService` class in `components/mod_ble_serial/src/BleUartService.cpp` uses `volatile` for ring buffer indices (`rxHead_`, `rxTail_`) but lacks proper atomic operations for ring buffer access. The ring buffer is accessed from two contexts:
1. **ISR context**: `onRxData()` is called from BLE GATT write callback (potentially interrupt context)
2. **Task context**: `getchar()`, `read()`, and `available()` are called from regular tasks

The current implementation uses `volatile` qualifiers (lines 124-125 in header) which prevents compiler optimization but does NOT provide atomic read-modify-write operations needed for safe concurrent access.

**Evidence** - Header file (`components/mod_ble_serial/include/mod_ble_serial/BleUartService.h:124-125`):
```cpp
volatile size_t rxHead_ = 0;
volatile size_t rxTail_ = 0;
```

**Evidence** - `onRxData()` writing head (line 256-263):
```cpp
void BleUartService::onRxData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        size_t nextHead = (rxHead_ + 1) % RX_BUFFER_SIZE;  // Non-atomic read-modify-write
        if (nextHead == rxTail_) {
            rxTail_ = (rxTail_ + 1) % RX_BUFFER_SIZE;  // Non-atomic
        }
        rxBuffer_[rxHead_] = data[i];  // Non-atomic read
        rxHead_ = nextHead;  // Non-atomic write
    }
}
```

**Evidence** - `available()` reading both indices (line 194-201):
```cpp
size_t BleUartService::available() const {
    size_t head = rxHead_;  // Non-atomic read
    size_t tail = rxTail_;  // Non-atomic read
    if (head >= tail) {
        return head - tail;
    } else {
        return RX_BUFFER_SIZE - tail + head;
    }
}
```

## Impact

**Data Corruption**: Without atomic operations or mutex protection, concurrent access can lead to:
1. **Lost data**: `rxHead_` update can be overwritten if ISR and task access interleave
2. **Duplicate data**: `rxTail_` can be read twice with different values
3. **Buffer overflow**: Race between checking `nextHead == rxTail_` and updating indices

**Symptoms**: Characters appearing twice, missing characters, or corrupted BLE serial data.

## Recommended Fix

Replace `volatile` with proper `std::atomic` operations and use memory ordering:

```cpp
// In header (BleUartService.h:124-127)
std::atomic<size_t> rxHead_{0};
std::atomic<size_t> rxTail_{0};
std::atomic<bool> txCongested_{false};
std::atomic<bool> txInProgress_{false};

// In onRxData() (BleUartService.cpp:256-263)
void BleUartService::onRxData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        size_t nextHead = rxHead_.load(std::memory_order_relaxed);
        nextHead = (nextHead + 1) % RX_BUFFER_SIZE;
        
        size_t tail = rxTail_.load(std::memory_order_relaxed);
        if (nextHead == tail) {
            rxTail_.store((tail + 1) % RX_BUFFER_SIZE, std::memory_order_release);
        }
        
        rxBuffer_[nextHead] = data[i];
        rxHead_.store(nextHead, std::memory_order_release);
    }
}

// In available() (BleUartService.cpp:194-201)
size_t BleUartService::available() const {
    size_t head = rxHead_.load(std::memory_order_acquire);
    size_t tail = rxTail_.load(std::memory_order_acquire);
    if (head >= tail) {
        return head - tail;
    } else {
        return RX_BUFFER_SIZE - tail + head;
    }
}

// In getchar() (BleUartService.cpp:210-216)
int BleUartService::getchar() {
    size_t tail = rxTail_.load(std::memory_order_relaxed);
    size_t head = rxHead_.load(std::memory_order_acquire);
    
    if (head == tail) return -1;
    
    uint8_t c = rxBuffer_[tail];
    rxTail_.store((tail + 1) % RX_BUFFER_SIZE, std::memory_order_release);
    return c;
}
```

## References

- C++11 `<atomic>` documentation: https://en.cppreference.com/w/cpp/atomic
- FreeRTOS atomic operations: https://www.freertos.org/a00111.html
- ESP32-S3 dual-core considerations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/freertos-smp.html
