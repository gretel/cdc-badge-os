---
title: "[MEDIUM] PasswordStore EntryIndex structure has unclear ownership semantics"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `PasswordStore::EntryIndex` structure stores a copy of the title:

```cpp
// components/mod_password/include/mod_password/PasswordStore.h
struct EntryIndex {
    char title[TITLE_LEN + 1];  // Owned by caller
    uint16_t slot;
};
```

Used in `listEntriesSorted()`:
```cpp
bool listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const;
```

The API is unclear about:
1. Who owns the `title` string (caller allocates or callee fills?)
2. How long the title remains valid
3. Whether the caller should pre-fill the structure or just allocate space

Looking at usage patterns, the callee fills the structure, but this isn't documented.

## Impact
- **Confusing API**: Developers may not know if they should pre-populate titles
- **Memory errors**: May double-free or access stale data
- **Inconsistent usage**: Different callers may use it differently

## Evidence
- Structure: components/mod_password/include/mod_password/PasswordStore.h:46-49
- Method: components/mod_password/include/mod_password/PasswordStore.h:56

## Recommended Fix
Clarify ownership with documentation and/or API redesign:

**Option A: Document current API**
```cpp
/**
 * \brief Entry index for sorted listing.
 * \note Caller allocates array, callee fills title and slot.
 */
struct EntryIndex {
    char title[TITLE_LEN + 1];  // Filled by callee
    uint16_t slot;              // Filled by callee
};

/**
 * \brief List password entries sorted by title.
 * \param entries Output array (caller allocates, size maxEntries)
 * \param maxItems Maximum entries to return
 * \param countOut Actual number of entries written
 * \return true on success
 */
bool listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const;
```

**Option B: Use string view for clarity**
```cpp
struct EntryIndex {
    std::string_view title;  // View into PasswordStore's data
    uint16_t slot;
};
```

**Option C: Return const pointers**
```cpp
struct EntryIndex {
    const char* title;  // Pointer to internal data
    uint16_t slot;
};
```

## References
- PasswordStore: components/mod_password/include/mod_password/PasswordStore.h
