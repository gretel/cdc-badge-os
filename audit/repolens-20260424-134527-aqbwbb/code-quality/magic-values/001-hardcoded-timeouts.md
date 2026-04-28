---
title: "[MEDIUM] Hardcoded timeout values in FreeRTOS calls"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
Multiple hardcoded numeric timeout values are used directly in FreeRTOS API calls (`pdMS_TO_TICKS()`, `vTaskDelay()`, `xQueueSend()`, `xQueueReceive()`) without named constants or configuration. These values appear in core system components:

**Files affected:**
- `components/cdc_core/src/EventBus.cpp:97` - Queue timeout: `pdMS_TO_TICKS(10)`
- `components/serial_cmd/src/SerialCmd.cpp:468` - Reboot delay: `pdMS_TO_TICKS(100)`
- `components/cdc_hal/src/TCA9535Keypad.cpp:389,392,463` - Poll timeout: `POLL_TIMEOUT_MS` (50ms), Debounce: `DEBOUNCE_MS` (10ms)
- `components/cdc_hal/src/I2cBus.cpp:142,176` - I2C timeout: `I2C_TIMEOUT_MS` (100ms)
- `components/cdc_hal/src/libtropic_port_esp32.cpp:133` - Generic delay: `pdMS_TO_TICKS(ms)`

**Additional instances in CalEPD (80+ occurrences):**
- Various display-specific delays: 1ms, 2ms, 10ms, 100ms, 200ms, 300ms
- Power-up delays: 200ms, 500ms
- Busy wait timeouts: 2000000us

## Impact
- **Maintainability**: Changing timeout behavior requires searching through multiple files
- **Tuning difficulty**: Hardware-specific timing adjustments are scattered
- **Consistency**: Similar timeouts may have different values across modules
- **Documentation**: Rationale for specific values is not captured near the usage

## Evidence
```cpp
// components/cdc_core/src/EventBus.cpp:97
result = xQueueSend(static_cast<QueueHandle_t>(queue_),
                    &event, pdMS_TO_TICKS(10));  // Magic value: 10ms

// components/serial_cmd/src/SerialCmd.cpp:468
vTaskDelay(pdMS_TO_TICKS(100));  // Magic value: 100ms before reboot

// components/cdc_hal/src/TCA9535Keypad.cpp:389,392
xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));  // 50ms
vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));  // 10ms

// components/cdc_hal/src/I2cBus.cpp:142,176
i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));  // 100ms
```

Note: Some values are already extracted (e.g., `POLL_TIMEOUT_MS`, `DEBOUNCE_MS`, `I2C_TIMEOUT_MS` in TCA9535Keypad.cpp), but the base values (50, 10, 100) are still magic numbers without justification comments.

## Recommended Fix
1. **Create a centralized timeout configuration header** (e.g., `components/cdc_core/include/cdc_core/timeout_config.h`)
2. **Define named constants with justification comments**:
   ```cpp
   // Event queue timeout: 10ms balances responsiveness with ISR throughput
   static constexpr uint16_t EVENT_QUEUE_TIMEOUT_MS = 10;
   
   // Reboot delay: 100ms ensures serial output flush before reset
   static constexpr uint16_t REBOOT_DELAY_MS = 100;
   
   // I2C timeout: 100ms covers typical register reads with small margin
   static constexpr uint16_t I2C_DEFAULT_TIMEOUT_MS = 100;
   ```
3. **Replace magic values** with the named constants across all affected files
4. **Add comments** explaining the rationale for each timeout value (hardware specs, protocol requirements, empirical testing)

## References
- [FreeRTOS documentation on task delays](https://www.freertos.org/RTOS-task-delay.html)
- [ESP-IDF I2C driver documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html)
