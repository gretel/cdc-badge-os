---
title: "[MEDIUM] PinManager exposes implementation details in public API"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `PinManager` class exposes internal implementation details in its public API:

```cpp
// components/cdc_core/include/cdc_core/PinManager.h
class PinManager {
public:
    // Implementation details (should be private)
    static constexpr uint16_t RMEM_SLOT_PIN = 0;  // Internal storage location
    static constexpr uint8_t KDF_ITERSALTED_S2K = 0x03;  // OpenPGP spec constant
    static constexpr uint8_t HASH_SHA256 = 0x08;
    static constexpr uint32_t DEFAULT_ITERATIONS = 100000;
    static constexpr uint8_t MAGIC_V3 = 0xDD;  // Storage format magic
    static constexpr uint8_t STORAGE_SIZE = 106;
    
    // Public access to internal state
    uint8_t getBadgeRetries() const { return badgeRetries_; }
    uint8_t getPW1Retries() const { return pw1Retries_; }
    uint8_t getPW3Retries() const { return pw3Retries_; }
};
```

These are implementation details that shouldn't be part of the public API:
- `RMEM_SLOT_PIN` is an internal detail
- `MAGIC_V3` and `STORAGE_SIZE` are storage format details
- Retry counters are internal state

## Impact
- **API bloat**: Public API exposes too much detail
- **Fragile changes**: Changing storage format breaks consumers
- **Encapsulation violation**: Internal state is accessible

## Evidence
- PinManager.h: lines 40, 48-50, 108-110
- Public getters: lines 66, 82, 92

## Recommended Fix
Move implementation details to private section:

```cpp
class PinManager {
public:
    // Public API - what consumers need
    static constexpr uint8_t BADGE_PIN_MIN = 4;
    static constexpr uint8_t BADGE_PIN_MAX = 8;
    static constexpr uint8_t PW1_MIN = 6;
    static constexpr uint8_t PW3_MIN = 8;
    static constexpr uint8_t PIN_MAX = 16;
    
    // Methods
    bool verifyBadgePin(const char* pin);
    bool isBadgeBlocked() const;
    bool isPW1Blocked() const;
    bool isPW3Blocked() const;
    
private:
    // Implementation details
    static constexpr uint16_t RMEM_SLOT_PIN = 0;
    static constexpr uint8_t KDF_ITERSALTED_S2K = 0x03;
    static constexpr uint8_t HASH_SHA256 = 0x08;
    static constexpr uint32_t DEFAULT_ITERATIONS = 100000;
    static constexpr uint8_t MAGIC_V3 = 0xDD;
    static constexpr uint8_t STORAGE_SIZE = 106;
    
    // Internal state (no public getters)
    uint8_t badgeRetries_ = MAX_RETRIES;
    uint8_t pw1Retries_ = MAX_RETRIES;
    uint8_t pw3Retries_ = MAX_RETRIES;
};
```

If consumers need retry counts, expose via status struct:
```cpp
struct PinStatus {
    uint8_t badgeRetries;
    uint8_t pw1Retries;
    uint8_t pw3Retries;
    bool badgeBlocked;
    bool pw1Blocked;
    bool pw3Blocked;
};

PinStatus getStatus() const;
```

## References
- PinManager: components/cdc_core/include/cdc_core/PinManager.h
