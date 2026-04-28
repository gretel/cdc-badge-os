---
title: "[INFO] SQLMap applicable - No SQL database found in codebase"
severity: INFO
domain: database
lens: toolgate/session-sqlmap
labels:
  - "scan-scope"
---

## Summary

The **SQLMap pentest session** analyzed the `library/cdc-badge-os` repository to identify SQL injection candidates. The codebase is an **embedded C/C++ firmware project** for a hardware security key (ESP32-S3 with TROPIC01 secure element) and contains **no SQL database layer**.

**Repository Type:** Embedded firmware (PlatformIO/ESP-IDF)
**Language:** C/C++
**Data Storage:** NVS (flash-based key-value store) and TROPIC01 secure element (ECC slots, R-Memory)
**Database:** None

## Impact

**Low impact** - SQLMap is designed for web applications with SQL databases (PostgreSQL, MySQL, SQLite, MSSQL). This firmware uses:
- NVS (Non-Volatile Storage) - ESP32's built-in flash key-value store
- TROPIC01 secure element - Hardware-backed key storage with ECC slots and R-Memory

Neither of these are SQL databases and cannot be tested for SQL injection.

## Evidence

**Codebase structure:**
```
components/
  cdc_core/       Core services (EventBus, ServiceRegistry)
  cdc_hal/        Hardware abstraction (Display, Keypad, Power)
  mod_fido2/      FIDO2/WebAuthn module
  mod_totp/       TOTP store (NVS-based)
  mod_password/   Password vault (NVS + TROPIC01 R-Memory)
  ...
```

**No SQL queries found:**
- Searched for: `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `WHERE`, `FROM`
- Searched for ORM patterns: `.query()`, `.execute()`, `.raw()`, `.findOne()`, `.findAll()`
- Searched for query builders: `knex()`, `sequelize.query()`, `.whereRaw()`

All matches were false positives (UI strings like "SELECT", "DELETE" and APDU smart card commands).

**Data storage implementation (NVS example):**
```cpp
// From mod_totp - NVS key-value storage, not SQL
nvs_handle_t nvs = nvs_open("totp", NVS_READWRITE, &out);
nvs_set_str(nvs, key, value);
nvs_get_str(nvs, key, buffer, &size);
nvs_commit(nvs);
```

## Recommended Fix

**No action required** for SQL injection testing. This is not a SQL database application.

If SQLMap testing is needed, it should be applied to:
1. Any **web-based management interface** for the badge (if exists separately)
2. Any **backend API** that manages badge data (if exists separately)

## References

- [SQLMap Documentation](https://sqlmap.org/) - SQL database injection tool
- [ESP32 NVS Library](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs.html) - Key-value storage
- [TROPIC01 Datasheet](https://www.trinsicsecurity.com/tropic01/) - Secure element specification
