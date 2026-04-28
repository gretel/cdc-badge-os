---
title: "[MEDIUM] gpio_isr_handler_add() return values ignored in BQ25895 and TCA9535"
severity: MEDIUM
domain: cdc_hal
lens: error-handling
labels:
  - "GPIO"
  - "ISR"
  - "interrupts"
  - "BQ25895"
  - "TCA9535"
---

## Summary

In two hardware drivers, the return value of `gpio_isr_handler_add()` is ignored, potentially masking interrupt handler registration failures:

1. **BQ25895 Power Manager** - `components/cdc_hal/src/BQ25895Power.cpp:278`
2. **TCA9535 Keypad** - `components/cdc_hal/src/TCA9535Keypad.cpp:238`

While `gpio_install_isr_service()` is checked, the subsequent `gpio_isr_handler_add()` call is not verified.

## Impact

- If `gpio_isr_handler_add()` fails, the interrupt handler won't be registered
- Charger IRQ (BQ25895) or keypad IRQ (TCA9535) will not trigger
- Device may appear to work but miss critical events (charge status changes, key presses)
- Debugging becomes difficult as the symptom (no IRQ) doesn't match the cause (handler not registered)

## Evidence

### BQ25895 Power Manager (line 272-279)
```cpp
// Install ISR service (may already be installed by another driver)
esp_err_t err = gpio_install_isr_service(0);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    state_ = core::ServiceState::ERROR;
    return false;
}
gpio_isr_handler_add(CHG_IRQ_PIN, charger_isr, nullptr);  // <-- Return ignored!

// Read initial status (clears any stale IRQ flags)
readChargerStatus();
```

### TCA9535 Keypad (line 230-239)
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
gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);  // <-- Return ignored!

state_ = core::ServiceState::INITIALIZED;
LOG_I(TAG, "TCA9535 keypad initialized (IRQ=GPIO%d)", EXP_IRQ_PIN);
return true;
```

## Recommended Fix

Check the return value of `gpio_isr_handler_add()` and handle failures:

### BQ25895 Power Manager
```cpp
// Install ISR service (may already be installed by another driver)
esp_err_t err = gpio_install_isr_service(0);
if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOG_E(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
    state_ = core::ServiceState::ERROR;
    return false;
}
err = gpio_isr_handler_add(CHG_IRQ_PIN, charger_isr, nullptr);
if (err != ESP_OK) {
    LOG_E(TAG, "Failed to add charger ISR: %s", esp_err_to_name(err));
    state_ = core::ServiceState::ERROR;
    return false;
}
```

### TCA9535 Keypad
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
err = gpio_isr_handler_add(EXP_IRQ_PIN, isrHandler, this);
if (err != ESP_OK) {
    LOG_E(TAG, "Failed to add keypad ISR: %s", esp_err_to_name(err));
    vSemaphoreDelete(semaphore_);
    semaphore_ = nullptr;
    state_ = core::ServiceState::ERROR;
    return false;
}
```

## References

- [ESP-IDF GPIO Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- `gpio_isr_handler_add()` returns `ESP_OK` on success, `ESP_ERR_INVALID_ARG` if handler is NULL, or `ESP_FAIL` if ISR service not installed
