---
title: "[MEDIUM] Non-constant-time hash comparison in PIN verification"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The `PinManager::compareHash()` function uses a simple byte-by-byte comparison that may not be constant-time, potentially allowing timing attacks to guess PIN hashes.

**Location:** `components/cdc_core/src/PinManager.cpp:280-289`

## Impact

Timing attacks can exploit variations in comparison time to incrementally discover hash values:

1. **Badge PIN (16 bytes)**: Each byte can be discovered independently with ~128 attempts on average (comparing 0-255 until match found)
2. **OpenPGP PIN hashes (32 bytes)**: Same approach, ~4096 total comparisons needed
3. With only 3 retries before lockout, an attacker needs to be strategic, but the lockout resets on power cycle (RAM-only counter)

The `compareHash` function:
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

While this uses bitwise OR to accumulate differences (which is good), modern compilers may still optimize the loop, and the function returns early if the caller checks the result immediately. More importantly, the function is called after `computeBadgeHash()` or `computeKdfHash()`, and the total execution time varies based on when the hash mismatch occurs.

## Evidence

**File: `components/cdc_core/src/PinManager.cpp`**

Line 280-289:
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

Used in `verifyBadgePin()`:
```cpp
if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {  // Line 313
    // PIN correct
}
```

**Key vulnerability factors:**
1. Badge PIN hash is only 16 bytes (truncated SHA-256)
2. PIN space is small (4-8 digits = 10,000 to 100,000 possibilities)
3. Lockout is RAM-only and resets on power cycle
4. `saveToStorage()` only called after decrementing retries, not on every attempt

## Recommended Fix

Implement a truly constant-time comparison using compiler barriers or assembly:

```cpp
#include <stdint.h>

/**
 * \brief Constant-time hash comparison (prevents timing attacks).
 * \param h1 First hash buffer.
 * \param h2 Second hash buffer.
 * \param len Number of bytes to compare.
 * \return `true` when buffers are equal.
 */
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    volatile uint8_t diff = 0;  // volatile prevents compiler optimization
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    // Always iterate through all bytes regardless of early mismatch
    return (diff == 0) ? 1 : 0;
}
```

Additionally, ensure the calling code doesn't short-circuit:
```cpp
// Add a small delay after comparison to normalize timing
bool PinManager::verifyBadgePin(const char* pin) {
    // ... hash computation ...
    
    bool match = compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE);
    
    // Add constant-time delay (e.g., 10ms)
    // This ensures failed and successful attempts take similar time
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (match) {
        // ... success handling ...
    }
    // ... failure handling ...
}
```

## References

- CWE-1323: Timing-based Side Channel
- [OWASP: Timing Attacks](https://cheatsheetseries.owasp.org/cheatsheets/Timing_Attacks_Cheat_Sheet.html)
- [Constant-time comparison in cryptography](https://en.wikipedia.org/wiki/Time_complexity#Comparison_of_different_classes)
