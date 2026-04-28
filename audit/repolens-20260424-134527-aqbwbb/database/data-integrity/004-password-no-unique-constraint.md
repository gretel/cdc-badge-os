---
title: "[MEDIUM] Password store lacks unique constraint on entry titles"
severity: MEDIUM
domain: data-integrity
lens: database
labels:
  - password-storage
  - uniqueness
  - duplicate-entries
---

## Summary
The `PasswordStore` allows multiple password entries with the same title. While entries are sorted alphabetically by title for display, there is no uniqueness constraint to prevent duplicates. This differs from the FIDO2 store which enforces (RP ID + User ID) uniqueness.

**Files:**
- `components/mod_password/src/PasswordStore.cpp:206-249` (addEntry function)
- `components/mod_password/include/mod_password/PasswordStore.h:49-51` (addEntry signature)

## Impact
Users can accidentally create duplicate password entries:
- Confusion about which entry is the correct/updated one
- Wasted storage slots (limited to 353 password slots: slots 159-511)
- Potential for using stale credentials after re-adding an entry

The `listEntriesSorted()` function sorts by title, but duplicates would appear adjacent with no indication they're duplicates.

## Evidence
In `PasswordStore.cpp:206-249`, the `addEntry` function:
```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        LOG_W(TAG, "No free password slots");
        return false;
    }
    // ... writes entry without checking for duplicate titles ...
}
```

The `findFreeSlot()` function at line 154-198 only checks for empty slots:
```cpp
bool PasswordStore::findFreeSlot(uint16_t* slotOut) const {
    // ... iterates TropicStorage cache to find unused slots ...
    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            *slotOut = candidate;
            return true;
        }
    }
    return false;
}
```

No check for existing entries with the same title.

## Recommended Fix
Add a title-based uniqueness check similar to TOTP fix:

1. Create `findEntryByTitle(const char* title)` that searches existing entries
2. Call this in `addEntry()` before writing
3. Return error or offer to update existing entry

```cpp
int16_t PasswordStore::findEntryByTitle(const char* title) const {
    if (!title) return -1;
    
    EntryIndex* entries = new(std::nothrow) EntryIndex[capacity()];
    if (!entries) return -1;
    
    uint16_t count = 0;
    listEntriesSorted(entries, capacity(), &count);
    
    for (uint16_t i = 0; i < count; i++) {
        if (compareTitles(entries[i].title, title) == 0) {
            delete[] entries;
            return entries[i].slot;  // Found duplicate
        }
    }
    
    delete[] entries;
    return -1;  // No duplicate
}

bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    
    // Check for duplicate title
    int16_t existing = findEntryByTitle(entry.title);
    if (existing >= 0) {
        LOG_W(TAG, "Duplicate password title: %s", entry.title);
        return false;  // Or call updateEntry(existing, entry)
    }
    
    // ... proceed with add ...
}
```

## References
- FIDO2 enforces (RP ID + User ID) uniqueness: `fido2_storage.cpp:558-590`
- TOTP could use similar (name + issuer) uniqueness
- Consider allowing case-insensitive comparison for better UX
