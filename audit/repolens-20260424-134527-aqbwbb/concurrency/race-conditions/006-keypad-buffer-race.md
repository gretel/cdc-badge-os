---
title: "[LOW] Keypad Buffer Race - Missing Critical Section in isKeyPressed()"
severity: LOW
domain: concurrency
lens: race-conditions
labels:
  - "audit:concurrency/race-conditions"
---

## Summary

The `TCA9535Keypad` class in `components/cdc_hal/src/TCA9535Keypad.cpp` uses a critical section (`portMUX_TYPE`) to protect the key buffer, but the `isKeyPressed()` method reads the raw keypad state without synchronization. This can cause inconsistent state if the ISR/task modifies the buffer while `isKeyPressed()` is checking.

**Evidence** - `isKeyPressed()` (lines 286-292):
```cpp
bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;

    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();  // No lock
    return (current & 0x0FFF) == mask;
}
```

**Evidence** - Buffer operations with proper locking (lines 304-310, 343-352):
```cpp
bool TCA9535Keypad::hasKey() const {
    portENTER_CRITICAL(&bufferMux_);
    bool hasKey = bufferHead_ != bufferTail_;
    portEXIT_CRITICAL(&bufferMux_);
    return hasKey;
}

void TCA9535Keypad::bufferAddKey(Key key) {
    if (key == Key::KEY_NONE) return;

    portENTER_CRITICAL(&bufferMux_);
    uint8_t nextHead = (bufferHead_ + 1) % KEY_BUFFER_SIZE;
    if (nextHead != bufferTail_) {
        keyBuffer_[bufferHead_] = key;
        bufferHead_ = nextHead;
    }
    portEXIT_CRITICAL(&bufferMux_);
}
```

**Evidence** - Task that modifies buffer (lines 395-402):
```cpp
// In taskFunc()
if (key != Key::KEY_NONE) {
    self->bufferAddKey(key);  // Adds to buffer

    if (self->callback_) {
        self->callback_(key, true);  // Callback might call isKeyPressed()
    }
    // ...
}
```

**The Race**:
1. Task calls `isKeyPressed(KEY_1)`
2. `readInputs()` reads raw state (e.g., `0x1234`)
3. ISR fires, task adds key to buffer
4. `isKeyPressed()` compares stale `0x1234` with expected mask
5. Result might be wrong if state changed between read and compare

While this is less critical than data corruption (it's just a snapshot read), it can cause inconsistent behavior in UI code that checks `isKeyPressed()` after getting a key from `getNextKey()`.

## Impact

**Inconsistent State**: UI code might check `isKeyPressed(KEY_1)` expecting it to return true (because the key was just pressed), but the check happens after the key was already processed and the task moved on.

**UI Glitches**: If the UI checks `isKeyPressed()` in a loop or during animation, it might see flickering or inconsistent results.

## Recommended Fix

The fix depends on the intended semantics:

**Option 1**: Add documentation that `isKeyPressed()` returns a snapshot and callers should use `getNextKey()` for reliable key detection:

```cpp
/**
 * \brief Checks whether specified key is currently pressed.
 * \param key Key to test.
 * \return `true` when key is active.
 * \note This returns a snapshot of the current state. For reliable
 *       key detection, use getNextKey() which reads from the buffered queue.
 */
bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;

    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    return (current & 0x0FFF) == mask;
}
```

**Option 2**: If consistent state is needed, add the buffer to the check:

```cpp
bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;

    // Check both raw state and buffer for consistent result
    portENTER_CRITICAL(&bufferMux_);
    bool inBuffer = false;
    for (uint8_t i = bufferTail_; i != bufferHead_; ) {
        if (keyBuffer_[i] == key) {
            inBuffer = true;
            break;
        }
        i = (i + 1) % KEY_BUFFER_SIZE;
    }
    portEXIT_CRITICAL(&bufferMux_);

    if (inBuffer) return true;

    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
    return (current & 0x0FFF) == mask;
}
```

## References

- FreeRTOS critical sections: https://www.freertos.org/PortMux.html
- ESP-IDF GPIO interrupt handling: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html#interrupt-handler
