---
title: "[MEDIUM] Password Stored in NVS Without Expiration Tracking"
severity: MEDIUM
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The password module stores password entries in TROPIC01 R-Memory slots but does not track when passwords were last updated or when they should expire. Each `PasswordEntry` structure contains title, username, password, URL, TOTP slot, and notes fields, but no timestamp metadata for data freshness or retention purposes.

**File**: `components/mod_password/src/PasswordStore.cpp` and `components/mod_password/include/mod_password/PasswordStore.h`

## Impact
**Data Quality**: Without timestamps, users cannot determine how old their stored passwords are, making it difficult to implement password rotation policies.

**Compliance**: GDPR Art. 5(1)(e) requires data to be "kept in a form which permits identification of data subjects for no longer than is necessary." Without timestamps, it's impossible to determine if passwords have become stale or should be reviewed.

**User Experience**: No way to sort or filter passwords by age, making it hard to prioritize which passwords to update.

## Evidence

### PasswordEntry Structure (No Timestamps)
```cpp
// components/mod_password/include/mod_password/PasswordStore.h:42
struct PasswordEntry {
    char title[64];
    char username[64];
    char password[128];
    char url[128];
    uint16_t totpSlot;
    char notes[256];
    // NO timestamp fields!
};
```

### Storage Implementation
```cpp
// components/mod_password/src/PasswordStore.cpp:120
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // Finds free slot and writes entry
    // No timestamp capture here
}
```

### Serial Command Outputs Password in Plain Text
```cpp
// components/mod_password/src/PasswordModule.cpp:238
cdc::serial::Console::printf("Password: %s\r\n", entry.password);
```

## Recommended Fix

### Option 1: Add Timestamp Fields to PasswordEntry (~1 hour)
Modify the `PasswordEntry` structure to include creation and modification timestamps:

```cpp
// components/mod_password/include/mod_password/PasswordStore.h
struct PasswordEntry {
    char title[64];
    char username[64];
    char password[128];
    char url[128];
    uint16_t totpSlot;
    char notes[256];
    uint32_t created_at;     // Unix timestamp (seconds since epoch)
    uint32_t updated_at;     // Unix timestamp
};
```

Then update `addEntry` and `updateEntry` to populate timestamps:

```cpp
// components/mod_password/src/PasswordStore.cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // ... existing validation ...
    
    uint32_t now = cdc::hal::getRtc()->getUnixTime(); // or similar
    PasswordEntry withTimestamps = entry;
    withTimestamps.created_at = now;
    withTimestamps.updated_at = now;
    
    // ... rest of existing code ...
}

bool PasswordStore::updateEntry(uint16_t slot, const PasswordEntry& entry) {
    // ... existing validation ...
    
    PasswordEntry withTimestamps = entry;
    withTimestamps.updated_at = cdc::hal::getRtc()->getUnixTime();
    
    // ... rest of existing code ...
}
```

### Option 2: Add Serial Command for Password Age Report (~1 hour)
Add a new command to list passwords with their age:

```cpp
static void cmd_password_age(const char* args) {
    (void)args;
    auto& store = PasswordStore::instance();
    uint16_t cap = store.capacity();
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    uint16_t count = 0;
    store.listEntriesSorted(list.get(), cap, &count);
    
    uint32_t now = cdc::hal::getRtc()->getUnixTime();
    cdc::serial::Console::printf("Password Age Report:\r\n");
    for (uint16_t i = 0; i < count; i++) {
        uint16_t slot = list[i].slot;
        PasswordEntry entry;
        store.readEntry(slot, &entry);
        uint32_t age_days = (now - entry.updated_at) / 86400;
        cdc::serial::Console::printf("%u: %s (age: %u days)\r\n", i, entry.title, age_days);
    }
}

// In registerCommands():
reg.registerCommand({"PASSWORD_AGE", "List passwords by age", cmd_password_age, CMD_MODULE, true});
```

### References
- **GDPR Art. 5(1)(e)**: "Storage limitation" - Data kept no longer than necessary
- **GDPR Art. 5(1)(d)**: "Accuracy" - Data should be up-to-date with necessary updates
- Best practice: Password rotation every 60-90 days for critical accounts
