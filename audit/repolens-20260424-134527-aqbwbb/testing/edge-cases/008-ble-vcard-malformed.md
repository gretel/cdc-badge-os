---
title: "[MEDIUM] BLE vCard exchange lacks edge case tests for malformed data"
severity: MEDIUM
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The BLE vCard exchange implementation in `ble_vcard.cpp` handles incoming vCard data but lacks edge case tests for malformed or boundary inputs:

1. **Empty vCard from BLE** - `s_rx_vcard` with length 0
2. **Truncated vCard** - Missing END:VCARD or partial data
3. **vCard larger than buffer** - `len >= sizeof(s_rx_vcard)` (768 bytes)
4. **vCard with embedded NUL** - `"BEGIN:VCARD\x00VERSION:4.0..."`
5. **Invalid UTF-8** - Malformed Unicode in name fields
6. **Very long name field** - Name exceeding buffer size
7. **Special BLE characters** - Null bytes, control characters in advertising data

**Evidence** (file:line):
- `ble_vcard.cpp:294-317` - `onCharacteristicRead()` handling
- `ble_vcard.cpp:128-129` - `s_rx_vcard` buffer definition
- `ble_vcard.cpp:552-570` - `processScanResults()` parsing

```cpp
// onCharacteristicRead at ble_vcard.cpp:294-317
static void onCharacteristicRead(uint16_t connHandle, uint16_t attrHandle,
                                  const uint8_t* data, uint16_t len) {
    (void)connHandle;
    (void)attrHandle;

    if (s_exchange_state != VCARD_EXCHANGE_READING_PEER_VCARD) return;

    if (data && len > 0 && len < sizeof(s_rx_vcard)) {
        memcpy(s_rx_vcard, data, len);
        s_rx_vcard[len] = '\0';
        // ...
    } else {
        setExchangeError("Invalid vCard data");
    }
}
```

The check `len < sizeof(s_rx_vcard)` at line 298 should be `len <= sizeof(s_rx_vcard) - 1` to account for null terminator.

## Impact
- **Buffer overflow risk**: If `len == sizeof(s_rx_vcard)`, null terminator overwrites buffer
- **Malformed data acceptance**: Truncated vCards might pass validation
- **Unicode issues**: Invalid UTF-8 could cause display issues

## Evidence
No edge case tests exist for BLE vCard data handling. The test file `test_ble_vcard_symbols.cpp` only tests initialization.

## Recommended Fix
Add edge case tests:

```cpp
void test_ble_vcard_empty_data() {
    // Simulate empty vCard from BLE
    const uint8_t empty_data[] = "";
    onCharacteristicRead(1, 1, empty_data, 0);
    // Should set error
}

void test_ble_vcard_truncated() {
    // vCard missing END:VCARD
    const uint8_t truncated[] = "BEGIN:VCARD\nVERSION:4.0\nFN:Test";
    onCharacteristicRead(1, 1, truncated, sizeof(truncated) - 1);
    // Should fail validation
}

void test_ble_vcard_exact_buffer_size() {
    // vCard at exactly buffer size (768)
    char exact[769];
    memset(exact, 'A', 768);
    exact[768] = '\0';
    const uint8_t* data = (const uint8_t*)exact;
    onCharacteristicRead(1, 1, data, 768);
    // Should handle gracefully (at boundary)
}

void test_ble_vcard_just_over_buffer() {
    // vCard at buffer size + 1 (769)
    char over[770];
    memset(over, 'A', 769);
    over[769] = '\0';
    const uint8_t* data = (const uint8_t*)over;
    onCharacteristicRead(1, 1, data, 769);
    // Should fail validation
}

void test_ble_vcard_embedded_null() {
    // vCard with NUL in middle
    const uint8_t null_vcard[] = "BEGIN:VCARD\nVERSION:4.0\x00\nEND:VCARD\n";
    onCharacteristicRead(1, 1, null_vcard, sizeof(null_vcard) - 1);
    // Should fail validation
}

void test_ble_vcard_special_chars() {
    // vCard with control characters
    const uint8_t ctrl_vcard[] = "BEGIN:VCARD\nVERSION:4.0\nFN:\x01\x02\x03\nEND:VCARD\n";
    onCharacteristicRead(1, 1, ctrl_vcard, sizeof(ctrl_vcard) - 1);
    // Should handle gracefully
}

void test_ble_scan_malformed_adv() {
    // Test BLE advertising parser with malformed data
    uint8_t mal_adv[] = {0xFF, 0x00, 0x01}; // Invalid structure
    uint16_t companyId = 0;
    const uint8_t* mfgPayload = nullptr;
    uint8_t mfgLen = 0;
    bool found = BleAdvParser::findManufacturerData(mal_adv, sizeof(mal_adv),
                                                    &companyId, &mfgPayload, &mfgLen);
    // Should handle gracefully
}
```

## References
- BLE advertising data format
- vCard 4.0 specification
- Buffer overflow edge cases
