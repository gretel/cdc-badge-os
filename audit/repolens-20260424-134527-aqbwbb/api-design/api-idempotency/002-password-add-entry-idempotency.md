---
title: "[MEDIUM] PASSWORD_ADD serial command lacks idempotency for duplicate password entries"
severity: MEDIUM
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The `PASSWORD_ADD` serial command (and `PasswordStore::addEntry()`) does not check for duplicate password entries before creating a new one. Repeated calls with the same title create multiple entries, wasting secure element R-Memory slots (180 slots available: 150-511).

**Location:** `components/mod_password/src/PasswordStore.cpp:207-247` (addEntry), `components/mod_password/src/PasswordModule.cpp:245-299` (cmd_password_add)

## Impact
- **Data duplication:** Multiple identical password entries can accumulate
- **Slot exhaustion:** Password vault uses 180 R-Memory slots; duplicates waste this limited resource
- **Inconsistent with FIDO2 pattern:** FIDO2 credentials check for existing RP+User combinations; passwords do not similar deduplication

## Evidence
```cpp
// components/mod_password/src/PasswordStore.cpp:207-247
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {  // Only checks for free slot, not duplicates
        LOG_W(TAG, "No free password slots");
        return false;
    }

    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    // ... populate payload ...

    auto res = se->rmemWriteWithHeader(
        slot, moduleId_, headerName, 0,
        reinterpret_cast<const uint8_t*>(&payload), sizeof(payload)
    );
    // ...
}
```

Serial command handler (no duplicate check):
```cpp
// components/mod_password/src/PasswordModule.cpp:245-299
static void cmd_password_add(const char* args) {
    PasswordEntry entry = {};
    // Parses title, username, password, url, totpSlot, notes
    // Calls addEntry without checking for existing entry
    bool ok = PasswordStore::instance().addEntry(entry);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## Recommended Fix
Add a duplicate check before creating a new password entry:

1. **Check by title** (case-insensitive) for uniqueness
2. **Return existing slot** if duplicate found
3. **Add helper function** `findEntryByTitle()` similar to FIDO2's `find_by_rp_user()`

Example implementation:
```cpp
int16_t PasswordStore::findEntryByTitle(const char* title) {
    if (!title || !hasSlotRange_) return -1;
    
    uint16_t cap = capacity();
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return -1;
    memset(used.get(), 0, cap * sizeof(bool));
    
    struct Ctx {
        const char* target;
        int16_t found;
    } ctx = { title, -1 };
    
    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (c->found >= 0) return;  // Already found
        if (strcasecmp(entry.name, c->target) == 0) {
            c->found = slot;
        }
    };
    
    cdc::core::TropicStorage::instance().forEachSlot(moduleId_, rmemStart_, rmemEnd_, cb, &ctx);
    return ctx.found;
}
```

Then modify `addEntry()`:
```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    
    // Check for duplicate title first
    int16_t existing = findEntryByTitle(entry.title);
    if (existing >= 0) {
        LOG_I(TAG, "Password entry '%s' already exists in slot %u", entry.title, existing);
        return false;  // Or update existing instead
    }
    
    // ... rest of existing add logic ...
}
```

## References
- FIDO2 `fido2_storage_find_by_rp_user()` pattern for finding existing credentials
- Password managers (LastPass, 1Password) use title/URL as unique key for entries
- Database unique constraints serve as safety nets for application-level deduplication
