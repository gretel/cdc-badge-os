---
title: "[MEDIUM] RMem name truncation edge case in TropicStorage rebuild"
severity: MEDIUM
domain: storage
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary

The `TropicStorage::rebuildVerbose()` function (`components/cdc_core/src/TropicStorage.cpp:252-253`) uses `strncpy()` to copy the `header.name` field from the TROPIC01 R-Memory header into the `CacheEntry.name` field. While the code properly null-terminates the destination, there's an edge case where the **source header.name might not be null-terminated** if it was written with exactly 16 bytes (filling the entire name field).

**Location**: `components/cdc_core/src/TropicStorage.cpp:252-253`

When `rmemReadWithHeader()` reads a name that was written with exactly `RMEM_NAME_LEN` (16) bytes, the source `header.name` array might not have a null terminator. The `strncpy()` will read past the intended boundary looking for a null byte, potentially reading garbage data from the stack.

## Impact

**Memory Safety Risk**: If the source `header.name` is exactly 16 bytes with no null terminator, `strncpy()` will continue reading beyond the 16-byte boundary until it finds a null byte. This could:
- Read uninitialized stack memory (potentially sensitive data)
- Cause undefined behavior depending on compiler/optimization
- Result in inconsistent name truncation

**Data Integrity**: The name displayed in log output (line 254) could be incorrect or include garbage characters.

## Evidence

**Relevant code at line 169-170** (ISecureElement.h):
```cpp
struct __attribute__((packed)) RMemHeader {
    uint8_t magic;
    uint8_t checksum;
    uint8_t moduleId;
    uint8_t flags;
    char name[RMEM_NAME_LEN];  // RMEM_NAME_LEN = 16
    uint16_t payloadLen;
};
```

**Code at line 252-253** (TropicStorage.cpp):
```cpp
strncpy(entry.name, header.name, sizeof(entry.name) - 1);
entry.name[sizeof(entry.name) - 1] = '\0';
```

**The issue**: `strncpy()` behavior when source is not null-terminated:
- If `header.name` has exactly 16 non-null characters, `strncpy()` will read past the 16-byte boundary
- The destination is properly truncated to 15 chars + null, but the source read may overflow

**Test case that triggers edge case**:
- Write RMem with name = exactly 16 characters (no null terminator in source)
- Rebuild cache
- Result: `strncpy()` reads past `header.name[15]` looking for null

## Recommended Fix

Use `memcpy()` with explicit length calculation, or ensure the source is always null-terminated before copying:

**Option 1 - Safer string copy**:
```cpp
// Copy up to 15 chars, ensure null termination
char srcName[RMEM_NAME_LEN + 1] = {};  // Extra byte for null
std::memcpy(srcName, header.name, RMEM_NAME_LEN);  // header.name is guaranteed 16 bytes
strncpy(entry.name, srcName, sizeof(entry.name) - 1);
entry.name[sizeof(entry.name) - 1] = '\0';
```

**Option 2 - Direct copy with manual null termination**:
```cpp
// Direct copy, then null-terminate
std::memcpy(entry.name, header.name, RMEM_NAME_LEN);
entry.name[RMEM_NAME_LEN - 1] = '\0';  // Always null-terminate at position 15
```

**Option 3 - Use strnlen to find actual length**:
```cpp
size_t nameLen = strnlen(header.name, RMEM_NAME_LEN);
if (nameLen > sizeof(entry.name) - 1) {
    nameLen = sizeof(entry.name) - 1;
}
std::memcpy(entry.name, header.name, nameLen);
entry.name[nameLen] = '\0';
```

## References

- [C++ `strncpy()` behavior](https://en.cppreference.com/w/cpp/string/byte/strncpy)
- [ CWE-170: Improper Null Termination](https://cwe.mitre.org/data/definitions/170.html)
- [Secure Coding: String Functions](https://wiki.sei.cmu.edu/confluence/display/c/STR34-C)
