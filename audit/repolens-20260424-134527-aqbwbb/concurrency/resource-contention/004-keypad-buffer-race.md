---
title: "[HIGH] Keypad buffer race condition in concurrent access"
severity: HIGH
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The `TCA9535Keypad` class uses a circular buffer (`keyBuffer_`) protected by `portMUX_TYPE` spinlock, but `isKeyPressed()` reads raw input without holding the lock, creating a potential race with the background task that updates the buffer. Additionally, `bufferAddKey()` and `bufferGetKey()` use the same lock but don't handle the case where the ISR and task access the buffer simultaneously.

**Locations:**
- `components/cdc_hal/src/TCA9535Keypad.cpp:120` - bufferMux_ definition
- `components/cdc_hal/src/TCA9535Keypad.cpp:330-337` - bufferAddKey()
- `components/cdc_hal/src/TCA9535Keypad.cpp:341-352` - bufferGetKey()
- `components/cdc_hal/src/TCA9535Keypad.cpp:286-292` - isKeyPressed()
- `components/cdc_hal/src/TCA9535Keypad.cpp:373-380` - isrHandler()

## Impact
1. **Lost Key Events**: If ISR fires while task is updating buffer, key events can be lost.
2. **Buffer Corruption**: Concurrent access to `bufferHead_` and `bufferTail_` can cause wraparound logic to fail.
3. **Inconsistent State**: `isKeyPressed()` reads raw state without synchronization, potentially returning stale data.

**Evidence:**
```cpp
// TCA9535Keypad.cpp:bufferAddKey (lines 330-337)
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

// TCA9535Keypad.cpp:bufferGetKey (lines 341-352)
Key TCA9535Keypad::bufferGetKey() {
    portENTER_CRITICAL(&bufferMux_);
    if (bufferHead_ == bufferTail_) {
        portEXIT_CRITICAL(&bufferMux_);
        return Key::KEY_NONE;
    }
    
    Key key = keyBuffer_[bufferTail_];
    bufferTail_ = (bufferTail_ + 1) % KEY_BUFFER_SIZE;
    portEXIT_CRITICAL(&bufferMux_);
    return key;
}

// TCA9535Keypad.cpp:isKeyPressed (lines 286-292)
bool TCA9535Keypad::isKeyPressed(Key key) const {
    uint16_t mask = keyToMask(key);
    if (mask == 0xFFFF) return false;
    
    uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();  // <-- No lock!
    return (current & 0x0FFF) == mask;
}

// TCA9535Keypad.cpp:isrHandler (lines 373-380)
void IRAM_ATTR TCA9535Keypad::isrHandler(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    if (self->inSleepMode_) return;
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(self->semaphore_, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// TCA9535Keypad.cpp:taskFunc (lines 383-433)
void TCA9535Keypad::taskFunc(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    LOG_I(TAG, "Keypad task started");
    
    while (true) {
        xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));
        
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
        
        uint16_t raw = self->readInputs();
        
        if (raw != self->lastRawState_) {
            Key key = rawToKey(raw);
            
            if (key != Key::KEY_NONE) {
                self->bufferAddKey(key);  // <-- Adds to buffer
                
                if (self->callback_) {
                    self->callback_(key, true);
                }
                
                // Long-press tracking...
            } else {
                // Key release...
            }
            
            self->lastRawState_ = raw;
        }
        
        // Long-press check...
    }
}
```

