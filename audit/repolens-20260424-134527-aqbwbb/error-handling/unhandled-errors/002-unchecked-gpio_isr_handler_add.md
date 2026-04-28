---
title: "[MEDIUM] Unchecked gpio_isr_handler_add return value in keypad initialization"
severity: MEDIUM
domain: hardware-abstraction
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp`, the `init()` function calls `gpio_isr_handler_add()` at line 238 but does not check its return value. If the ISR handler registration fails, the keypad will not receive interrupt notifications, leading to missed key presses.

## Impact
If `gpio_isr_handler_add()` fails (e.g., IRQ pin is already used, invalid pin configuration), the code continues as if everything is fine. The keypad service starts but interrupts don't work, causing:
- Silent failure of key press detection
- Only polling-based detection would work (but polling is a no-op: `poll() {}`)
- Very hard to debug since no error is logged

## Evidence
File: `components/cdc_hal/src/TCA9535Keypad.cpp:235-242`

```cpp
// Install ISR service (may already be installed by another driver)
esp_err_t err = gpio_install_isr_service(0);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;
}
gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);  // <-- UNCHECKED RETURN VALUE

state_ = core::ServiceState::INITIALIZED;
LOG_I(TAG, "TCA9535 keypad initialized (IRQ=GPIO%d)", EXP_IRQ_PIN);
return true;
```

## Recommended Fix
Check the return value of `gpio_isr_handler_add()` and handle failure:

```cpp
gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);
if (gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this) != ESP_OK) {
    LOG_E(TAG, "Failed to add ISR handler for keypad IRQ");
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;
}
```

Or more idiomatically:
```cpp
esp_err_t err = gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);
if (err != ESP_OK) {
    LOG_E(TAG, "Failed to add ISR handler for keypad IRQ: %s", esp_err_to_name(err));
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## References
- ESP-IDF GPIO driver documentation: `gpio_isr_handler_add()` returns `esp_err_t`
- TCA9535 datasheet: Interrupt operation
