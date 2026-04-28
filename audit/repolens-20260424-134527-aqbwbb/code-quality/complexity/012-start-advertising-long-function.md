---
title: "[MEDIUM] Long function startAdvertising with multiple responsibilities"
severity: MEDIUM
domain: code-quality
lens: cyclomatic-complexity
labels:
  - "complexity:high"
---

## Summary
The `startAdvertising` function in `components/cdc_hal/src/BluetoothController.cpp:704` is approximately 90 lines long and handles multiple responsibilities: stopping existing advertising, configuring primary fields, building scan response with UUIDs, adding manufacturer data, and starting the advertisement.

**Evidence:**
- File: `components/cdc_hal/src/BluetoothController.cpp`
- Lines: 704-791 (87 lines)
- Branching paths: 6+ (if/else for advertising state, UUID separation, manufacturer data check, error handling)
- Local variables: 15+ (advParams, fields, rsp, uuid16s, uuid128s, mfgAdvBuf, rc, etc.)

## Impact
- **Maintainability**: Hard to understand the complete flow at a glance
- **Testing**: Multiple code paths require separate test scenarios
- **Reusability**: Cannot easily reuse parts (e.g., just setting manufacturer data)
- **Stack usage**: Multiple local structs and arrays consume significant stack space

## Evidence
```cpp
void BluetoothController::startAdvertising() {
    if (!enabled_ || !synced_) return;

    // Stop current advertising if running
    if (advertising_) { ... }

    struct ble_gap_adv_params advParams = {};
    // ... init advParams

    struct ble_hs_adv_fields fields = {};
    // ... configure primary fields
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ... }

    // Scan response: registered service UUIDs + manufacturer data
    if (advUuidCount_ > 0 || mfgDataSet_) {
        struct ble_hs_adv_fields rsp = {};
        ble_uuid16_t uuid16s[MAX_ADV_UUIDS];
        ble_uuid128_t uuid128s[MAX_ADV_UUIDS];
        // ... separate and configure UUIDs
        // ... configure manufacturer data
        rc = ble_gap_adv_rsp_set_fields(&rsp);
    }

    rc = ble_gap_adv_start(...);
    if (rc != 0) { ... }

    advertising_ = true;
}
```

## Recommended Fix
Decompose into focused helper methods:

1. **Stop advertising helper:**
   ```cpp
   void BluetoothController::stopAdvertisingIfNeeded() {
       if (advertising_) {
           ble_gap_adv_stop();
           advertising_ = false;
       }
   }
   ```

2. **Primary fields configuration:**
   ```cpp
   bool BluetoothController::configureAdvFields(struct ble_hs_adv_fields* fields) {
       fields->flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
       fields->tx_pwr_lvl_is_present = 1;
       fields->tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
       fields->name = (uint8_t*)deviceName_;
       fields->name_len = strlen(deviceName_);
       fields->name_is_complete = 1;
       return ble_gap_adv_set_fields(fields) == 0;
   }
   ```

3. **Scan response builder:**
   ```cpp
   bool BluetoothController::buildScanResponse(struct ble_hs_adv_fields* rsp) {
       // Separate UUIDs
       // Configure manufacturer data
       // Return success
   }
   ```

4. **Refactored main function:**
   ```cpp
   void BluetoothController::startAdvertising() {
       if (!enabled_ || !synced_) return;
       
       stopAdvertisingIfNeeded();
       
       struct ble_gap_adv_params advParams = {};
       // ... simple init
       
       struct ble_hs_adv_fields fields = {};
       if (!configureAdvFields(&fields)) return;
       
       if (advUuidCount_ > 0 || mfgDataSet_) {
           struct ble_hs_adv_fields rsp = {};
           if (!buildScanResponse(&rsp)) {
               LOG_W(TAG, "Scan response setup failed");
           }
       }
       
       // Start advertising...
   }
   ```

**Estimated effort**: 50-60 minutes

## References
- Martin, R. C. (2008). Clean Code. Chapter 6: Single Responsibility Principle
- Fowler, M. (2018). Refactoring. "Extract Method" pattern
- ESP32 NimBLE stack limits: 31 bytes for advertising data