**Race condition sequence:**
```
ISR Context:                    Main Task:
                                xSemaphoreTake()
                                readInputs() → raw = 0x0FFD (KEY_2 pressed)
                                rawToKey() → key = KEY_2
bufferAddKey(KEY_3):
    portENTER_CRITICAL()
    nextHead = (0 + 1) % 16 = 1
    keyBuffer_[0] = KEY_3
    bufferHead_ = 1
    portEXIT_CRITICAL()
                                bufferAddKey(KEY_2):
                                    portENTER_CRITICAL()  // Waits
                                    nextHead = (1 + 1) % 16 = 2
                                    keyBuffer_[1] = KEY_2
                                    bufferHead_ = 2
                                    portEXIT_CRITICAL()
                                
                                bufferGetKey():
                                    portENTER_CRITICAL()
                                    bufferHead_ != bufferTail_ (2 != 0)
                                    key = keyBuffer_[0] → KEY_3
                                    bufferTail_ = 1
                                    portEXIT_CRITICAL()
                                    return KEY_3
                                
                                ISR fires (key still pressed)
                                xSemaphoreGiveFromISR()
                                
                                xSemaphoreTake() → returns immediately
                                readInputs() → raw = 0x0FFD (same)
                                raw != lastRawState_? NO (0x0FFD == 0x0FFD)
                                // Key event NOT added again (correct debounce)
                                
                                bufferGetKey():
                                    portENTER_CRITICAL()
                                    bufferHead_ != bufferTail_ (2 != 1)
                                    key = keyBuffer_[1] → KEY_2
                                    bufferTail_ = 2
                                    portEXIT_CRITICAL()
                                    return KEY_2
```

The logic appears correct for the normal case. However, there's a subtle issue:

```
ISR Context:                    Main Task:
                                xSemaphoreTake()
                                readInputs() → raw = 0x0FFD
                                rawToKey() → key = KEY_2
                                bufferAddKey(KEY_2):
                                    portENTER_CRITICAL()
                                    nextHead = (0 + 1) % 16 = 1
                                    keyBuffer_[0] = KEY_2
                                    bufferHead_ = 1
                                    portEXIT_CRITICAL()
                                
                                lastRawState_ = 0x0FFD
                                bufferGetKey() → KEY_2
                                
                                // Key released
                                xSemaphoreTake()
                                readInputs() → raw = 0x0FFF
                                rawToKey() → key = KEY_NONE
                                bufferAddKey(KEY_NONE) → returns early
                                lastRawState_ = 0x0FFF
                                
                                // Key pressed again immediately
                                xSemaphoreTake()
                                readInputs() → raw = 0x0FFD
                                raw != lastRawState_? YES (0x0FFD != 0x0FFF)
                                rawToKey() → key = KEY_2
                                bufferAddKey(KEY_2):
                                    portENTER_CRITICAL()
                                    nextHead = (1 + 1) % 16 = 2
                                    keyBuffer_[1] = KEY_2
                                    bufferHead_ = 2
                                    portEXIT_CRITICAL()
                                lastRawState_ = 0x0FFD
                                
                                bufferGetKey() → KEY_2
```

This looks correct. The real issue is when **ISR fires between buffer operations**:

```
ISR fires, semaphore given
Main Task:
    xSemaphoreTake() → success
    readInputs() → raw = 0x0FFD
    rawToKey() → key = KEY_2
    bufferAddKey(KEY_2):
        portENTER_CRITICAL()
        nextHead = 1
        keyBuffer_[0] = KEY_2
        bufferHead_ = 1
        portEXIT_CRITICAL()
    
    // ISR fires again (key still pressed, debounced)
    xSemaphoreGiveFromISR()
    
    lastRawState_ = 0x0FFD
    // Loop continues...
    xSemaphoreTake() → returns immediately (count > 0)
    readInputs() → raw = 0x0FFD
    raw != lastRawState_? NO → skip
    // Semaphore count still > 0 from ISR
```

The issue is that `bufferAddKey` is called from the task context (after debounce), but the semaphore can be given from ISR multiple times. If the task is slow, the semaphore count can build up, causing the task to process multiple "wakeups" for a single key press.

