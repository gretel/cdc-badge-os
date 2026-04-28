---
title: "[MEDIUM] Individual R-Memory reads in batch contexts trigger repeated session checks"
severity: MEDIUM
domain: embedded-storage
lens: query-performance
labels:
  - "session-management"
  - "batch-read"
  - "rmem"
  - "lock-overhead"
---

## Summary

The `Tropic01Element` class calls `ensureSession()` and `lock()`/`unlock()` for every individual R-Memory operation. In batch contexts (like cache rebuild, credential enumeration, or password listing), this results in redundant session checks and mutex operations. Each R-Memory read involves: mutex lock → session check → SPI transaction → mutex unlock.

**Location**: `components/cdc_hal/src/Tropic01Element.cpp:547-590` (rmemRead), `components/cdc_hal/src/Tropic01Element.cpp:277-281` (ensureSession)

## Impact

**Performance Cost per Operation**:
- Mutex lock/unlock: ~10-50us (depends on contention)
- Session check: ~1us (simple flag read)
- SPI R-Memory read: ~100-500us (444 bytes at ~1MHz SPI)
- Total per read: ~150-600us

**Batch Operation Impact**:
- Cache rebuild (512 slots): 512 × ~300us = ~150ms
- FIDO2 credential list (30 creds): 30 × ~300us = ~9ms
- Password list (100 entries): 100 × ~300us = ~30ms

**Scalability**:
- Linear scaling with number of entries
- No batch optimization path available
- Session state checked on EVERY read, even when session is known to be active

## Evidence

**Session check in `rmemRead()` (lines 547-576)**:
```cpp
SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                    uint16_t* actualLen) {
    // ...
    lock();  // Mutex lock

    if (!ensureSession("rmemRead")) {  // Session check every call
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);
    // ...
    unlock();  // Mutex unlock
    // ...
}
```

**Session check in `ensureSession()` (lines 277-281)**:
```cpp
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;  // Fast path
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();  // Slow path: full handshake
}
```

**Called in batch context - `rebuildVerbose()` (TropicStorage.cpp:217-275)**:
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ...
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            
            // Each iteration triggers: lock → ensureSession → SPI → unlock
            auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
        }
    }
    // ...
}
```

**All R-Memory operations have same pattern**:
- `rmemRead()` (line 555): `ensureSession("rmemRead")`
- `rmemWrite()` (line 592): `ensureSession("rmemWrite")`
- `rmemErase()` (line 616): `ensureSession("rmemErase")`
- `rmemReadWithHeader()` (line 730): calls `rmemRead()` internally

## Recommended Fix

**Option 1: Add batch-optimized API**

Add methods that assume caller has already ensured session:

```cpp
/**
 * \brief R-Memory read without session check (caller ensures session).
 * \param slot R-memory slot index.
 * \param data Destination buffer.
 * \param maxLen Size of `data`.
 * \param actualLen Bytes read.
 * \return Operation result.
 */
SeResult rmemReadFast(uint16_t slot, uint8_t* data, uint16_t maxLen, uint16_t* actualLen) {
    // Assumes session is active - no ensureSession() call
    // Caller must lock() before calling
    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, actualLen);
    return mapResult(ret);
}

/**
 * \brief Batch R-Memory read with single session check.
 * \param slots Array of slot indices.
 * \param data Array of destination buffers.
 * \param count Number of slots to read.
 * \return `true` on success.
 */
bool rmemReadBatch(const uint16_t* slots, uint8_t** data, uint16_t count) {
    lock();
    if (!ensureSession("rmemReadBatch")) {
        unlock();
        return false;
    }
    
    for (uint16_t i = 0; i < count; i++) {
        lt_ret_t ret = lt_r_mem_data_read(&handle_, slots[i], data[i], 444, &actualLen[i]);
        // ...
    }
    unlock();
    return true;
}
```

**Option 2: Add scoped session guard**

Allow caller to manage session scope:

```cpp
class SessionScope {
public:
    SessionScope(Tropic01Element* se, const char* op) : se_(se) {
        se_->lock();
        if (!se_->ensureSession(op)) {
            valid_ = true;  // Still valid, just no session
        }
    }
    ~SessionScope() { se_->unlock(); }
    
    bool isValid() const { return valid_; }
    Tropic01Element* get() const { return se_; }
    
private:
    Tropic01Element* se_;
    bool valid_ = false;
};

// Usage:
void rebuildCache() {
    SessionScope guard(Tropic01Element::instance(), "rebuild");
    for (auto slot : slots) {
        // Caller guarantees session is active
        guard.get()->rmemReadFast(slot, ...);
    }
}
```

**Option 3: Session-aware loop in TropicStorage**

Modify `rebuildVerbose()` to handle session at loop level:

```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    if (!secureElement_) return false;
    
    // Ensure session once at start
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            return false;
        }
    }
    
    // Lock once for entire rebuild (or use finer granularity)
    secureElement_->lock();
    
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        // Use fast path reads
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            // ... rmemReadFast() without session check ...
        }
    }
    
    secureElement_->unlock();
    return true;
}
```

**Recommended approach**: Option 3 is simplest (~45 minutes). It requires minimal API changes and provides immediate benefit for the most expensive batch operation (cache rebuild).

## References

- [TROPIC01 SPI Interface](https://www.microchip.com/en-us/products/security-ics/secure-elements/tropic01) - R-Memory accessed via SPI with ~100us latency per 444-byte read
- [libtropic R-Memory API](third_party/libtropic/src/lt_l3_api_structs.h) - `lt_r_mem_data_read()` is the underlying function
- [ESP32 Mutex](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/kernel/semaphore.html) - Mutex lock/unlock overhead

</content>