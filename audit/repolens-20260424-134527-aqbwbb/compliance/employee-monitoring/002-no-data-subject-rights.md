---
title: "[MEDIUM] No Transparency or Employee Data Subject Rights for Stored vCard Data"
severity: MEDIUM
domain: employee-monitoring
lens: betriebsrat-compliance
labels:
  - data-subject-rights
  - transparency
  - vcard-storage
---

## Summary

The **mod_vcard** component stores peer vCards in NVS (up to 100 contacts) but lacks implementation of **employee data subject rights** under GDPR and BetrVG. Employees cannot easily access, export, or delete their own monitoring data (stored vCards).

**Files Affected:**
- `components/mod_vcard/src/vcard_store.cpp` (lines 27-32, 400-450)
- `components/mod_vcard/include/mod_vcard/vcard_store.h` (lines 9-21)
- `components/mod_vcard/src/VcardModule.cpp` (lines 250-300)

**Evidence:**

```cpp
// components/mod_vcard/include/mod_vcard/vcard_store.h:6-7
#define VCARD_MAX_LEN   768
#define VCARD_MAX_CARDS 100  // Persistent storage without transparency

// components/mod_vcard/src/vcard_store.cpp:27-32
typedef struct {
    bool used;
    uint32_t hash;
    char last_name[32];
    char display[64];
} vcard_meta_t;

static vcard_meta_t g_cards[VCARD_MAX_CARDS];
static bool g_cards_loaded = false;
static uint16_t g_card_count = 0;

// Storage functions exist, but no "export all data" or "get metadata" functions
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len);
bool vcard_store_delete(uint16_t slot);
size_t vcard_store_get(uint16_t slot, char* out, size_t max_len);
// Missing: vcard_store_export_all(), vcard_store_get_metadata()
```

## Impact

Under **GDPR Articles 15-20** and **BetrVG §87 Abs. 1 Nr. 6**, employees have the right to:

1. **Access (Art. 15 GDPR)**: Know what data is stored about them and their contacts
2. **Export (Art. 20 GDPR)**: Receive their data in a structured format
3. **Delete (Art. 17 GDPR)**: Erase their data (right to be forgotten)

Current implementation issues:
- **No bulk export function**: Employees cannot retrieve all stored vCards in one operation
- **No metadata API**: Cannot query how many peers are stored, when they were added
- **No clear indexing**: vCards are stored by slot index, not by date or context
- **No audit trail**: No record of when vCards were added/modified

## Recommended Fix

Implement the following within ~1 hour:

1. **Add metadata structure and functions** in `vcard_store.h`:
   ```cpp
   typedef struct {
       uint16_t slot;
       char display[64];
       uint32_t added_timestamp;  // Unix timestamp when added
       uint32_t last_accessed;    // Last time vCard was read
   } vcard_info_t;
   
   uint16_t vcard_store_get_all_info(vcard_info_t* out, uint16_t max_count);
   char* vcard_store_export_all(char* out, size_t max_len);  // JSON or CSV format
   ```

2. **Add timestamp tracking** in `vcard_store.cpp`:
   ```cpp
   typedef struct {
       bool used;
       uint32_t hash;
       char last_name[32];
       char display[64];
       uint32_t added_ts;  // Add timestamp
   } vcard_meta_t;
   
   // In vcard_store_add():
   g_cards[free_slot].added_ts = xTaskGetTickCount() * portTICK_PERIOD_MS;
   ```

3. **Add serial commands for transparency** in `VcardModule.cpp`:
   ```cpp
   static void cmdVcardList(const char* args) {
       // List all stored vCards with metadata
       vcard_info_t info[100];
       uint16_t count = vcard_store_get_all_info(info, 100);
       for (uint16_t i = 0; i < count; i++) {
           serial::Console::printf("Slot %d: %s (added: %lu)\r\n", 
                                   info[i].slot, info[i].display, info[i].added_ts);
       }
   }
   
   static void cmdVcardExport(const char* args) {
       // Export all vCards in JSON format
       char buf[8192];
       char* out = vcard_store_export_all(buf, sizeof(buf));
       serial::Console::printf("%s\r\n", out);
   }
   
   // Register commands
   reg.registerCommand({"VCARD_LIST", "List all stored vCards", cmdVcardList, "vcard", false});
   reg.registerCommand({"VCARD_EXPORT", "Export all vCards (JSON)", cmdVcardExport, "vcard", false});
   ```

## References

- **GDPR Article 15**: Right of access by the data subject
- **GDPR Article 20**: Right to data portability
- **GDPR Article 17**: Right to delete (right to be forgotten)
- **BetrVG §87 Abs. 1 Nr. 6**: Works Council co-determination for monitoring systems
- **BAG Urteil vom 1.12.2015 (Az.: 1 ABR 10/14)**: Employees must be informed about monitoring
