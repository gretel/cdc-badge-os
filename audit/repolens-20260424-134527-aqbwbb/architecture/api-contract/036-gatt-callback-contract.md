---
title: "[HIGH] GATT Callback Return Value Contract Ambiguity"
severity: HIGH
domain: API Contract Integrity
lens: callback-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
`IBluetoothController::GattWriteCallback` and `GattReadCallback` use `int` return types, but the contract for what values to return is undocumented. This creates ambiguity between different implementations and consumers of the API.

**Location**: `components/cdc_hal/include/cdc_hal/IBluetoothController.h:79-93`

## Impact
1. **Inconsistent behavior**: Different implementations may return different values for success/failure
2. **Protocol errors**: BLE stack may interpret wrong return values as errors, causing disconnects
3. **Debugging difficulty**: Hard to trace why a write/read is failing without documented contract

## Evidence

In `components/cdc_hal/include/cdc_hal/IBluetoothController.h:79-93`:
```cpp
/**
 * GATT write callback for characteristic writes
 */
using GattWriteCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                             const uint8_t* data, uint16_t len)>;

/**
 * GATT read callback for characteristic reads
 */
using GattReadCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                            uint8_t* buf, uint16_t* len)>;

struct GattCharacteristic {
    BleUuid uuid;
    uint8_t properties;
    uint8_t permissions;
    uint16_t* valueHandle;
    GattWriteCallback onWrite;
    GattReadCallback onRead;
};
```

**No documentation** on:
- What return value indicates success?
- What return value indicates failure?
- What specific values mean specific errors?
- Should it return ATT error codes (0x01-0xFF)?
- Should it return 0 for success, -1 for error?
- Should it return number of bytes processed?

In `mod_ble_serial/src/BleUartService.cpp:65-69`:
```cpp
chars[0].onWrite = [](uint16_t /*connHandle*/, uint16_t /*attrHandle*/,
                      const uint8_t* data, uint16_t len) -> int {
    BleUartService::instance().onRxData(data, len);
    return 0;  // <-- What does 0 mean? Success? Error bytes?
};
```

The implementation returns `0`, but there's no contract specifying that `0` means success.

## Recommended Fix

1. **Define return value contract** in interface documentation:
   ```cpp
   /**
    * \brief GATT write callback for characteristic writes.
    * \param connHandle Connection handle.
    * \param attrHandle Attribute handle being written.
    * \param data Write payload.
    * \param len Write length.
    * \return ATT status code (0x00 = success, 0x01-0xFF = error codes per BLE spec).
    * \note Return 0x00 (GATT_SUCCESS) for successful write.
    * \note Return 0x03 (WRITE_NOT_PERMITTED) for permission errors.
    * \note Return 0x04 (WRITE_REQ_TOO_LONG) if data exceeds characteristic size.
    */
   using GattWriteCallback = std::function<uint8_t(uint16_t connHandle, uint16_t attrHandle,
                                                    const uint8_t* data, uint16_t len)>;
   ```

2. **Define read callback contract**:
   ```cpp
   /**
    * \brief GATT read callback for characteristic reads.
    * \param connHandle Connection handle.
    * \param attrHandle Attribute handle being read.
    * \param buf Output buffer for read value.
    * \param len Pointer to buffer size (input) and actual length (output).
    * \return ATT status code (0x00 = success).
    * \note Set *len to actual number of bytes written to buf.
    * \note Return 0x04 (READ_REQ_RANGE_INVALID) if offset is out of range.
    */
   using GattReadCallback = std::function<uint8_t(uint16_t connHandle, uint16_t attrHandle,
                                                   uint8_t* buf, uint16_t* len)>;
   ```

3. **Add error code constants** to `IBluetoothController`:
   ```cpp
   namespace GattStatus {
       constexpr uint8_t SUCCESS = 0x00;
       constexpr uint8_t WRITE_NOT_PERMITTED = 0x03;
       constexpr uint8_t WRITE_REQ_TOO_LONG = 0x07;
       constexpr uint8_t INVALID_OFFSET = 0x07;
       constexpr uint8_t INSUFFICIENT_AUTHENTICATION = 0x08;
       constexpr uint8_t INSUFFICIENT_ENCRYPTION = 0x0F;
   }
   ```

4. **Update implementations** to match contract:
   ```cpp
   chars[0].onWrite = [](uint16_t /*connHandle*/, uint16_t /*attrHandle*/,
                         const uint8_t* data, uint16_t len) -> uint8_t {
       BleUartService::instance().onRxData(data, len);
       return cdc::hal::GattStatus::SUCCESS;  // Explicit success code
   };
   ```

## References
- `components/cdc_hal/include/cdc_hal/IBluetoothController.h:79-93` - Callback type definitions
- `components/mod_ble_serial/src/BleUartService.cpp:65-69` - Example implementation
- BLE Core Spec Vol 3, Part F, Section 3.2.2 - ATT error codes
