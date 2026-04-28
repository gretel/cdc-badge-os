---
title: "[MEDIUM] Blocking I2C reads in keypad polling loop"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "i2c"
  - "keypad"
  - "tca9535"
---

## Summary

The TCA9535 keypad implementation in `components/cdc_hal/src/TCA9535Keypad.cpp` performs blocking I2C reads in the polling loop, which can delay keypress detection and cause missed inputs during I2C bus contention.

**Affected file:**
- `components/cdc_hal/src/TCA9535Keypad.cpp` (lines 271-278, 387-398)

**Evidence:**
```cpp
// components/cdc_hal/src/TCA9535Keypad.cpp:271-278
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t lo = 0xFF, hi = 0xFF;
    if (bus_->readReg(device_, REG_INPUT_0, &lo, 1) != ESP_OK) return 0xFFFF;  // Blocks!
    if (bus_->readReg(device_, REG_INPUT_1, &hi, 1) != ESP_OK) return 0xFFFF;  // Blocks!

    return (uint16_t)((hi << 8) | lo);
}
```

```cpp
// components/cdc_hal/src/TCA9535Keypad.cpp:387-398 (taskFunc)
void TCA9535Keypad::taskFunc(void* arg) {
    auto* self = static_cast<TCA9535Keypad*>(arg);
    LOG_I(TAG, "Keypad task started");

    while (true) {
        // Wait for IRQ or timeout (poll fallback)
        xSemaphoreTake(self->semaphore_, pdMS_TO_TICKS(POLL_TIMEOUT_MS));

        // Small debounce delay
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        // Read current state - BLOCKING I2C calls!
        uint16_t raw = self->readInputs();  // 2 blocking I2C reads

        if (raw != self->lastRawState_) {
            Key key = rawToKey(raw);
            // ...
        }
    }
}
```

**Blocking I2C operations:**
- `bus_->readReg(device_, REG_INPUT_0, &lo, 1)`: ~1-5ms
- `bus_->readReg(device_, REG_INPUT_1, &hi, 1)`: ~1-5ms
- Total per poll: ~2-10ms

## Impact

1. **Delayed keypress detection**: During I2C reads (2-10ms), the keypad task cannot process other events.

2. **Missed rapid keypresses**: If user types quickly, keypresses during I2C reads may be missed or delayed.

3. **I2C bus contention**: If other I2C devices (RTC, sensors) use the bus simultaneously, keypad reads are delayed.

4. **Debounce accuracy**: Blocking I2C affects debounce timing accuracy.

5. **Long-press detection**: Long-press timing may be inaccurate if I2C is blocked.

## Evidence

**I2C timing:**
- Standard mode (100kHz): ~1ms per byte
- With overhead: ~1-5ms per `readReg()` call
- Two calls per poll: ~2-10ms total

**Polling frequency:**
- `POLL_TIMEOUT_MS = 50`: Polls every 50ms (or on IRQ)
- `DEBOUNCE_MS = 10`: Additional 10ms delay after IRQ

**Blocking pattern:**
```
Keypad task:    xSemaphoreTake()  // Wait for IRQ
Keypad task:    vTaskDelay(10ms)  // Debounce
Keypad task:    bus_->readReg()   // Blocks 1-5ms (I2C)
Keypad task:    bus_->readReg()   // Blocks 1-5ms (I2C)
Keypad task:    process key...
RTC task:       wait for I2C...   // Blocked!
```

**I2C bus sharing:**
- TCA9535 keypad (0x20)
- RTC (likely PCF8563 or similar)
- TROPIC01 uses SPI, not I2C

## Recommended Fix

1. **Use non-blocking I2C with timeout** (1 hour):
```cpp
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t lo = 0xFF, hi = 0xFF;
    
    // Use short timeout to prevent long blocking
    esp_err_t err1 = bus_->readReg(device_, REG_INPUT_0, &lo, 1);
    if (err1 != ESP_OK) return 0xFFFF;
    
    err1 = bus_->readReg(device_, REG_INPUT_1, &hi, 1);
    if (err1 != ESP_OK) return 0xFFFF;

    return (uint16_t)((hi << 8) | lo);
}
```

2. **Batch I2C reads into single transaction** (30 min):
```cpp
// Read both registers in one I2C transaction
uint16_t TCA9535Keypad::readInputs() {
    if (!device_) return 0xFFFF;

    uint8_t data[2] = {0xFF, 0xFF};
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x20 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG_INPUT_0, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x20 << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    
    if (err != ESP_OK) return 0xFFFF;
    return (uint16_t)((data[1] << 8) | data[0]);
}
```

3. **Use DMA for I2C reads** (2 hours):
```cpp
// Configure I2C for DMA
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    // ...
    .master.clk_speed = 100000,
    .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL  // Enable DMA
};

// Use i2c_master_transmit_receive for non-blocking
```

4. **Prioritize keypad I2C with higher frequency** (30 min):
```cpp
// Use faster I2C for keypad (400kHz)
static constexpr uint3_t KEYPAD_I2C_FREQ = 400000;  // Fast mode

// Configure TCA9535-specific I2C timing
i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
i2c_set_timeout(I2C_NUM_0, 100);  // Short timeout
```

5. **Add I2C read caching** (30 min):
```cpp
// Cache last read, only re-read on IRQ
static uint16_t cachedState = 0xFFFF;
static uint32_t lastReadMs = 0;

uint16_t TCA9535Keypad::readInputs() {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Return cached if recently read
    if (now - lastReadMs < 10) {
        return cachedState;
    }
    
    cachedState = readActualInputs();
    lastReadMs = now;
    return cachedState;
}
```

**Estimated effort**: 1-2 hours for I2C batching and caching

## References

- [TCA9535 datasheet](https://www.ti.com/lit/ds/symlink/tca9535.pdf)
- [ESP-IDF I2C driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html)
- I2C bus timing: https://www.nxp.com/docs/en/user-guide/UM10204.pdf
- Keypad debouncing: https://www.allaboutcircuits.com/technical-articles/understanding-mechanical-debounce-and-simple-solutions/

</content>