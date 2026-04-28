---
title: "[MEDIUM] BleUuid equality operator may give false positives"
severity: MEDIUM
domain: architecture/api-contract
lens: type-safety
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `BleUuid` struct's equality operator (lines 46-52 in `IBluetoothController.h`) compares 128-bit UUIDs by direct memory comparison without ensuring all bytes are initialized, potentially causing false positives when comparing UUIDs with different type fields.

**Evidence location:** `components/cdc_hal/include/cdc_hal/IBluetoothController.h:27-52`

```cpp
struct BleUuid {
    enum Type : uint8_t { UUID_16 = 0, UUID_128 = 1 };
    Type type;
    union {
        uint16_t u16;
        uint8_t u128[16];
    };

    static BleUuid from16(uint16_t v) {
        BleUuid u;
        u.type = UUID_16;
        std::memcpy(u.u128, 0, sizeof(u.u128)); // zero padding
        return u;
    }

    static BleUuid from128(const uint8_t v[16]) {
        BleUuid u;
        u.type = UUID_128;
        std::memcpy(u.u128, v, 16);
        return u;
    }

    bool operator==(const BleUuid& other) const {
        if (type != other.type) return false;
        if (type == UUID_16) return u16 == other.u16;
        return std::memcmp(u128, other.u128, 16) == 0;
    }
};
```

## Impact
When comparing two 128-bit UUIDs, the operator compares all 16 bytes. However:
- `from16()` properly zero-pads the 128-bit array
- `from128()` copies only the provided 16 bytes but doesn't initialize the `Type` field in the union

More critically, if a `BleUuid` is default-constructed (e.g., in a struct initializer), the union members contain garbage values. Two such objects with the same `type` field but uninitialized union data may compare as equal or unequal unpredictably.

## Evidence
1. **Default constructor not defined**: `BleUuid` has no default constructor, so `BleUuid u;` leaves all members uninitialized.

2. **Union memory overlap**: The union means `u16` and `u128[0..1]` share memory. Comparing 16 bytes when only 2 were set (for a 16-bit UUID stored in a 128-bit comparison) gives incorrect results.

3. **Missing Type initialization in from128**: While `from128` sets `u.type = UUID_128`, the union assignment doesn't clear previous data.

**Problematic scenario:**
```cpp
BleUuid a;  // Uninitialized!
BleUuid b;  // Uninitialized!
if (a == b) {  // May be true if stack happens to have same values
    // False positive!
}

BleUuid uuid16 = BleUuid::from16(0x2A00);
BleUuid uuid128 = BleUuid::from128(bytes128);
// If uuid128's first 2 bytes happen to be 0x00, 0x2A (little-endian),
// and type happens to match, comparison may be wrong
```

## Recommended Fix
Add explicit initialization and improve comparison logic:

```cpp
struct BleUuid {
    Type type;
    union {
        uint16_t u16;
        uint8_t u128[16];
    };

    // Default constructor: zero-initialize
    BleUuid() : type(UUID_16), u16(0) {}

    static BleUuid from16(uint16_t v) {
        BleUuid u;
        u.type = UUID_16;
        u.u16 = v;
        // Zero-fill rest of array for consistent comparison
        std::memset(&u.u128[2], 0, 14);
        return u;
    }

    static BleUuid from128(const uint8_t v[16]) {
        BleUuid u;
        u.type = UUID_128;
        std::memcpy(u.u128, v, 16);
        return u;
    }

    bool operator==(const BleUuid& other) const {
        if (type != other.type) return false;
        if (type == UUID_16) {
            // Compare only meaningful bytes for 16-bit UUID
            return u16 == other.u16;
        }
        // Compare all 16 bytes for 128-bit UUID
        return std::memcmp(u128, other.u128, 16) == 0;
    }
};
```

Alternatively, add a `hashCode()` method for use in hash tables and ensure all comparison operators are implemented.

## References
- C++ Core Guidelines C.166: Use `constexpr` for simple functions
- C++ Core Guidelines C.17: For non-trivial classes, define copyable/movable by value
- ISO C++ Standard §9.5 (Unions): Active member must be tracked for correct access
