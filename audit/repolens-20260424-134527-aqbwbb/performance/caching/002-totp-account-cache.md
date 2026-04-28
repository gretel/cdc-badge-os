---
title: "[MEDIUM] No TOTP account cache - repeated secure-element reads for code generation"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "secure-element-cache"
  - "totp-optimization"
---

## Summary
The TOTP store (`components/mod_totp/src/TotpStore.cpp`) reads account data from the secure element's R-Memory on every code generation. Since TOTP codes are typically displayed and refreshed every 30 seconds, the same account data is read multiple times without caching.

**Evidence:**
- File: `components/mod_totp/src/TotpStore.cpp`
- Function: `generateCode(uint16_t slot)` (line 480) calls `readAccount(slot, &account)`
- Function: `readAccount(uint16_t slot, TotpAccount* out)` (line 150) performs `rmemReadWithHeader()` on each call
- TOTP codes displayed on UI refresh read account data fresh each time

## Impact
**Performance Cost:**
- Secure element R-Memory reads require SPI communication and session management
- Each read involves: session check → SPI transfer → data parsing
- Typical TOTP UI refreshes every second (countdown timer) while code is valid
- For 10 TOTP accounts shown on a list view: 10 secure-element reads per refresh

**User Experience:**
- UI lag when scrolling through TOTP accounts
- Delayed code display when switching between accounts
- Unnecessary power consumption from secure element staying active longer

## Evidence
From `components/mod_totp/src/TotpStore.cpp`:

```cpp
/**
 * \brief Generates formatted TOTP code string for account slot.
 */
int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    if (!codeOut) return -1;

    TotpAccount account = {};
    if (!readAccount(slot, &account)) {  // Fresh read every time!
        return -1;
    }

    // ... generate code from account data
}

/**
 * \brief Reads one TOTP account from secure-element storage.
 */
bool TotpStore::readAccount(uint16_t slot, TotpAccount* out) {
    // ...
    auto res = se->rmemReadWithHeader(physSlot, &header, payloadBuf, sizeof(payloadBuf), &payloadLen);
    // ...
}
```

Every call to `generateCode()` triggers a fresh secure-element read, even if the account hasn't changed.

## Recommended Fix
Implement account data caching with slot-to-account mapping:

1. **Add cache structure** in TotpStore:
```cpp
struct AccountCache {
    uint16_t slot;
    TotpAccount account;
    uint32_t lastReadTime;  // For cache invalidation
};

static AccountCache s_accountCache[10];  // Cache up to 10 accounts
static uint8_t s_cacheCount = 0;
```

2. **Add cache lookup function**:
```cpp
static bool getAccountFromCache(uint16_t slot, TotpAccount* out) {
    for (uint8_t i = 0; i < s_cacheCount; i++) {
        if (s_accountCache[i].slot == slot) {
            *out = s_accountCache[i].account;
            return true;
        }
    }
    return false;
}
```

3. **Modify `generateCode()` to use cache**:
```cpp
int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    TotpAccount account = {};
    
    // Try cache first
    if (!getAccountFromCache(slot, &account)) {
        if (!readAccount(slot, &account)) {
            return -1;
        }
        // Add to cache
        addToCache(slot, account);
    }
    
    // Generate code from cached data
    // ...
}
```

4. **Add cache invalidation** when account is updated/deleted in `addAccount()`, `updateAccount()`, `deleteAccount()`

## References
- TOTP refresh cycle: 30s default period
- Secure element latency: SPI reads typically 5-20ms per operation
- Embedded caching patterns: https://www.espressif.com/sites/default/files/wiki/esp32-memory-management.pdf
