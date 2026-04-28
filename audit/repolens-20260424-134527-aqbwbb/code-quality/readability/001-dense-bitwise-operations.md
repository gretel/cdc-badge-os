---
title: "[001] [MEDIUM] Dense bitwise operations in key fingerprint generation lack clarity"
severity: MEDIUM
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_core/src/KeyFingerprint.cpp:50-54`, the `key_fingerprint_from_pubkey` function uses dense bitwise operations to extract 5-bit indices from a SHA-256 hash. The operations pack multiple bit shifts and ORs into single lines without intermediate variables or explanation:

```cpp
indices[0] = (hash[0] >> 3) & 0x1F;
indices[1] = ((hash[0] << 2) | (hash[1] >> 6)) & 0x1F;
indices[2] = (hash[1] >> 1) & 0x1F;
indices[3] = ((hash[1] << 4) | (hash[2] >> 4)) & 0x1F;
indices[4] = ((hash[2] << 1) | (hash[3] >> 7)) & 0x1F;
```

## Impact
- **Cognitive load**: A developer must manually trace bit positions to understand which bits map to which index
- **Maintenance risk**: Modifying the algorithm requires careful verification; easy to introduce off-by-one errors
- **Onboarding friction**: New team members need to "decode" the bit-packing logic rather than reading it like prose

## Evidence
**File**: `components/cdc_core/src/KeyFingerprint.cpp:50-54`

The algorithm extracts 5-bit chunks from a 32-bit window of the hash to index into a 32-word alchemical table. While the math is correct, the dense expressions make it hard to verify:
- Line 51: `hash[0]` contributes bits 7-3, `hash[1]` contributes bits 1-0
- Line 52: `hash[1]` contributes bits 7-2
- Line 53: `hash[1]` bits 7-4 + `hash[2]` bits 7-4
- Line 54: `hash[2]` bits 7-1 + `hash[3]` bit 7

A reader must mentally reconstruct this bit-stream to understand the mapping.

## Recommended Fix
Introduce named intermediate variables and add a comment explaining the bit-stream layout:

```cpp
// Extract 5-bit indices from consecutive hash bytes
// Bit stream: [hash[0]:7..0][hash[1]:7..0][hash[2]:7..0][hash[3]:7..0]
// Each index consumes 5 bits from the stream
indices[0] = (hash[0] >> 3) & 0x1F;           // Bits 7-3 of byte 0
indices[1] = ((hash[0] << 2) | (hash[1] >> 6)) & 0x1F;  // Bits 2-0 of byte 0 + bits 7-6 of byte 1
indices[2] = (hash[1] >> 1) & 0x1F;           // Bits 5-1 of byte 1
indices[3] = ((hash[1] << 4) | (hash[2] >> 4)) & 0x1F;  // Bits 0 of byte 1 + bits 7-4 of byte 2
indices[4] = ((hash[2] << 1) | (hash[3] >> 7)) & 0x1F;  // Bits 3-0 of byte 2 + bit 7 of byte 3
```

Alternatively, extract the bit-packing into a helper function:
```cpp
static uint8_t extractBits5(const uint8_t* hash, uint8_t bitOffset) {
    uint8_t byteIdx = bitOffset / 8;
    uint8_t bitPos = bitOffset % 8;
    uint32_t combined = (static_cast<uint32_t>(hash[byteIdx]) << 8) |
                        static_cast<uint32_t>(hash[byteIdx + 1]);
    return (combined >> (11 - bitPos)) & 0x1F;
}
```

## References
- [C++ Core Guidelines - CL.3: Use clear expressions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#cl3-use-clear-expressions)
- [Google C++ Style Guide - Comments](https://google.github.io/styleguide/cppguide.html#Comments)
