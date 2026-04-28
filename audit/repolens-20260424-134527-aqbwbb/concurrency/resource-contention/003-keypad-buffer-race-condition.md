---
title: "[MEDIUM] Keypad Ring Buffer Race Condition Without Mutex"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

The TCA9535 keypad implementation uses a ring buffer (`s_keyBuffer`) to store key events between the keypad polling task and the main UI loop, but **no mutex or atomic operations protect concurrent read/write access** to this buffer.

**Location**: `components/cdc_hal/src/TCA9535Keypad.cpp:140-200`

## Impact

**Resource Contention Risk**: The keypad task (priority 5) can write to the buffer while the main loop reads from it, potentially causing:

1. **Data corruption**: Partial reads of key events (2-byte struct)
2. **Lost key presses**: Buffer index race conditions
3. **Duplicate key events**: Same key read multiple times or missed

**Evidence**:
- `TCA9535Keypad.cpp:140-142`: Ring buffer defined with no synchronization
- `TCA9535Keypad.cpp:170-180`: `bufferAddKey()` called from keypad task (ISR context)
- `TCA9535Keypad.cpp:182-190`: `bufferGetKey()` called from main loop

## Evidence

**Ring Buffer Definition** (`components/cdc_hal/src/TCA9535Keypad.cpp:140-145`):
```cpp
class TCA9535Keypad : public IKeypad {
private:
    // ...
    Key s_keyBuffer[KEY_BUFFER_SIZE];
    uint8_t s_bufferHead = 0;
    uint8_t s_bufferTail = 0;
    uint8_t s_bufferCount = 0;
    // ...
};
```

**Buffer Write** (`components/cdc_hal/src/TCA9535Keypad.cpp:330-350`):
```cpp
void TCA9535Keypad::bufferAddKey(Key key) {
    if (s_bufferCount >= KEY_BUFFER_SIZE) {
        // Buffer full - drop oldest
        s_bufferHead = (s_bufferHead + 1) % KEY_BUFFER_SIZE;
        s_bufferCount--;
    }

    s_keyBuffer[s_bufferTail] = key;
    s_bufferTail = (s_bufferTail + 1) % KEY_BUFFER_SIZE;
    s_bufferCount++;
}
```

**Buffer Read** (`components/cdc_hal/src/TCA9535Keypad.cpp:352-365`):
```cpp
Key TCA9535Keypad::bufferGetKey() {
    if (s_bufferCount == 0) return Key::KEY_NONE;

    Key key = s_keyBuffer[s_bufferHead];
    s_bufferHead = (s_bufferHead + 1) % KEY_BUFFER_SIZE;
    s_bufferCount--;
    return key;
}
```

**Keypad Task** (`components/cdc_hal/src/TCA9535Keypad.cpp:200-250`):
```cpp
static void taskFunc(void* arg) {
    TCA9535Keypad* keypad = static_cast<TCA9535Keypad*>(arg);
    // ...
    while (true) {
        // Read keypad state
        uint16_t raw = keypad->readInputs();
        Key key = rawToKey(raw);

        if (key != Key::KEY_NONE) {
            keypad->bufferAddKey(key);  // <-- WRITE from task
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_TIMEOUT_MS));
    }
}
```

**Main Loop Access** (`main/main.cpp:241-250`):
```cpp
while (true) {
    // ...
    cdc::ui::ui_process(nowMs);  // <-- Calls keypad->getNextKey() which reads buffer
    // ...
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

The keypad task (priority 5) and main task (default priority) access the same buffer without synchronization.

## Recommended Fix

1. **Add a mutex for buffer protection**:
```cpp
// In TCA9535Keypad.cpp
class TCA9535Keypad : public IKeypad {
private:
    Key s_keyBuffer[KEY_BUFFER_SIZE];
    uint8_t s_bufferHead = 0;
    uint8_t s_bufferTail = 0;
    uint8_t s_bufferCount = 0;
    SemaphoreHandle_t s_bufferMutex;  // Add this

    // ...
};

bool TCA9535Keypad::init() {
    s_bufferMutex = xSemaphoreCreateMutex();
    // ...
}

void TCA9535Keypad::bufferAddKey(Key key) {
    xSemaphoreTake(s_bufferMutex, portMAX_DELAY);
    if (s_bufferCount >= KEY_BUFFER_SIZE) {
        s_bufferHead = (s_bufferHead + 1) % KEY_BUFFER_SIZE;
        s_bufferCount--;
    }
    s_keyBuffer[s_bufferTail] = key;
    s_bufferTail = (s_bufferTail + 1) % KEY_BUFFER_SIZE;
    s_bufferCount++;
    xSemaphoreGive(s_bufferMutex);
}

Key TCA9535Keypad::bufferGetKey() {
    xSemaphoreTake(s_bufferMutex, portMAX_DELAY);
    if (s_bufferCount == 0) {
        xSemaphoreGive(s_bufferMutex);
        return Key::KEY_NONE;
    }
    Key key = s_keyBuffer[s_bufferHead];
    s_bufferHead = (s_bufferHead + 1) % KEY_BUFFER_SIZE;
    s_bufferCount--;
    xSemaphoreGive(s_bufferMutex);
    return key;
}
```

2. **Alternative: Use atomic indices** (lock-free ring buffer):
```cpp
#include <atomic>

class TCA9535Keypad : public IKeypad {
private:
    std::atomic<uint8_t> s_bufferHead{0};
    std::atomic<uint8_t> s_bufferTail{0};
    std::atomic<uint8_t> s_bufferCount{0};
    Key s_keyBuffer[KEY_BUFFER_SIZE];

    void bufferAddKey(Key key) {
        uint8_t tail = s_bufferTail.load();
        uint8_t head = s_bufferHead.load();
        // ... same logic but with atomic loads/stores
        s_bufferTail.store((tail + 1) % KEY_BUFFER_SIZE);
        s_bufferCount.fetch_add(1);
    }
};
```

3. **Use FreeRTOS queue** instead of ring buffer:
```cpp
// Replace ring buffer with queue
static QueueHandle_t s_keyQueue = nullptr;

bool TCA9535Keypad::init() {
    s_keyQueue = xQueueCreate(KEY_BUFFER_SIZE, sizeof(Key));
}

void TCA9535Keypad::bufferAddKey(Key key) {
    xQueueSendFromISR(s_keyQueue, &key, nullptr);
}

Key TCA9535Keypad::bufferGetKey() {
    Key key;
    if (xQueueReceive(s_keyQueue, &key, 0) == pdTRUE) {
        return key;
    }
    return Key::KEY_NONE;
}
```

## References

- FreeRTOS: Mutex usage for shared data structures
- ESP32: Task priorities and context switching
- Ring buffer patterns: Lock-free vs. mutex-protected
