---
title: "[HIGH] TCA9535 keypad task and semaphore not cleaned up on stop"
severity: HIGH
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/TCA9535Keypad.cpp`, the `init()` method creates a FreeRTOS task and binary semaphore using `xTaskCreate()` and `xSemaphoreCreateBinary()`, but the `stop()` method never cleans them up. The task handle `taskHandle_` and semaphore handle `semaphore_` are stored but not destroyed, causing resource leaks on each init/stop cycle.

**Location:** `components/cdc_hal/src/TCA9535Keypad.cpp:202-218` (creation) and `261-265` (stop method)

## Impact

1. **Task leak**: The keypad task continues running even after `stop()` is called, consuming:
   - Stack memory (defined by `TASK_STACK_SIZE`)
   - CPU time (task remains in ready state)
   - TCB (Task Control Block) structure

2. **Semaphore leak**: The binary semaphore uses heap memory (typically 16-32 bytes) that is never freed.

3. **Accumulated leaks**: If `init()`/`stop()` cycles occur multiple times (e.g., during sleep/wake cycles or reconfiguration), multiple tasks and semaphores accumulate, eventually causing:
   - Heap exhaustion
   - Task table exhaustion (limited by `configMAX_TASKS`)
   - System instability or crash

4. **Interrupt handler still active**: The GPIO ISR handler added at line 238 is also not removed, potentially firing and accessing stale task/semaphore data.

5. **Race conditions**: The running task might access freed resources if other parts of the system assume `stop()` cleaned everything up.

## Evidence

**File: `components/cdc_hal/src/TCA9535Keypad.cpp`**

1. **Task and semaphore created in init() (line 202-218):**
```cpp
// Create semaphore
semaphore_ = xSemaphoreCreateBinary();  // Created
if (!semaphore_) {
    LOG_E(TAG, "Failed to create semaphore");
    state_ = core::ServiceState::ERROR;
    return false;
}

// Create task
BaseType_t ret = xTaskCreate(taskFunc, "keypad", TASK_STACK_SIZE,
                              this, TASK_PRIORITY, &taskHandle_);  // Created
if (ret != pdPASS) {
    LOG_E(TAG, "Failed to create task");
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;
}

// Configure interrupt pin
// ...
gpio_install_isr_service(0);
gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);  // ISR installed
```

2. **stop() doesn't clean up (line 261-265):**
```cpp
void TCA9535Keypad::stop() {
    if (state_ == core::ServiceState::STARTED) {
        state_ = core::ServiceState::STOPPED;
        // taskHandle_ not deleted!
        // semaphore_ not deleted!
        // ISR handler not removed!
    }
}
```

3. **Member variables (from header):**
```cpp
TaskHandle_t taskHandle_ = nullptr;
SemaphoreHandle_t semaphore_ = nullptr;
```

4. **Task function runs indefinitely:**
```cpp
static void taskFunc(void* arg) {
    TCA9535Keypad* keypad = static_cast<TCA9535Keypad*>(arg);
    while (true) {
        // Wait for interrupt or timeout
        // Process keypad scan
        // ...
        vTaskDelay(pdMS_TO_TICKS(10));  // Runs forever
    }
}
```

5. **ISR still fires after stop:**
```cpp
static void isrHandler(void* arg) {
    TCA9535Keypad* keypad = static_cast<TCA9535Keypad*>(arg);
    BaseType_t higherPriorityTaskWoken;
    xSemaphoreGiveFromISR(keypad->semaphore_, &higherPriorityTaskWoken);  // Might fire after stop!
    return higherPriorityTaskWoken == pdTRUE;
}
```

## Recommended Fix

Add proper cleanup in the `stop()` method:

1. **Update stop() method:**
```cpp
void TCA9535Keypad::stop() {
    if (state_ == core::ServiceState::STARTED) {
        state_ = core::ServiceState::STOPPED;
    }
    
    // Remove ISR handler first (stop interrupts)
    gpio_isr_handler_remove(EXP_IRQ_PIN);
    
    // Delete task
    if (taskHandle_) {
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }
    
    // Delete semaphore
    if (semaphore_) {
        vSemaphoreDelete(semaphore_);
        semaphore_ = nullptr;
    }
    
    LOG_I(TAG, "TCA9535 keypad stopped");
}
```

2. **Add guard in init() for re-initialization:**
```cpp
bool TCA9535Keypad::init() {
    // Destroy old resources if exists
    if (taskHandle_) {
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }
    if (semaphore_) {
        vSemaphoreDelete(semaphore_);
        semaphore_ = nullptr;
    }
    // ... rest of init
}
```

3. **Alternatively, add a separate cleanup method:**
```cpp
void TCA9535Keypad::cleanup() {
    // Remove ISR handler
    gpio_isr_handler_remove(EXP_IRQ_PIN);
    
    // Delete task
    if (taskHandle_) {
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }
    
    // Delete semaphore
    if (semaphore_) {
        vSemaphoreDelete(semaphore_);
        semaphore_ = nullptr;
    }
}

void TCA9535Keypad::stop() {
    if (state_ == core::ServiceState::STARTED) {
        state_ = core::ServiceState::STOPPED;
    }
    cleanup();
}
```

## References

- [FreeRTOS Task API](https://www.freertos.org/tasks.html) - vTaskDelete
- [FreeRTOS Semaphore API](https://www.freertos.org/a00102.html) - vSemaphoreDelete
- [ESP-IDF GPIO ISR](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html) - gpio_isr_handler_remove
- [Resource Acquisition Is Initialization](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) - RAII pattern

</content>