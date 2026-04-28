---
title: "[INFO] Resource Contention Audit Summary - CDC Badge OS"
severity: INFO
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

This document summarizes the resource contention patterns identified in the CDC Badge OS firmware during the concurrency audit. The findings cover lock contention, queue exhaustion, and shared resource access patterns across the ESP32-S3 embedded system.

## Findings Overview

### [HIGH] 001-spi-bus-race-condition.md
**Shared SPI Bus Race Condition Between Display and TROPIC01**
- The SPI2_HOST bus is shared between E-Paper display and TROPIC01 secure element
- No mutex protects concurrent access
- Long-running display updates (100-500ms) can starve secure-element operations
- **Impact**: Data corruption, protocol violations, intermittent failures

### [MEDIUM] 002-eventbus-queue-exhaustion.md
**EventBus Queue Exhaustion Under High Event Load**
- Fixed-size queue (32 events) with no backpressure
- Silent event drops when queue fills
- No monitoring or statistics
- **Impact**: Lost critical events (lock/unlock, battery warnings)

### [MEDIUM] 003-keypad-buffer-race-condition.md
**Keypad Ring Buffer Race Condition Without Mutex**
- Ring buffer accessed from keypad task (priority 5) and main loop
- No synchronization for read/write operations
- **Impact**: Data corruption, lost key presses, duplicate events

### [LOW] 004-tropic01-mutex-timeout.md
**TROPIC01 Mutex Reentrancy Without Timeout**
- Recursive mutex uses `portMAX_DELAY` (blocks forever)
- No fallback for lock contention
- **Impact**: Potential deadlock if mutex held during slow SPI transaction

### [MEDIUM] 005-display-render-task-starvation.md
**Display Render Task Starvation Due to Blocking Mutex**
- Render task blocks forever on mutex during 100-500ms updates
- Multiple flush requests queue up but only one executes
- **Impact**: UI lag, missed updates, starvation

### [MEDIUM] 006-pinmanager-concurrent-access.md
**PinManager Concurrent Access Without Synchronization**
- PIN retry counters accessed from multiple contexts without mutex
- Race condition can corrupt retry counts
- **Impact**: Security bypass, incorrect lockout behavior

## Common Patterns

### 1. Missing Timeouts
Multiple mutex acquisitions use `portMAX_DELAY` without timeout:
- `Tropic01Element::lock()` - line 95
- `EpaperDisplay::flushSync()` - line 179

### 2. No Backpressure
Event producers don't check queue capacity:
- `EventBus::publish()` - drops events silently
- `Fido2Module::onFidoSetReport()` - drops packets

### 3. Shared Resources Without Locks
- SPI bus: shared between display and TROPIC01
- Ring buffers: accessed from multiple tasks
- PIN state: accessed from UI, serial, modules

### 4. Long Critical Sections
- Display update: 100-500ms while holding mutex
- Secure session start: 50-100ms while holding mutex

## Recommended Architecture Changes

### Priority 1: Shared SPI Mutex
Create a shared SPI bus mutex that both display and TROPIC01 use:
```cpp
// In SpiBus.cpp
static SemaphoreHandle_t s_spiMutex = nullptr;
SemaphoreHandle_t getSharedSpiMutex() { return s_spiMutex; }

// In initSharedSpiBus()
s_spiMutex = xSemaphoreCreateMutex();
```

### Priority 2: Event Queue Monitoring
Add backpressure detection and statistics:
```cpp
uint32_t EventBus::getQueueDepth() const;
bool EventBus::isNearCapacity(float threshold);
```

### Priority 3: Timeout on All Mutexes
Replace `portMAX_DELAY` with configurable timeouts:
```cpp
bool lock(uint32_t timeoutMs = 1000);
```

## Related Findings

- **Race Conditions**: See `concurrency/race-conditions/` for TOCTOU and state races
- **Async Patterns**: See `concurrency/async-patterns/` for queue and task patterns

## References

- FreeRTOS: Mutex and queue documentation
- ESP-IDF: Task synchronization patterns
- Embedded concurrency best practices
