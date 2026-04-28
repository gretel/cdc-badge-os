---
title: "[MEDIUM] TropicStorage.h missing documentation for CacheEntry struct"
severity: MEDIUM
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `TropicStorage.h` file has a detailed comment at the start of the file, but the `CacheEntry` struct and its members are completely undocumented. This struct is central to understanding how slot metadata is stored.

**File:** `components/cdc_core/include/cdc_core/TropicStorage.h:9-78`

## Impact

- Struct field meanings unclear (e.g., what do the flags mean?)
- No guidance on how this struct is used in the cache system
- Makes debugging cache issues harder

## Evidence

```cpp
// Line 10-15: Undocumented struct
struct CacheEntry {
    uint8_t moduleId;         ///< No docs - what is this?
    uint8_t flags;            ///< No docs - what flags?
    char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];
} __attribute__((packed));

// Line 17-18: Undocumented constants
static constexpr uint8_t FLAG_USED = 0x01;   ///< What does this flag mean?
static constexpr uint16_t CHUNK_SLOTS = 64;  ///< What is a "chunk"?
```

## Recommended Fix

Add documentation to struct and members:

```cpp
/**
 * \brief Cache entry for one R-Memory slot.
 *
 * Stored in NVS cache to avoid reading TROPIC01 for every lookup.
 * Packed layout for efficient storage.
 */
struct CacheEntry {
    uint8_t moduleId;           ///< Owning module ID (0-255)
    uint8_t flags;              ///< Entry flags (FLAG_USED = 0x01)
    char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];  ///< Slot name (max 16 chars)
} __attribute__((packed));

/**
 * \brief Flag: Entry is in use.
 */
static constexpr uint8_t FLAG_USED = 0x01;

/**
 * \brief Number of entries per NVS chunk.
 *
 * Cache is split into chunks for efficient NVS writes.
 */
static constexpr uint16_t CHUNK_SLOTS = 64;
```

## References

- Related: `ISecureElement.h` defines `RMEM_NAME_LEN` constant
