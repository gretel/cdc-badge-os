---
title: "[LOW] CPU-bound CRC16 computation in tight loop"
severity: LOW
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "crc"
  - "cpu-bound"
---

## Summary

The CRC16 implementation in `third_party/libtropic/src/lt_crc16.c` uses a bit-by-bit computation approach with a tight loop. While CRC16 is typically fast, the implementation processes each byte with 8 iterations of bit manipulation, which can add up when computing CRCs for larger data blocks (e.g., TROPIC01 SPI frames).

**Affected file:**
- `third_party/libtropic/src/lt_crc16.c` (lines 23-56)

**Evidence:**
```c
// third_party/libtropic/src/lt_crc16.c:23-41
static uint16_t crc16_byte(uint8_t data, uint16_t crc)
{
    uint16_t current_byte;
    int i;

    current_byte = data;
    crc ^= current_byte << 8;
    i = 8;  // Iterate over every bit in a byte.
    do {
        if (crc
            & 0x8000) {  // Highest bit set -> carry -> add generator polynomial
            crc <<= 1;
            crc ^= LT_CRC16_POLYNOMIAL;
        }
        else {
            crc <<= 1;
        }
    } while (--i);

    return (crc);
}

// third_party/libtropic/src/lt_crc16.c:43-56
uint16_t crc16(const uint8_t *data, int16_t len)
{
    uint16_t crc = LT_CRC16_INITIAL_VAL;

    while (--len >= 0) {
        crc = crc16_byte(*data++, crc);  // 8 bit-ops per byte
    }

    crc ^= LT_CRC16_FINAL_XOR_VALUE;

    return (crc << 8 | crc >> 8);
}
```

The `add_crc()` function calls `crc16()` for every TROPIC01 request:
```c
// third_party/libtropic/src/lt_crc16.c:58-67
void add_crc(void *req)
{
    uint8_t *p = (uint8_t *)req;
    uint16_t len = p[TR01_L2_REQ_LEN_OFFSET] + TR01_L2_REQ_ID_SIZE + TR01_L2_REQ_RSP_LEN_SIZE;

    uint16_t crc = crc16(p, len);  // Called for every SPI transaction

    p[len] = crc >> 8;
    p[len + 1] = crc & 00FF;
}
```

## Impact

1. **Per-transaction overhead**: CRC16 is computed for every TROPIC01 SPI request, adding ~10-50 CPU cycles per byte depending on frame size.

2. **No table lookup optimization**: The bit-by-bit approach is simpler but slower than a pre-computed 256-entry lookup table.

3. **Blocking in SPI path**: CRC computation happens synchronously before each SPI transfer, blocking the calling task.

4. **Firmware size tradeoff**: The current implementation uses minimal code size but sacrifices speed.

## Evidence

**Performance characteristics:**
- Bit-by-bit CRC: ~10-15 cycles per byte (no cache misses, but more instructions)
- Table-based CRC: ~2-3 cycles per byte (256-byte lookup table)

**Usage in TROPIC01 communication:**
- Every SPI request needs CRC16
- Typical frame sizes: 3-32 bytes
- Total overhead: ~30-480 cycles per transaction

## Recommended Fix

1. **Use table-based CRC16** (faster, slightly larger code):
```c
// Pre-computed CRC16 table
static const uint16_t crc16_table[] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
    // ... 248 more entries
};

uint16_t crc16(const uint8_t *data, int16_t len)
{
    uint16_t crc = LT_CRC16_INITIAL_VAL;

    while (--len >= 0) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF];
    }

    return (crc << 8 | crc >> 8);
}
```

2. **Use ESP32 hardware CRC** (fastest, if available):
```c
// ESP32-S3 has hardware CRC in SPI peripheral
// Can be used for SPI-transmitted CRC
```

3. **Batch CRC computation** (for multiple frames):
```c
// Compute CRC for multiple requests at once
// Reduces overhead when sending bulk data
```

**Estimated effort**: 30-60 minutes to implement table-based CRC

## References

- [CRC16-CCITT polynomial 0x8005](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
- [Table-driven CRC implementation](https://www.intel.com/content/www/us/en/developer/articles/technical/crc-calculation-in-c.html)
- TROPIC01 protocol uses CRC16 for frame integrity

</content>