---
title: "[MEDIUM] TotpStore API mixes C-style and C++-style interfaces"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `TotpStore` class (components/mod_totp/include/mod_totp/TotpStore.h) uses inconsistent interface styles:

```cpp
class TotpStore {
public:
    // C++ style
    static TotpStore& instance();
    bool readEntry(uint16_t slot, TotpEntry* out) const;
    
    // C-style callback
    using EntryCallback = void(*)(const TotpEntry& entry, void* ctx);
    void forEachEntry(uint8_t moduleId, EntryCallback cb, void* ctx);
    
    // C-style function in header
    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    
    // Returns C-style string
    const char* getEntryName(uint16_t logicalIndex);
};
```

The mix of styles creates inconsistency:
- Some methods use references (`TotpEntry&`)
- Some use callbacks with `void* ctx`
- Some return raw pointers (`const char*`)

## Impact
- **Inconsistent usage**: Developers must switch between styles
- **Memory safety**: Callback pattern requires manual context management
- **Type safety**: `void*` context can be anything

## Evidence
- API definition: components/mod_totp/include/mod_totp/TotpStore.h
- Callback type: line 28
- Method signatures: lines 21-35

## Recommended Fix
Standardize on C++ style:

```cpp
class TotpStore {
public:
    // Use std::function instead of C-style callback
    void forEachEntry(uint8_t moduleId, std::function<void(const TotpEntry&)> fn);
    
    // Return std::string_view instead of const char*
    std::string_view getEntryName(uint16_t logicalIndex);
    
    // Or use a proper struct for slot range
    struct SlotRange {
        uint16_t start;
        uint16_t end;
        uint8_t moduleId;
    };
    void setSlotRange(SlotRange range);
};
```

## References
- TotpStore: components/mod_totp/include/mod_totp/TotpStore.h
