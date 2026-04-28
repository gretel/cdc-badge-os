---
title: "[LOW] HAL Hardware Abstraction Layer Classes Lack Unit Test Coverage"
severity: LOW
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The HAL components (`components/cdc_hal/src/`) provide hardware abstraction with minimal unit test coverage. Critical untested classes include:

**BQ25895Power.cpp** - Battery/charging management
- `getBatteryLevel()` - Battery percentage
- `isCharging()` - Charging status
- `getVoltage()` - Battery voltage

**BluetoothController.cpp** - BLE control
- `init()` - BLE initialization
- `startAdvertising()` - BLE advertisement
- `stopAdvertising()` - Stop advertisement
- `getConnectedCount()` - Connected devices

**WifiController.cpp** - WiFi control
- `init()` - WiFi initialization
- `connect()` - Network connection
- `disconnect()` - Network disconnect
- `scan()` - Network scan

**Rtc.cpp** - Real-time clock
- `getTime()` - Current time
- `setTime()` - Set time
- `getDate()` - Current date

**SleepController.cpp** - Sleep management
- `enterDeepSleep()` - Deep sleep entry
- `setWakeTime()` - Wake timer

**EpaperDisplay.cpp** - E-paper display
- `init()` - Display initialization
- `flush()` - Screen update
- `clear()` - Clear screen

## Impact
**Hardware Integration Risk:** HAL classes interface with physical hardware:
1. Power management (battery, charging) is untested
2. BLE/WiFi state transitions are unproven
3. RTC time/date handling is untested
4. Sleep/wake cycles are unverified
5. Display refresh modes (FULL/PARTIAL) are untested

## Evidence
Files in `components/cdc_hal/src/`:

File: `BQ25895Power.cpp` - Battery management
```cpp
// Typical implementation reads I2C registers from BQ25895
uint8_t BQ25895Power::getBatteryLevel() {
    uint16_t voltage = readRegister(BQ_REG_VBUS);
    // Calculate percentage from voltage
    return calculatePercentage(voltage);
}
```

File: `BluetoothController.cpp` - BLE control
```cpp
void BluetoothController::startAdvertising() {
    ble_gap_adv_params_t params = {...};
    esp_ble_gap_start_advertising(&params);
}
```

File: `Rtc.cpp` - RTC handling
```cpp
void Rtc::setTime(uint32_t timestamp) {
    struct timeval tv = {timestamp, 0};
    settimeofday(&tv, nullptr);
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l -i "bq25895\|bluetooth\|wifi\|rtc\|epaper" {} \;
# Returns nothing - no HAL tests exist
```

## Recommended Fix
Create `test/test_hal/` with separate test files for each HAL component:

1. **BQ25895Power tests:**
   - Test `getBatteryLevel()` with known voltage
   - Test `isCharging()` with charging flag
   - Test voltage-to-percentage conversion

2. **BluetoothController tests:**
   - Test `startAdvertising()` sets params
   - Test `stopAdvertising()` clears advertising
   - Test `getConnectedCount()` returns count

3. **WifiController tests:**
   - Test `connect()` with SSID/password
   - Test `disconnect()` stops connection
   - Test `scan()` returns networks

4. **Rtc tests:**
   - Test `setTime()` sets system time
   - Test `getTime()` returns correct time
   - Test `getDate()` formats date correctly

5. **SleepController tests:**
   - Test `enterDeepSleep()` configures wake source
   - Test `setWakeTime()` sets RTC alarm

6. **EpaperDisplay tests:**
   - Test `flush(FULL)` uses full refresh
   - Test `flush(PARTIAL)` uses partial refresh
   - Test `clear()` fills with white

Example test:
```cpp
void test_bq25895_battery_percentage() {
    auto& power = BQ25895Power::instance();
    // Simulate 3.7V battery (50%)
    power.setVoltage(3700);
    
    uint8_t pct = power.getBatteryLevel();
    TEST_ASSERT_EQUAL(50, pct);
}

void test_bq25895_charging_status() {
    auto& power = BQ25895Power::instance();
    power.setChargingFlag(true);
    
    TEST_ASSERT_TRUE(power.isCharging());
}

void test_rtc_set_time() {
    auto& rtc = Rtc::instance();
    uint32_t timestamp = 1650000000;  // Specific date
    
    rtc.setTime(timestamp);
    uint32_t retrieved = rtc.getTime();
    
    TEST_ASSERT_EQUAL(timestamp, retrieved);
}

void test_epaper_flush_full() {
    auto& display = EpaperDisplay::instance();
    display.init();
    
    display.flush(RefreshMode::FULL);
    TEST_ASSERT_TRUE(display.fullRefreshCalled);
    
    display.flush(RefreshMode::PARTIAL);
    TEST_ASSERT_TRUE(display.partialRefreshCalled);
}
```

## References
- File: `components/cdc_hal/include/cdc_hal/IEpPaperDisplay.h` - Display interface
- File: `components/cdc_hal/include/cdc_hal/IPowerManager.h` - Power interface
- File: `components/cdc_hal/include/cdc_hal/IBluetoothController.h` - BLE interface
- File: `components/cdc_hal/include/cdc_hal/IWifiController.h` - WiFi interface
