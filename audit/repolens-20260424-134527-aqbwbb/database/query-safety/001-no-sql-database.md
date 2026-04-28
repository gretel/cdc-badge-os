---
title: "[INFO] No SQL Database Found - Query Safety Audit Complete"
severity: INFO
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary

A comprehensive query safety audit was performed on the CDC Badge OS codebase. **No SQL database or ORM usage was found.** This is an embedded firmware project using ESP32-S3 with TROPIC01 secure element, not a traditional application backend.

**Files Analyzed:**
- All `.cpp`, `.h`, `.c`, `.hpp` files in `components/`, `main/`, `test/`
- Python scripts in `tools/`
- JavaScript/TypeScript in `web-flasher/`
- Configuration files (CMakeLists.txt, platformio.ini)

## Impact

**No query safety issues exist** because there is no SQL database in this codebase. The data persistence layer consists of:

| Storage Type | Location | Purpose |
|--------------|----------|---------|
| **NVS** | 22 files | ESP32 key-value store for settings, PINs, counters |
| **TROPIC01 ECC Slots** | 15+ files | Hardware-based private key storage (slots 0-31) |
| **TROPIC01 R-Memory** | 15+ files | Structured metadata storage (slots 0-511) |

## Evidence

**Search Results:**

1. **SQL Keywords** (`SELECT`, `UPDATE`, `DELETE`, `INSERT`, `DROP`, `TRUNCATE`):
   - Found only in UI strings (e.g., "Delete", "Select" menu items)
   - Found in APDU command definitions (smart card `SELECT` command, not SQL)
   - Found in display update functions (e.g., `display.update()`)
   - Found in cryptographic digest operations (e.g., `md.update()`)

2. **ORM Methods** (`.update()`, `.delete()`, `.destroy()`, `.raw()`, `.execute()`):
   - All found methods are not ORM-related
   - `.update()` - Display refresh, cryptographic digest operations
   - `.delete` - UI menu actions

3. **Database Configuration Files**:
   - No `.db`, `.sqlite`, `.sql` files found
   - No `database.yml`, `ormconfig*`, `db_config*` files found

4. **Storage Implementation Examples**:
   ```cpp
   // NVS usage (components/cdc_core/src/NvsManager.cpp)
   nvs_open("cdc", NVS_READWRITE, &handle);
   nvs_set_str(handle, "language", "en");
   nvs_commit(handle);

   // TROPIC01 usage (components/cdc_hal/src/Tropic01Element.cpp)
   lt_tropic01_r_mem_write(&tropic, slot, offset, data, len);
   lt_tropic01_ecc_get_public_key(&tropic, slot, pubkey);
   ```

## Recommended Fix

**No action required.** This is informational only. The codebase uses appropriate embedded storage mechanisms for a hardware security key:

- **NVS** for simple key-value pairs (settings, counters)
- **TROPIC01 Secure Element** for cryptographic keys and structured metadata

These are the correct choices for an embedded firmware project with hardware security requirements.

## References

1. [ESP32 NVS Library](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
2. [TROPIC01 Secure Element Datasheet](https://www.microchip.com/en-us/product/tropic01)
3. [CDC Badge OS README](/input/20260423-132359-oj8ayc/cdc-badge-os/README.md)

## Audit Scope

| Category | Files Scanned | Results |
|----------|---------------|---------|
| SQL Queries | All source files | 0 found |
| ORM Usage | All source files | 0 found |
| SQL String Building | All source files | 0 found |
| Database Config | All config files | 0 found |

**DONE**
