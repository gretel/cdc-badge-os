---
title: "[MEDIUM] vCard Contact Data Stored Indefinitely Without Retention Policy"
severity: MEDIUM
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
The vCard module (`components/mod_vcard/src/vcard_store.cpp`) stores contact data (own vCard and peer vCards) in NVS with no retention policy. Contacts are stored indefinitely until manually deleted, with no automatic expiration, archival, or cleanup mechanism.

**Location**: `components/mod_vcard/src/vcard_store.cpp` (lines 16-18, 38-40 for storage structures; lines 316-382 for own vCard storage; lines 428-699 for peer vCard storage)

## Impact
1. **Storage Exhaustion**: NVS namespace `mod_vcard` has limited space; up to `VCARD_MAX_CARDS` (configurable) peer vCards plus own vCard can fill it
2. **PII Retention**: Contact data (names, emails, phones) stored indefinitely - potential GDPR "storage limitation" violation
3. **No Data Lifecycle**: No way to identify stale or old contacts for cleanup
4. **Orphaned Data**: Users may forget about old contacts, leaving dead data consuming storage

## Evidence
**Own vCard storage** (`vcard_store.cpp:16-18, 316-382`):
```cpp
static char g_own_vcard[VCARD_MAX_LEN + 1];  // Line 16
static bool g_own_loaded = false;
static bool g_own_present = false;
```

Own vCard persisted in NVS:
```cpp
// Lines 353-366
nvs_handle_t nvs;
if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
    set_err(err, err_len, "NVS open failed");
    return false;
}
esp_err_t ret = nvs_set_str(nvs, VCARD_KEY_OWN, tmp);  // "own" key
if (ret == ESP_OK) {
    ret = nvs_commit(nvs);
}
nvs_close(nvs);
```

**Peer vCard storage** (`vcard_store.cpp:38-40, 428-699`):
```cpp
static vcard_meta_t g_cards[VCARD_MAX_CARDS];  // Line 38 - metadata cache
static bool g_cards_loaded = false;
static uint16_t g_card_count = 0;
```

Peer vCards stored in NVS with keys like `c00`, `c01`, etc.:
```cpp
// Lines 55-58
static void vcard_key_for_slot(char* out, size_t out_len, uint16_t slot) {
    snprintf(out, out_len, "c%02u", static_cast<unsigned>(slot));
}

// Lines 565-570 - storing vCard
esp_err_t ret = nvs_set_str(nvs, key, tmp);  // key = "c00", "c01", etc.
if (ret == ESP_OK) {
    ret = nvs_commit(nvs);
}
```

**No retention fields**: vCard data stored as raw strings with no metadata:
- No `created_at` timestamp
- No `last_contacted` field
- No `retention_days` configuration
- No automatic cleanup based on age

## Recommended Fix
Add retention tracking and optional automatic cleanup:

**Option 1: Add retention metadata**
Create a metadata structure for each vCard:
```cpp
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
    uint32_t created_at;      // Unix timestamp
    uint32_t last_accessed;   // Unix timestamp
    uint8_t retention_days;   // 0 = indefinite
} vcard_meta_t;
```

Store metadata separately in NVS (e.g., `meta_00`, `meta_01` keys).

**Option 2: Add serial commands for retention management**
```bash
VCARD_LIST            # Show all vCards with age
VCARD_DELETE_OLD <days>  # Delete vCards older than X days
VCARD_SET_RETENTION <slot> <days>  # Set retention for specific vCard
```

**Option 3: Document retention policy**
Add to `docs/DATA_RETENTION.md`:
```markdown
## vCard Contact Data
- **Own vCard**: Indefinite (user's own contact)
- **Peer vCards**: Indefinite until manually deleted
- **Location**: NVS namespace `mod_vcard`
- **PII**: Names, emails, phone numbers
- **Legal basis**: User consent (contacts added manually)
```

**Option 4: Add "clear all" command**
```bash
VCARD_CLEAR_ALL  # Delete all peer vCards (keeps own vCard)
```

## References
- GDPR Article 5(1)(e) - Storage limitation principle
- ISO/IEC 27001 - Information security management (data lifecycle)
- NVS API documentation (`nvs.h`)

</content>