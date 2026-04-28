---
title: "[LOW] ISecureElement RMemHeader packing may cause portability issues"
severity: LOW
domain: architecture/api-contract
lens: data-layout
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `ISecureElement::RMemHeader` struct uses `__attribute__((packed))` but doesn't explicitly document byte order (endianness) or alignment guarantees. This creates potential portability issues if data is exchanged between different architectures or if the header is read by external tools.

**Evidence location:** `components/cdc_hal/include/cdc_hal/ISecureElement.h:163-169`

```cpp
struct __attribute__((packed)) RMemHeader {
    uint8_t magic;
    uint8_t checksum;
    uint8_t moduleId;
    uint8_t flags;
    char name[16];
    uint16_t payloadLen;
};
```

## Impact
1. **Endianness**: `payloadLen` is a `uint16_t` but byte order isn't specified. Big-endian vs little-endian systems will interpret this differently.
2. **Checksum algorithm**: The `checksum` field has no documented algorithm (simple XOR, CRC, sum?).
3. **Name encoding**: `char name[16]` doesn't specify encoding (ASCII, UTF-8, null-terminated?).
4. **Magic number**: No documentation of valid magic values or versioning.

## Evidence
The struct is used in `rmemWriteWithHeader()` and `rmemReadWithHeader()`:
```cpp
virtual SeResult rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                     const char* name, uint8_t flags,
                                     const uint8_t* payload, uint16_t payloadLen) = 0;

virtual SeResult rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                    uint8_t* payloadOut, uint16_t payloadMax,
                                    uint16_t* payloadLenOut) = 0;
```

No documentation specifies:
- Byte order of multi-byte fields
- Checksum calculation method
- Name encoding and termination
- Magic number values

## Recommended Fix
Add explicit documentation and consider standardizing the format:

```cpp
/**
 * \brief R-Memory slot header structure.
 *
 * Layout (23 bytes, packed, little-endian):
 * [0]  magic:        0xDD (version 1)
 * [1]  checksum:     XOR of all payload bytes
 * [2]  moduleId:     Module ID (0-254, 255=UNKNOWN)
 * [3]  flags:        Bitfield (bit 0 = FLAG_USED)
 * [4-19] name:       ASCII, null-terminated, padded with 0x00
 * [20-21] payloadLen: Little-endian 16-bit length
 *
 * Total header size: 22 bytes
 * Max payload: 454 bytes (476 - 22)
 */
struct __attribute__((packed)) RMemHeader {
    uint8_t magic;        ///< 0xDD for version 1
    uint8_t checksum;     ///< XOR of payload bytes
    uint8_t moduleId;     ///< Module ID
    uint8_t flags;        ///< Bitfield
    char name[16];        ///< ASCII name, null-terminated
    uint16_t payloadLen;  ///< Little-endian
};
```

Alternatively, use explicit byte-order functions:
```cpp
struct RMemHeader {
    uint8_t magic;
    uint8_t checksum;
    uint8_t moduleId;
    uint8_t flags;
    char name[16];
    uint16_t payloadLen;  ///< Store in little-endian

    uint16_t getPayloadLen() const {
        return __builtin_bswap16(payloadLen);  // Or platform-specific
    }

    void setPayloadLen(uint16_t len) {
        payloadLen = __builtin_bswap16(len);
    }
};
```

## References
- RFC 1700: Assigned Numbers (byte order conventions)
- C++ Core Guidelines E.5: Don't cast between pointer types
- C++ Core Guidelines E.6: Use `reinterpret_cast` for low-level casts
