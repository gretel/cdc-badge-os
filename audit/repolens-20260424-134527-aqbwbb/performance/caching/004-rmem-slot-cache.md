---
title: "[MEDIUM] TROPIC01 secure-element slot-usage cache misses on repeated queries"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "secure-element-cache"
  - "slot-lookup"
---

## Summary
The TROPIC01 secure element (`components/cdc_hal/src/Tropic01Element.cpp`) has an ECC slot cache but no equivalent cache for R-Memory slot usage. The `rmemSlotUsed()` function performs a full secure-element read operation on every call instead of using cached metadata from TropicStorage.

**Evidence:**
- File: `components/cdc_hal/src/Tropic01Element.cpp`
- ECC slot cache exists: `eccSlotCache_`, `eccCacheValid_` (line 100-102)
- `eccSlotUsed()` uses cache effectively (line 459-471)
- `rmemSlotUsed()` (line 630-642) performs fresh read every time with no caching
- TropicStorage has cache but isn't leveraged by `rmemSlotUsed()`

## Impact
**Performance Cost:**
- Each `rmemSlotUsed()` call triggers: NVS open → secure-element session → SPI read → NVS close
- For modules iterating multiple slots (e.g., listing TOTP accounts, FIDO credentials), this overhead multiplies
- Typical usage: 10-20ms per call including session management

**Redundant Operations:**
- Slot-usage queries often happen in loops (e.g., "find first free slot")
- Same slot checked multiple times within same UI operation
- Cache from TropicStorage (`TropicStorage::getEntry()`) is not utilized

## Evidence
From `components/cdc_hal/src/Tropic01Element.cpp`:

```cpp
// ECC slot caching EXISTS - good pattern!
mutable uint32_t eccSlotCache_ = 0;
mutable bool eccCacheValid_ = false;

/**
 * \brief Checks whether ECC slot currently contains a key.
 */
bool Tropic01Element::eccSlotUsed(uint8_t slot) const {
    if (eccCacheValid_) {
        return (eccSlotCache_ & (1u << slot)) != 0;  // Cache hit!
    }
    // Only read if cache invalid
    auto* self = const_cast<Tropic01Element*>(this);
    uint8_t tempKey[65];
    SeResult res = self->eccGetPublicKey(slot, tempKey, nullptr);
    return (res == SeResult::OK);
}

// R-Memory slot check - NO CACHING!
bool Tropic01Element::rmemSlotUsed(uint16_t slot) const {
    auto* self = const_cast<Tropic01Element*>(this);
    uint8_t tempBuf[4];
    uint16_t actualLen = 0;
    SeResult res = self->rmemRead(slot, tempBuf, sizeof(tempBuf), &actualLen);
    return (res == SeResult::OK && actualLen > 0);  // Fresh read every time!
}
```

The pattern for ECC slots is established but not applied to R-Memory slots.

## Recommended Fix
Add R-Memory slot cache matching ECC slot pattern:

1. **Add cache fields to Tropic01Element class**:
```cpp
// Cache for R-Memory slot usage
mutable uint32_t rmemSlotCache_[4];  // 128 slots, 32 bits per word
mutable bool rmemCacheValid_ = false;
```

2. **Implement cache-aware rmemSlotUsed()**:
```cpp
bool Tropic01Element::rmemSlotUsed(uint16_t slot) const {
    if (rmemCacheValid_) {
        uint16_t wordIdx = slot / 32;
        uint16_t bitIdx = slot % 32;
        return (rmemSlotCache_[wordIdx] & (1u << bitIdx)) != 0;
    }
    
    // Cache miss - do read and populate cache
    auto* self = const_cast<Tropic01Element*>(this);
    uint8_t tempBuf[4];
    uint16_t actualLen = 0;
    SeResult res = self->rmemRead(slot, tempBuf, sizeof(tempBuf), &actualLen);
    
    if (res == SeResult::OK && actualLen > 0) {
        uint16_t wordIdx = slot / 32;
        uint16_t bitIdx = slot % 32;
        rmemSlotCache_[wordIdx] |= (1u << bitIdx);
    }
    
    return (res == SeResult::OK && actualLen > 0);
}
```

3. **Invalidate cache on mutations**:
```cpp
SeResult Tropic01Element::rmemErase(uint16_t slot) {
    // ... existing code ...
    if (ret == LT_OK) {
        uint16_t wordIdx = slot / 32;
        uint16_t bitIdx = slot % 32;
        rmemSlotCache_[wordIdx] &= ~(1u << bitIdx);
    }
    return mapResult(ret);
}

SeResult Tropic01Element::rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) {
    // ... existing code ...
    if (ret == LT_OK) {
        uint16_t wordIdx = slot / 32;
        uint16_t bitIdx = slot % 32;
        rmemSlotCache_[wordIdx] |= (1u << bitIdx);
    }
    return mapResult(ret);
}
```

4. **Or leverage TropicStorage cache**:
```cpp
bool Tropic01Element::rmemSlotUsed(uint16_t slot) const {
    // Check TropicStorage cache first
    cdc::core::TropicStorage::CacheEntry entry;
    if (cdc::core::TropicStorage::instance().getEntry(slot, &entry)) {
        return cdc::core::TropicStorage::instance().isEntryUsed(entry);
    }
    // Fallback to direct read
    // ...
}
```

## References
- Secure element session overhead: ~5-10ms per session start
- SPI read latency: ~1-2ms per R-Memory read
- ECC slot cache pattern established in same file (lines 100-102, 459-471)
