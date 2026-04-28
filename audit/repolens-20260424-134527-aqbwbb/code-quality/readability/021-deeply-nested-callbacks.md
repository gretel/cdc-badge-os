---
title: "[021] [LOW] Deeply nested callbacks in keypad task"
severity: LOW
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The keypad task function uses deeply nested callbacks that make the control flow harder to follow.

## Impact
- Multiple levels of indentation reduce readability
- Harder to see the overall flow of the task
- Requires scrolling to understand all the logic

## Evidence
**File: `components/cdc_hal/src/TCA9535Keypad.cpp:376-430`**
```cpp
void TCA9535Keypad::taskFunc(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    LOG_I(TAG, "Keypad task started");

    while (true) {
        // Wait for IRQ or timeout (poll fallback)
        xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));

        // Small debounce delay
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        // Read current state
        uint16_t raw = self->readInputs();

        if (raw != self->lastRawState_) {
            Key key = rawToKey(raw);

            // Key press detection
            if (key != Key::KEY_NONE) {
                self->bufferAddKey(key);

                if (self->callback_) {
                    self->callback_(key, true);
                }

                // Start long-press tracking
                if (self->longPressEnabled_) {
                    self->pressedKey_ = key;
                    self->pressStartTime_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    self->longPressFired_ = false;
                }
            } else {
                // Key release
                if (self->callback_ && self->pressedKey_ != Key::KEY_NONE) {
                    self->callback_(self->pressedKey_, false);
                }
                self->pressedKey_ = Key::KEY_NONE;
            }

            self->lastRawState_ = raw;
        }

        // Long-press check
        if (self->longPressEnabled_ && self->pressedKey_ != Key::KEY_NONE && !self->longPressFired_) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - self->pressStartTime_ >= self->longPressThresholdMs_) {
                self->longPressFired_ = true;
                if (self->longPressCallback_) {
                    self->longPressCallback_(self->pressedKey_);
                }
            }
        }
    }
}
```

The function has 4-5 levels of nesting in some places.

## Recommended Fix
Use early returns (guard clauses) to flatten the logic:

```cpp
void TCA9535Keypad::taskFunc(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    LOG_I(TAG, "Keypad task started");

    while (true) {
        xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        uint16_t raw = self->readInputs();
        self->handleStateChange(raw);
        self->checkLongPress();
    }
}

// Extracted helper methods:
void TCA9535Keypad::handleStateChange(uint16_t raw) {
    if (raw == lastRawState_) return;  // Early return for no change

    Key key = rawToKey(raw);
    bool isPress = key != Key::KEY_NONE;

    if (isPress) {
        handleKeyPress(key);
    } else {
        handleKeyRelease();
    }

    lastRawState_ = raw;
}

void TCA9535Keypad::handleKeyPress(Key key) {
    bufferAddKey(key);

    if (callback_) {
        callback_(key, true);
    }

    if (!longPressEnabled_) return;

    pressedKey_ = key;
    pressStartTime_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
    longPressFired_ = false;
}

void TCA9535Keypad::handleKeyRelease() {
    if (!callback_ || pressedKey_ == Key::KEY_NONE) return;

    callback_(pressedKey_, false);
    pressedKey_ = Key::KEY_NONE;
}

void TCA9535Keypad::checkLongPress() {
    if (!longPressEnabled_ || pressedKey_ == Key::KEY_NONE || longPressFired_) return;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - pressStartTime_ < longPressThresholdMs_) return;

    longPressFired_ = true;
    if (longPressCallback_) {
        longPressCallback_(pressedKey_);
    }
}
```

## References
- Refactoring: "Replace Nested Conditional with Guard Clauses" - Martin Fowler
- Clean Code: "Functions should be small and have minimal nesting" - Robert C. Martin
