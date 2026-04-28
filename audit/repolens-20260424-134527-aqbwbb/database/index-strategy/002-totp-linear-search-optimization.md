---
title: "[MEDIUM] TOTP store uses linear search for account lookup by name"
severity: MEDIUM
domain: database
lens: index-strategy
labels:
  - audit:database/index-strategy
---

## Summary
The TOTP store (`components/mod_totp/src/TotpStore.cpp`) lacks an efficient index for looking up accounts by name. The `findFreeSlot()` method at line 194-230 iterates through all slots using `forEachSlot()` to build a used-boolean array, but there is no index for finding an account by name (useful for update/check-duplicate operations).

**Evidence:**
- `TotpStore::findFreeSlot()` at `components/mod_totp/src/TotpStore.cpp:194-230`
- No `findAccountByName()` or similar lookup method exists
- `TotpModule` needs to scan all entries for operations like finding an account to update

## Impact
- **No duplicate prevention**: Cannot efficiently check if an account with the same name already exists before adding
- **Update inefficiency**: To update an account by name, caller must iterate all entries manually
- **Memory allocation on every find**: `findFreeSlot()` allocates `bool[cap]` array each time (line 204)
- **Scalability**: 100-slot TOTP range means up to 100 iterations per lookup

## Evidence
```cpp
// components/mod_totp/src/TotpStore.cpp:194-230
bool TotpStore::findFreeSlot(uint16_t* slotOut) {
    // ...
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);  // Allocates every time
    // ...
    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);  // Full scan
}
```

```cpp
// No method exists for:
// - findAccountByName(const char* name)
// - findAccountByIssuer(const char* issuer)
// - countAccounts() without full scan
```

## Recommended Fix
Add a lightweight name index to `TotpStore`:

1. **Add index structure** (stored in NVS cache alongside existing data):
   ```cpp
   struct NameIndex {
       char name[TotpStore::NAME_LEN + 1];
       uint16_t slot;
   };
   ```

2. **Add index methods** to `TotpStore`:
   ```cpp
   bool buildNameIndex();  // Called during rebuild
   void addNameIndex(uint16_t slot, const char* name);
   void removeNameIndex(uint16_t slot);
   uint16_t findSlotByName(const char* name);  // Returns 0xFFFF if not found
   uint8_t countAccounts();  // O(1) from index count
   ```

3. **Optimize `findFreeSlot()`**:
   - Use cached "used slots" bitmask from TropicStorage instead of allocating array
   - Or maintain a free-list in NVS for O(1) free slot lookup

4. **Integration points**:
   - Call index update in `addAccount()`, `updateAccount()`, `deleteAccount()`
   - Rebuild index in `TropicStorage::rebuild()` callback

## References
- Hash index for O(1) lookups: https://en.wikipedia.org/wiki/Hash_table
- Linear probing vs chaining: https://www.geeksforgeeks.org/hashing-in-data-structure/