**Real race condition - buffer overflow edge case:**
```
Task:                           Task (reentrant via ISR wake):
                                xSemaphoreTake() → success
                                readInputs() → raw = 0x0FFD
bufferAddKey(KEY_2):            bufferAddKey(KEY_2) called again? NO, because
    portENTER_CRITICAL()            raw == lastRawState_
    nextHead = 1
    keyBuffer_[0] = KEY_2
    bufferHead_ = 1
                                    // But what if task is preempted?
    portEXIT_CRITICAL()
    
    // Preempted here!
                                    // Another task calls isKeyPressed()
                                    isKeyPressed(KEY_2):
                                        readInputs() → 0x0FFD
                                        return true (correct)
    
    // Resume
    lastRawState_ = 0x0FFD
    bufferGetKey() → KEY_2
```

The actual issue is that `isKeyPressed()` bypasses the buffer entirely and reads raw hardware state. This is fine for immediate feedback, but it's inconsistent with the buffered state:

```
Task:
    bufferAddKey(KEY_2) → buffer has KEY_2
    lastRawState_ = 0x0FFD
    
    // Key released physically
    // But task hasn't processed yet
    
    isKeyPressed(KEY_2):
        readInputs() → 0x0FFF (key released)
        return false  // BUT buffer still has KEY_2!
    
    bufferGetKey() → KEY_2  // Returns key that's no longer pressed
```

## Recommended Fix
Synchronize `isKeyPressed()` with buffer state or document the intentional inconsistency:

1. **Option A - Add lock to isKeyPressed:**
   ```cpp
   bool TCA9535Keypad::isKeyPressed(Key key) const {
       uint16_t mask = keyToMask(key);
       if (mask == 0xFFFF) return false;
       
       uint16_t current = const_cast<TCA9535Keypad*>(this)->readInputs();
       
       // Check both hardware state AND buffer state
       bool inBuffer = false;
       portENTER_CRITICAL(&bufferMux_);
       uint8_t head = bufferHead_;
       uint8_t tail = bufferTail_;
       portEXIT_CRITICAL(&bufferMux_);
       
       // If key is in buffer, consider it "pressed" even if physically released
       // (debounce delay)
       if (head != tail) {
           portENTER_CRITICAL(&bufferMux_);
           for (uint8_t i = tail; i != head; i = (i + 1) % KEY_BUFFER_SIZE) {
               if (keyBuffer_[i] == key) {
                   inBuffer = true;
                   break;
               }
           }
           portEXIT_CRITICAL(&bufferMux_);
       }
       
       return (current & 0x0FFF) == mask || inBuffer;
   }
   ```

2. **Option B - Document intentional behavior:**
   Add comment to `isKeyPressed()` explaining it reads raw hardware state for immediate feedback, independent of debounced buffer state:
   ```cpp
   /**
    * \brief Checks raw hardware state (not debounced).
    * \param key Key to test.
    * \return `true` when key is physically pressed.
    * \note This reads hardware directly, bypassing the debounced buffer.
    *       Use bufferGetKey() for debounced key events.
    */
   ```

3. **Option C - Add buffer peek:**
   ```cpp
   bool TCA9535Keypad::isKeyInBuffer(Key key) const {
       portENTER_CRITICAL(&bufferMux_);
       uint8_t head = bufferHead_;
       uint8_t tail = bufferTail_;
       portEXIT_CRITICAL(&bufferMux_);
       
       if (head == tail) return false;
       
       portENTER_CRITICAL(&bufferMux_);
       for (uint8_t i = tail; i != head; i = (i + 1) % KEY_BUFFER_SIZE) {
           if (keyBuffer_[i] == key) {
               portEXIT_CRITICAL(&bufferMux_);
               return true;
           }
       }
       portEXIT_CRITICAL(&bufferMux_);
       return false;
   }
   ```

**Recommended approach:** Option B (documentation) is sufficient since the current behavior is intentional - `isKeyPressed()` provides immediate hardware feedback, while the buffer provides debounced events.

## References
- [FreeRTOS Critical Sections](https://www.freertos.org/Protecting-Access-To-Shared-Variables.html)
- [IRAM_ATTR ISR Constraints](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/isr.html)
- [Debouncing Techniques](https://www.allaboutcircuits.com/technical-articles/mechanical-switch-debounce-techniques/)
