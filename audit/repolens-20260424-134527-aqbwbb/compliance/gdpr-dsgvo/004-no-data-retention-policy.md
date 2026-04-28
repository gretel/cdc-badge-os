---
title: "[LOW] No Data Retention Policy Implementation"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The CDC Badge OS firmware stores personal data indefinitely until manually deleted by the user. There is no implementation of data retention periods or automatic cleanup mechanisms. Article 5(1)(e) of the GDPR/DSGVO (storage limitation principle) requires that personal data be kept "no longer than is necessary for the purposes for which the personal data are processed."

## Impact
**Legal Risk**: Indefinite storage without retention periods may violate the storage limitation principle, especially for data that becomes stale or outdated (e.g., old contacts, expired passwords).

**Data Quality**: Stale data may lead to users relying on outdated information (wrong phone numbers, expired credentials).

**Storage Management**: As the badge ages, NVS and R-Memory may fill up with unused entries, requiring manual cleanup.

## Evidence

### Current Storage Implementation

1. **vCard Storage** - `components/mod_vcard/src/vcard_store.cpp`
   - NVS namespace `mod_vcard`, keys `c00`-`c99`
   - Lines 488-490: Count function shows no automatic cleanup
   - No timestamp or expiration field in vCard meta structure (line 24-29)

2. **Password Storage** - `components/mod_password/src/PasswordStore.cpp`
   - TROPIC01 R-Memory slots 150-511
   - Lines 14-22: `PasswordPayload` struct has no date fields
   - No expiration mechanism

3. **TOTP Storage** - `components/mod_totp/src/TotpStore.cpp`
   - TROPIC01 R-Memory slots 32-131
   - Lines 15-24: `TotpPayload` struct has no creation/expiration date
   - Only has `period` field for TOTP generation interval

4. **Storage Structure Analysis**
   ```cpp
   // vcard_store.cpp:24-29
   typedef struct {
       bool used;
       uint32_t hash;
       char last_name[32];
       char display[64];
   } vcard_meta_t;
   // No timestamp, no expiration, no last-accessed field
   ```

### Missing Features
No implementation of:
- Creation timestamps for entries
- Last-accessed timestamps
- Expiration dates
- Automatic cleanup of stale data
- "Data age" display in UI
- Retention period configuration

## Recommended Fix

### Option 1: Add Timestamps to Storage Structures (~1 hour)

**vcard_store.h** - Add to `vcard_meta_t`:
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
    uint32_t created_at;  // Unix timestamp
    uint32_t last_used;   // Unix timestamp
} vcard_meta_t;
```

**vcard_store.cpp** - Update storage functions:
```cpp
// In vcard_store_add(), after line 583:
g_cards[free_slot].created_at = time(nullptr);
g_cards[free_slot].last_used = time(nullptr);

// In vcard_store_get(), update last_used:
g_cards[slot].last_used = time(nullptr);
```

### Option 2: Add Retention Configuration (~30 min)

Add to `components/cdc_core/feature_flags.h`:
```cpp
// Default retention periods (0 = no retention, infinite)
#define DEFAULT_VCARD_RETENTION_DAYS    365
#define DEFAULT_PASSWORD_RETENTION_DAYS 0
#define DEFAULT_TOTP_RETENTION_DAYS     0
```

### Option 3: Add Cleanup Command (~30 min)

Add serial command to clean stale entries:
```cpp
static void cmd_cleanup_stale(const char* args) {
    // Parse days argument
    int days = atoi(args);
    if (days == 0) days = DEFAULT_VCARD_RETENTION_DAYS;
    
    uint32_t cutoff = time(nullptr) - (days * 86400);
    uint16_t cleaned = 0;
    
    // Clean vCards older than cutoff
    for (uint16_t slot = 0; slot < VCARD_MAX_CARDS; slot++) {
        if (g_cards[slot].used && g_cards[slot].last_used < cutoff) {
            vcard_store_delete(slot);
            cleaned++;
        }
    }
    
    Serial.printf("Cleaned %d stale vCards\n", cleaned);
}
```

### Option 4: Add UI Indicators (~30 min)

Add to UI:
- Display creation date for each entry
- Show "last used" date
- Color-code old entries (e.g., red text for >1 year old)

## References
- **Art. 5(1)(e) DSGVO** - Storage limitation principle
- **Art. 17 DSGVO** - Right to erasure (includes stale data)
- **ENISA Data Retention Guidelines** - https://www.enisa.europa.eu/
