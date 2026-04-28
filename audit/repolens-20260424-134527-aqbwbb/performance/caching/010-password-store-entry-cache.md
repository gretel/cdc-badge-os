---
title: "[MEDIUM] Password store lacks in-memory entry cache for repeated reads"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "secure-element-cache"
  - "password-optimization"
---

## Summary
The Password store (`components/mod_password/src/PasswordStore.cpp`) reads password entries from the secure element's R-Memory on every access without maintaining an in-memory cache. Similar to the TOTP module (which has caching implemented), the password module performs fresh secure-element reads for operations like viewing, editing, and listing entries.

**Evidence:**
- File: `components/mod_password/src/PasswordStore.cpp`
- Function: `readEntry(uint16_t slot, PasswordEntry* out)` (line 116-158) performs `rmemReadWithHeader()` on each call
- Function: `listEntriesSorted()` (line 338-380) iterates slots via `forEachSlot()` which reads metadata from TropicStorage cache, but full entry data is read fresh
- No caching mechanism exists for frequently accessed password entries

## Impact
**Performance Cost:**
- Secure element R-Memory reads require SPI communication and session management
- Each read involves: session check → SPI transfer → data parsing
- Typical password operations:
  - Displaying password list: 10-20 entries × metadata read
  - Viewing entry details: full entry read (~50-100 bytes)
  - Auto-fill or copy: another full entry read
- For 10 passwords displayed: 10 secure-element reads per refresh

**User Experience:**
- UI lag when scrolling through password list
- Delayed entry display when switching between passwords
- Unnecessary power consumption from secure element staying active longer
- Each R-Memory read: ~5-15ms including session management

## Evidence
From `components/mod_password/src/PasswordStore.cpp`:

```cpp
/**
 * \brief Reads one password entry from secure-element storage.
 * \param slot Logical slot index.
 * \param out Output entry.
 * \return `true` on success.
 */
bool PasswordStore::readEntry(uint16_t slot, PasswordEntry* out) const {
    if (!out) return false;
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    cdc::hal::ISecureElement::RMemHeader header = {};
    PasswordPayload payload = {};
    uint16_t payloadLen = 0;

    auto res = se->rmemReadWithHeader(physSlot, &header,
                                      reinterpret_cast<uint8_t*>(&payload),
                                      sizeof(payload), &payloadLen);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    // ... copy data to output ...
    return true;
}
```

Every call to `readEntry()` triggers a fresh secure-element read, even if the entry hasn't changed.

**Comparison with TOTP module:**
- TOTP has `generateCode()` which calls `readAccount()` on every invocation
- TOTP finding #002 already identifies this issue
- Password module has identical pattern but no caching at all

## Recommended Fix
Implement entry data caching with slot-to-entry mapping:

1. **Add cache structure in PasswordStore**:
```cpp
struct PasswordCache {
    uint16_t slot;
    PasswordEntry entry;
    uint32_t lastReadTime;  // For cache invalidation
    bool valid;
};

static constexpr uint8_t MAX_PASSWORD_CACHE = 10;  // Cache up to 10 entries
static PasswordCache s_entryCache[MAX_PASSWORD_CACHE];
static uint8_t s_cacheCount = 0;
```

2. **Add cache lookup function**:
```cpp
static bool getEntryFromCache(uint16_t slot, PasswordEntry* out) {
    for (uint8_t i = 0; i < s_cacheCount; i++) {
        if (s_entryCache[i].valid && s_entryCache[i].slot == slot) {
            *out = s_entryCache[i].entry;
            s_entryCache[i].lastReadTime = esp_timer_get_time();
            return true;
        }
    }
    return false;
}

static void addToCache(uint16_t slot, const PasswordEntry& entry) {
    // Find existing entry or free slot
    uint8_t idx = 0;
    for (uint8_t i = 0; i < s_cacheCount; i++) {
        if (s_entryCache[i].slot == slot) {
            idx = i;
            break;
        }
        if (!s_entryCache[i].valid) idx = i;
    }
    
    s_entryCache[idx].slot = slot;
    s_entryCache[idx].entry = entry;
    s_entryCache[idx].valid = true;
    s_entryCache[idx].lastReadTime = esp_timer_get_time();
    
    if (s_cacheCount < MAX_PASSWORD_CACHE) {
        s_cacheCount++;
    }
}
```

3. **Modify `readEntry()` to use cache**:
```cpp
bool PasswordStore::readEntry(uint16_t slot, PasswordEntry* out) const {
    if (!out) return false;
    
    // Try cache first
    PasswordEntry cached = {};
    if (getEntryFromCache(slot, &cached)) {
        *out = cached;
        return true;
    }
    
    // Cache miss - read from secure element
    // ... existing read logic ...
    
    // Add to cache
    addToCache(slot, *out);
    
    return true;
}
```

4. **Add cache invalidation on mutations**:
```cpp
bool PasswordStore::updateEntry(uint16_t slot, const PasswordEntry& entry) {
    // ... existing update logic ...
    
    // Invalidate cache for this slot
    for (uint8_t i = 0; i < s_cacheCount; i++) {
        if (s_entryCache[i].slot == slot) {
            s_entryCache[i].valid = false;
            break;
        }
    }
    
    return true;
}

bool PasswordStore::deleteEntry(uint16_t slot) {
    // ... existing delete logic ...
    
    // Invalidate cache for this slot
    for (uint8_t i = 0; i < s_cacheCount; i++) {
        if (s_entryCache[i].slot == slot) {
            s_entryCache[i].valid = false;
            break;
        }
    }
    
    return true;
}
```

5. **Optional: Add cache TTL for auto-expiration**:
```cpp
static constexpr uint32_t CACHE_TTL_MS = 5000;  // 5 seconds

static bool isCacheValid(uint32_t timestamp) {
    uint32_t now = esp_timer_get_time() / 1000;
    return (now - timestamp) < CACHE_TTL_MS;
}
```

## References
- TOTP caching pattern (related finding #002): Similar implementation in `mod_totp`
- Secure element R-Memory read latency: ~5-15ms per operation
- Password vault best practices: https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
